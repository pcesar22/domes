/**
 * @file traceCommandHandler.cpp
 * @brief Trace command handler implementation
 *
 * All response payloads are protobuf-encoded using nanopb.
 * TraceEvent data is carried as raw binary inside protobuf 'bytes' fields.
 */

#include "traceCommandHandler.hpp"

#include "esp_log.h"
#include "pb_encode.h"
#include "protocol/frameCodec.hpp"
#include "traceRecorder.hpp"

#include <array>
#include <cstring>

namespace {
constexpr const char* kTag = "trace_cmd";
}

namespace domes::trace {

CommandHandler::CommandHandler(ITransport& transport, uint8_t podId)
    : transport_(transport), podId_(podId) {}

bool CommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    (void)payload;
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

        case MsgType::kStatusReq:
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

    uint32_t eventCount = static_cast<uint32_t>(Recorder::buffer().count());
    uint32_t droppedCount = Recorder::buffer().droppedCount();

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
    uint32_t startTs = 0;
    TraceEvent firstEvent;
    if (Recorder::buffer().read(&firstEvent, 0)) {
        startTs = firstEvent.timestamp;
    }

    // Suppress ALL logging during binary data transfer to prevent
    // ESP_LOG text from corrupting the frame protocol on the shared serial port
    esp_log_level_set("*", ESP_LOG_NONE);

    // Send session info (protobuf-encoded metadata)
    sendSessionInfo(eventCount, droppedCount, startTs, 0);

    // Stream events directly from ring buffer in chunks
    std::array<TraceEvent, kEventsPerChunk> chunk;
    uint32_t offset = 0;
    uint32_t checksum = 0;
    uint32_t totalSent = 0;
    uint32_t endTs = startTs;

    // First chunk starts with the already-read event
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
            vTaskDelay(pdMS_TO_TICKS(10));
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

    // Send end marker with actual total
    sendDumpComplete(totalSent, checksum);

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

// ============================================================================
// Protobuf-encoded response senders
// ============================================================================

void CommandHandler::sendAck(Status status) {
    domes_trace_AckResponse msg = domes_trace_AckResponse_init_zero;
    msg.status = static_cast<domes_trace_Status>(status);

    std::array<uint8_t, 16> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_AckResponse_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode AckResponse");
        return;
    }

    sendFrame(MsgType::kAck, buf.data(), stream.bytes_written);
}

void CommandHandler::sendSessionInfo(uint32_t eventCount, uint32_t droppedCount,
                                     uint32_t startTs, uint32_t endTs) {
    domes_trace_TraceSessionInfo msg = domes_trace_TraceSessionInfo_init_zero;
    msg.pod_id = podId_;
    msg.event_count = eventCount;
    msg.dropped_count = droppedCount;
    msg.start_timestamp_us = startTs;
    msg.end_timestamp_us = endTs;
    msg.buffer_size_bytes = TraceBuffer::kDefaultBufferSize;
    msg.clock_offset_us = 0;  // TODO: populate from ESP-NOW sync

    // Fill task entries
    const auto& taskNames = Recorder::getTaskNames();
    size_t taskIdx = 0;
    for (const auto& entry : taskNames) {
        if (entry.valid && taskIdx < sizeof(msg.tasks) / sizeof(msg.tasks[0])) {
            msg.tasks[taskIdx].task_id = entry.taskId;
            std::strncpy(msg.tasks[taskIdx].name, entry.name, sizeof(msg.tasks[taskIdx].name) - 1);
            msg.tasks[taskIdx].name[sizeof(msg.tasks[taskIdx].name) - 1] = '\0';
            taskIdx++;
        }
    }
    msg.tasks_count = taskIdx;

    std::array<uint8_t, kMaxProtobufPayload> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_TraceSessionInfo_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode TraceSessionInfo: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kSessionInfo, buf.data(), stream.bytes_written);
}

void CommandHandler::sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count) {
    domes_trace_TraceDataChunk msg = domes_trace_TraceDataChunk_init_zero;
    msg.offset = offset;
    msg.count = static_cast<uint32_t>(count);

    // Copy raw binary events into the bytes field
    size_t eventBytes = count * sizeof(TraceEvent);
    msg.events.size = eventBytes;
    std::memcpy(msg.events.bytes, events, eventBytes);

    std::array<uint8_t, kMaxTraceDataPayload> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_TraceDataChunk_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode TraceDataChunk: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kData, buf.data(), stream.bytes_written);
}

void CommandHandler::sendDumpComplete(uint32_t totalEvents, uint32_t checksum) {
    domes_trace_TraceDumpComplete msg = domes_trace_TraceDumpComplete_init_zero;
    msg.total_events = totalEvents;
    msg.checksum = checksum;

    std::array<uint8_t, 16> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_TraceDumpComplete_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode TraceDumpComplete");
        return;
    }

    sendFrame(MsgType::kEnd, buf.data(), stream.bytes_written);
}

void CommandHandler::sendStatusResponse() {
    domes_trace_TraceStatusResponse msg = domes_trace_TraceStatusResponse_init_zero;
    msg.initialized = Recorder::isInitialized();
    msg.enabled = Recorder::isEnabled();
    msg.streaming = false;  // TODO: populate when streaming is implemented
    msg.event_count = static_cast<uint32_t>(Recorder::buffer().count());
    msg.dropped_count = Recorder::buffer().droppedCount();
    msg.buffer_size = TraceBuffer::kDefaultBufferSize;
    msg.stream_category_mask = 0;

    std::array<uint8_t, 32> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_TraceStatusResponse_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode TraceStatusResponse");
        return;
    }

    sendFrame(MsgType::kStatusResp, buf.data(), stream.bytes_written);
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
