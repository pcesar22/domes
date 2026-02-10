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

    // Count events and get timestamps by doing a first pass
    // Read events directly from ring buffer in small chunks (no large allocation)
    uint32_t eventCount = 0;
    uint32_t startTs = 0;
    uint32_t endTs = 0;
    uint32_t droppedCount = Recorder::buffer().droppedCount();

    // First: count events and get first/last timestamps
    // We read into a small stack buffer and count
    eventCount = static_cast<uint32_t>(Recorder::buffer().count());

    if (eventCount == 0) {
        ESP_LOGI(kTag, "No events to dump");
        sendAck(Status::kBufferEmpty);
        Recorder::buffer().resume();
        if (wasEnabled) {
            Recorder::setEnabled(true);
        }
        return;
    }

    ESP_LOGI(kTag, "Dumping ~%lu events", static_cast<unsigned long>(eventCount));

    // Peek first event for start timestamp
    TraceEvent firstEvent;
    if (Recorder::buffer().read(&firstEvent, 0)) {
        startTs = firstEvent.timestamp;
    }

    // Suppress ALL logging during binary data transfer to prevent
    // ESP_LOG text from corrupting the frame protocol on the shared serial port
    esp_log_level_set("*", ESP_LOG_NONE);

    // Send metadata (eventCount is approximate — we send actual count in END)
    sendMetadata(eventCount, droppedCount, startTs, 0);

    // Stream events directly from ring buffer in chunks
    std::array<TraceEvent, kEventsPerChunk> chunk;
    uint32_t offset = 0;
    uint32_t checksum = 0;
    uint32_t totalSent = 0;

    // First chunk already has one event read (firstEvent)
    chunk[0] = firstEvent;
    size_t chunkFill = 1;

    // Update checksum for first event
    {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&firstEvent);
        for (size_t j = 0; j < sizeof(TraceEvent); ++j) {
            checksum += bytes[j];
        }
    }

    TraceEvent event;
    while (Recorder::buffer().read(&event, 0)) {
        chunk[chunkFill] = event;
        endTs = event.timestamp;

        // Update checksum
        const auto* bytes = reinterpret_cast<const uint8_t*>(&event);
        for (size_t j = 0; j < sizeof(TraceEvent); ++j) {
            checksum += bytes[j];
        }

        chunkFill++;

        if (chunkFill >= kEventsPerChunk) {
            sendDataChunk(offset, chunk.data(), chunkFill);
            vTaskDelay(pdMS_TO_TICKS(20));
            offset += static_cast<uint32_t>(chunkFill);
            totalSent += static_cast<uint32_t>(chunkFill);
            chunkFill = 0;
        }
    }

    // Send remaining events in partial chunk
    if (chunkFill > 0) {
        sendDataChunk(offset, chunk.data(), chunkFill);
        totalSent += static_cast<uint32_t>(chunkFill);
    }

    // Use last event timestamp if we didn't get any after the first
    if (endTs == 0) {
        endTs = startTs;
    }

    // Send end marker with actual total
    sendEnd(totalSent, checksum);

    // Restore logging
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(kTag, "Dump complete: %lu events, checksum 0x%08lX",
             static_cast<unsigned long>(totalSent), static_cast<unsigned long>(checksum));

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
    // Max payload: 17 + 16*18 = 305 bytes — fits in kMaxFrameSize
    const auto& taskNames = Recorder::getTaskNames();
    size_t validTaskCount = 0;
    for (const auto& entry : taskNames) {
        if (entry.valid) {
            validTaskCount++;
        }
    }

    // Stack-allocated payload buffer (max ~305 bytes)
    constexpr size_t kMaxMetadataPayload = sizeof(TraceMetadata) + 16 * sizeof(TraceTaskEntry);
    std::array<uint8_t, kMaxMetadataPayload> payload = {};
    size_t payloadSize = sizeof(TraceMetadata) + validTaskCount * sizeof(TraceTaskEntry);

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
        if (entry.valid && validTaskCount > 0) {
            taskEntry->taskId = entry.taskId;
            std::strncpy(taskEntry->name, entry.name, kMaxTaskNameLength);
            taskEntry->name[kMaxTaskNameLength - 1] = '\0';
            taskEntry++;
        }
    }

    sendFrame(MsgType::kData, payload.data(), payloadSize);
}

void CommandHandler::sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count) {
    // Stack-allocated payload: header(6) + 8*16 = 134 bytes max
    std::array<uint8_t, sizeof(TraceDataHeader) + kEventsPerChunk * sizeof(TraceEvent)> payload = {};
    size_t payloadSize = sizeof(TraceDataHeader) + count * sizeof(TraceEvent);

    // Fill header
    auto* header = reinterpret_cast<TraceDataHeader*>(payload.data());
    header->offset = offset;
    header->count = static_cast<uint16_t>(count);

    // Copy events
    std::memcpy(payload.data() + sizeof(TraceDataHeader), events, count * sizeof(TraceEvent));

    sendFrame(MsgType::kData, payload.data(), payloadSize);
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
