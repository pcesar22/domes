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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>

namespace {
constexpr const char* kTag = "trace_cmd";
std::atomic<uint32_t> gTraceCommandBusy{0};

class TraceCommandLock {
public:
    TraceCommandLock() {
        uint32_t expected = 0;
        acquired_ = gTraceCommandBusy.compare_exchange_strong(
            expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }

    ~TraceCommandLock() {
        if (acquired_) {
            gTraceCommandBusy.store(0, std::memory_order_release);
        }
    }

    bool acquired() const { return acquired_; }

private:
    bool acquired_ = false;
};
}  // namespace

namespace domes::trace {

CommandHandler::CommandHandler(ITransport& transport, uint8_t podId)
    : transport_(transport), podId_(podId) {}

bool CommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    (void)payload;

    TraceCommandLock commandLock;
    if (!commandLock.acquired()) {
        ESP_LOGW(kTag, "Trace command rejected while another command is active");
        sendAck(Status::kError);
        return true;
    }

    auto msgType = static_cast<MsgType>(type);

    switch (msgType) {
        case MsgType::kStart:
        case MsgType::kStop:
        case MsgType::kDump:
        case MsgType::kClear:
        case MsgType::kStatusReq:
            if (len != 0) {
                ESP_LOGW(kTag, "Trace command 0x%02X requires an empty payload", type);
                sendAck(Status::kError);
                return true;
            }
            break;

        default:
            break;
    }

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

    TraceBuffer& buffer = Recorder::buffer();
    if (!buffer.tryClaimDumpSnapshot(this)) {
        ESP_LOGW(kTag, "Trace dump snapshot is owned by another transport");
        sendAck(Status::kError);
        return;
    }

    // Pause recording during dump
    bool wasEnabled = Recorder::isEnabled();
    Recorder::setEnabled(false);
    buffer.pause();

    uint32_t eventCount = static_cast<uint32_t>(buffer.captureDumpSnapshot(this));
    uint32_t droppedCount = buffer.droppedCount();

    if (eventCount == 0) {
        ESP_LOGI(kTag, "No events to dump");
        sendAck(Status::kBufferEmpty);
        buffer.resume();
        if (wasEnabled) {
            Recorder::setEnabled(true);
        }
        buffer.completeDumpSnapshot(this);
        return;
    }

    ESP_LOGI(kTag, "Dumping ~%lu events", static_cast<unsigned long>(eventCount));

    const uint32_t startTs = buffer.dumpSnapshotEvent(this, 0)->timestamp;
    const uint32_t endTs = buffer.dumpSnapshotEvent(this, eventCount - 1)->timestamp;

    // Send session info (protobuf-encoded metadata)
    bool delivered = sendSessionInfo(eventCount, droppedCount, startTs, endTs);

    // Stream the acquired snapshot in chunks. Buffer ownership is retained
    // until the final marker is delivered, so a transport failure is retryable.
    std::array<TraceEvent, kEventsPerChunk> chunk;
    uint32_t offset = 0;
    uint32_t checksum = 0;
    while (delivered && offset < eventCount) {
        const size_t chunkFill =
            std::min(kEventsPerChunk, static_cast<size_t>(eventCount - offset));
        for (size_t i = 0; i < chunkFill; ++i) {
            const TraceEvent* event = buffer.dumpSnapshotEvent(this, offset + i);
            if (event == nullptr) {
                delivered = false;
                break;
            }
            chunk[i] = *event;
            const auto* bytes = reinterpret_cast<const uint8_t*>(&chunk[i]);
            for (size_t j = 0; j < sizeof(TraceEvent); ++j) {
                checksum += bytes[j];
            }
        }

        if (delivered) {
            delivered = sendDataChunk(offset, chunk.data(), chunkFill);
        }
        if (delivered) {
            vTaskDelay(pdMS_TO_TICKS(10));
            offset += static_cast<uint32_t>(chunkFill);
        }
    }

    if (delivered) {
        delivered = sendDumpComplete(eventCount, checksum);
    }

    if (delivered) {
        buffer.resetDroppedCount();
        ESP_LOGI(kTag, "Dump complete: %lu events, checksum 0x%08lX",
                 static_cast<unsigned long>(eventCount), static_cast<unsigned long>(checksum));
    } else {
        ESP_LOGW(kTag, "Dump delivery failed; retaining %lu events for retry",
                 static_cast<unsigned long>(eventCount));
    }

    buffer.resume();

    // Re-enable if it was enabled before
    if (wasEnabled) {
        Recorder::setEnabled(true);
    }
    if (delivered) {
        buffer.completeDumpSnapshot(this);
    }
}

void CommandHandler::handleClear() {
    ESP_LOGI(kTag, "Received TRACE_CLEAR");

    if (!Recorder::isInitialized()) {
        sendAck(Status::kNotInit);
        return;
    }

    TraceBuffer& buffer = Recorder::buffer();
    if (!buffer.tryClaimDumpSnapshot(this)) {
        ESP_LOGW(kTag, "Cannot clear trace snapshot owned by another transport");
        sendAck(Status::kError);
        return;
    }

    const bool wasEnabled = Recorder::isEnabled();
    Recorder::setEnabled(false);
    if (!buffer.clearDumpSnapshot(this)) {
        sendAck(Status::kError);
        if (wasEnabled) {
            Recorder::setEnabled(true);
        }
        return;
    }
    if (wasEnabled) {
        Recorder::setEnabled(true);
    }
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

bool CommandHandler::sendSessionInfo(uint32_t eventCount, uint32_t droppedCount, uint32_t startTs,
                                     uint32_t endTs) {
    domes_trace_TraceSessionInfo msg = domes_trace_TraceSessionInfo_init_zero;
    msg.pod_id = podId_;
    msg.event_count = eventCount;
    msg.dropped_count = droppedCount;
    msg.start_timestamp_us = startTs;
    msg.end_timestamp_us = endTs;
    msg.buffer_size_bytes = TraceBuffer::kDefaultBufferSize;
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
        return false;
    }

    return sendFrame(MsgType::kSessionInfo, buf.data(), stream.bytes_written);
}

bool CommandHandler::sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count) {
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
        return false;
    }

    return sendFrame(MsgType::kData, buf.data(), stream.bytes_written);
}

bool CommandHandler::sendDumpComplete(uint32_t totalEvents, uint32_t checksum) {
    domes_trace_TraceDumpComplete msg = domes_trace_TraceDumpComplete_init_zero;
    msg.total_events = totalEvents;
    msg.checksum = checksum;

    std::array<uint8_t, 16> buf;
    pb_ostream_t stream = pb_ostream_from_buffer(buf.data(), buf.size());
    if (!pb_encode(&stream, domes_trace_TraceDumpComplete_fields, &msg)) {
        ESP_LOGE(kTag, "Failed to encode TraceDumpComplete");
        return false;
    }

    return sendFrame(MsgType::kEnd, buf.data(), stream.bytes_written);
}

void CommandHandler::sendStatusResponse() {
    domes_trace_TraceStatusResponse msg = domes_trace_TraceStatusResponse_init_zero;
    msg.initialized = Recorder::isInitialized();
    msg.enabled = Recorder::isEnabled();
    msg.streaming = Recorder::isStreaming();
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
