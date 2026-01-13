/**
 * @file traceCommandHandler.cpp
 * @brief Trace command handler implementation
 */

#include "traceCommandHandler.hpp"

#include "esp_log.h"
#include "protocol/frameCodec.hpp"
#include "traceRecorder.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace {
constexpr const char* kTag = "trace_cmd";
}

namespace domes::trace {

CommandHandler::CommandHandler(ITransport& transport) : transport_(transport) {}

bool CommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    (void)payload;  // Most commands don't use payload
    (void)len;

    auto msgType = static_cast<MsgType>(type);

    switch (msgType) {
        case MsgType::kStart:
            handleStart();
            return true;

        case MsgType::kStop:
            handleStop();
            return true;

        case MsgType::kDump:
            handleDump();
            return true;

        case MsgType::kClear:
            handleClear();
            return true;

        case MsgType::kStatus:
            handleStatus();
            return true;

        default:
            ESP_LOGW(kTag, "Unknown trace command: 0x%02X", type);
            return false;
    }
}

void CommandHandler::handleStart() {
    ESP_LOGI(kTag, "Received TRACE_START");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    if (Recorder::isEnabled()) {
        sendAck(Status::kAlreadyOn);
        return;
    }

    Recorder::setEnabled(true);
    sendAck(Status::kOk);
}

void CommandHandler::handleStop() {
    ESP_LOGI(kTag, "Received TRACE_STOP");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    if (!Recorder::isEnabled()) {
        sendAck(Status::kAlreadyOff);
        return;
    }

    Recorder::setEnabled(false);
    sendAck(Status::kOk);
}

void CommandHandler::handleDump() {
    ESP_LOGI(kTag, "Received TRACE_DUMP");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    // Pause recording during dump
    bool wasEnabled = Recorder::isEnabled();
    Recorder::setEnabled(false);
    Recorder::buffer().pause();

    // Collect all events into a temporary buffer
    std::vector<TraceEvent> events;
    events.reserve(TraceBuffer::kMaxEvents);

    TraceEvent event;
    while (Recorder::buffer().read(&event, 0)) {
        events.push_back(event);
    }

    if (events.empty()) {
        ESP_LOGI(kTag, "No events to dump");
        sendAck(Status::kBufferEmpty);
        Recorder::buffer().resume();
        if (wasEnabled) {
            Recorder::setEnabled(true);
        }
        return;
    }

    ESP_LOGI(kTag, "Dumping %zu events", events.size());

    // Get timestamps
    uint32_t startTs = events.front().timestamp;
    uint32_t endTs = events.back().timestamp;
    uint32_t droppedCount = Recorder::buffer().droppedCount();

    // Send metadata
    sendMetadata(static_cast<uint32_t>(events.size()), droppedCount, startTs, endTs);

    // Send events in chunks
    uint32_t offset = 0;
    uint32_t checksum = 0;

    while (offset < events.size()) {
        size_t remaining = events.size() - offset;
        size_t chunkSize = std::min(remaining, kEventsPerChunk);

        sendDataChunk(offset, &events[offset], chunkSize);

        // Give USB CDC time to transmit (prevent buffer overflow)
        // Smaller chunks (128 bytes) need less delay
        vTaskDelay(pdMS_TO_TICKS(20));

        // Update checksum (simple sum of all bytes)
        for (size_t i = 0; i < chunkSize; ++i) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(&events[offset + i]);
            for (size_t j = 0; j < sizeof(TraceEvent); ++j) {
                checksum += bytes[j];
            }
        }

        offset += static_cast<uint32_t>(chunkSize);
    }

    // Send end marker
    sendEnd(static_cast<uint32_t>(events.size()), checksum);

    ESP_LOGI(kTag, "Dump complete: %zu events, checksum 0x%08lX", events.size(),
             static_cast<unsigned long>(checksum));

    // Clear buffer and reset dropped count
    Recorder::buffer().resetDroppedCount();
    Recorder::buffer().resume();

    // Re-enable if it was enabled before
    if (wasEnabled) {
        Recorder::setEnabled(true);
    }
}

void CommandHandler::handleClear() {
    ESP_LOGI(kTag, "Received TRACE_CLEAR");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    Recorder::buffer().clear();
    sendAck(Status::kOk);
}

void CommandHandler::handleStatus() {
    ESP_LOGD(kTag, "Received TRACE_STATUS");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    sendStatusResponse();
}

void CommandHandler::sendAck(Status status) {
    TraceAckResponse ack;
    ack.status = static_cast<uint8_t>(status);

    sendFrame(MsgType::kAck, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
}

void CommandHandler::sendMetadata(uint32_t eventCount, uint32_t droppedCount, uint32_t startTs,
                                  uint32_t endTs) {
    // Calculate total size: metadata + task entries
    const auto& taskNames = Recorder::getTaskNames();
    size_t validTaskCount = 0;
    for (const auto& entry : taskNames) {
        if (entry.valid) {
            validTaskCount++;
        }
    }

    size_t payloadSize = sizeof(TraceMetadata) + validTaskCount * sizeof(TraceTaskEntry);

    std::vector<uint8_t> payload(payloadSize);

    // Fill metadata
    auto* meta = reinterpret_cast<TraceMetadata*>(payload.data());
    meta->eventCount = eventCount;
    meta->droppedCount = droppedCount;
    meta->startTimestamp = startTs;
    meta->endTimestamp = endTs;
    meta->taskCount = static_cast<uint8_t>(validTaskCount);

    // Fill task entries
    auto* taskEntry = reinterpret_cast<TraceTaskEntry*>(payload.data() + sizeof(TraceMetadata));

    for (const auto& entry : taskNames) {
        if (entry.valid) {
            taskEntry->taskId = entry.taskId;
            std::strncpy(taskEntry->name, entry.name, kMaxTaskNameLength);
            taskEntry->name[kMaxTaskNameLength - 1] = '\0';
            taskEntry++;
        }
    }

    sendFrame(MsgType::kData, payload.data(), payload.size());
}

void CommandHandler::sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count) {
    size_t payloadSize = sizeof(TraceDataHeader) + count * sizeof(TraceEvent);
    std::vector<uint8_t> payload(payloadSize);

    // Fill header
    auto* header = reinterpret_cast<TraceDataHeader*>(payload.data());
    header->offset = offset;
    header->count = static_cast<uint16_t>(count);

    // Copy events
    std::memcpy(payload.data() + sizeof(TraceDataHeader), events, count * sizeof(TraceEvent));

    sendFrame(MsgType::kData, payload.data(), payload.size());
}

void CommandHandler::sendEnd(uint32_t totalEvents, uint32_t checksum) {
    TraceDumpEnd endMsg;
    endMsg.totalEvents = totalEvents;
    endMsg.checksum = checksum;

    sendFrame(MsgType::kEnd, reinterpret_cast<const uint8_t*>(&endMsg), sizeof(endMsg));
}

void CommandHandler::sendStatusResponse() {
    TraceStatusResponse status;
    status.initialized = Recorder::isInitialized() ? 1 : 0;
    status.enabled = Recorder::isEnabled() ? 1 : 0;
    status.eventCount = static_cast<uint32_t>(Recorder::buffer().count());
    status.droppedCount = Recorder::buffer().droppedCount();
    status.bufferSize = TraceBuffer::kDefaultBufferSize;

    sendFrame(MsgType::kStatus, reinterpret_cast<const uint8_t*>(&status), sizeof(status));
}

bool CommandHandler::sendFrame(MsgType type, const uint8_t* payload, size_t len) {
    std::array<uint8_t, kMaxFrameSize> frameBuf;
    size_t frameLen = 0;

    TransportError err = encodeFrame(static_cast<uint8_t>(type), payload, len, frameBuf.data(),
                                     frameBuf.size(), &frameLen);

    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to encode frame");
        return false;
    }

    err = transport_.send(frameBuf.data(), frameLen);
    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to send frame: %s", transportErrorToString(err));
        return false;
    }

    return true;
}

}  // namespace domes::trace
