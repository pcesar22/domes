/**
 * @file traceStreamServer.cpp
 * @brief TCP trace streaming server implementation
 */

#include "traceStreamServer.hpp"

#include "protocol/frameCodec.hpp"
#include "traceProtocol.hpp"
#include "traceRecorder.hpp"

#include "pb_encode.h"
#include "trace.pb.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"

#include <array>
#include <cstring>

namespace {
constexpr const char* kTag = "trace_stream";
constexpr size_t kMaxFrameSize = 512;
}

namespace domes::trace {

// Static member definitions
TraceEvent TraceStreamServer::ringBuffer_[kStreamBufferSize] = {};
std::atomic<size_t> TraceStreamServer::writeIdx_{0};
std::atomic<size_t> TraceStreamServer::readIdx_{0};
std::atomic<uint32_t> TraceStreamServer::dropped_{0};

void TraceStreamServer::run() {
    // Create listen socket
    int listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock < 0) {
        ESP_LOGE(kTag, "Failed to create socket: errno %d", errno);
        return;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kStreamPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ESP_LOGE(kTag, "Bind failed: errno %d", errno);
        close(listenSock);
        return;
    }

    if (listen(listenSock, 1) < 0) {
        ESP_LOGE(kTag, "Listen failed: errno %d", errno);
        close(listenSock);
        return;
    }

    ESP_LOGI(kTag, "Trace stream server listening on port %u", kStreamPort);

    while (!stopRequested_.load()) {
        // Set timeout on accept so we can check stopRequested
        struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(listenSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(listenSock, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

        if (clientSock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Timeout, check stop flag
            }
            ESP_LOGE(kTag, "Accept failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(kTag, "Stream client connected");
        handleClient(clientSock);
        close(clientSock);
        ESP_LOGI(kTag, "Stream client disconnected");
    }

    close(listenSock);
    ESP_LOGI(kTag, "Trace stream server stopped");
}

esp_err_t TraceStreamServer::requestStop() {
    stopRequested_.store(true);
    return ESP_OK;
}

bool TraceStreamServer::shouldRun() const {
    return !stopRequested_.load();
}

void TraceStreamServer::handleClient(int clientSock) {
    // Reset ring buffer state
    writeIdx_.store(0, std::memory_order_relaxed);
    readIdx_.store(0, std::memory_order_relaxed);
    dropped_.store(0, std::memory_order_relaxed);
    sequence_ = 0;

    // Register streaming callback
    streaming_.store(true);
    Recorder::setStreamCallback(streamCallback);

    ESP_LOGI(kTag, "Streaming started");

    // Set send timeout
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    std::array<TraceEvent, kStreamBatchSize> batch;

    while (!stopRequested_.load()) {
        // Drain events from ring buffer into batch
        size_t batchCount = 0;
        size_t rd = readIdx_.load(std::memory_order_acquire);
        size_t wr = writeIdx_.load(std::memory_order_acquire);

        while (rd != wr && batchCount < kStreamBatchSize) {
            batch[batchCount++] = ringBuffer_[rd % kStreamBufferSize];
            rd++;
        }
        readIdx_.store(rd, std::memory_order_release);

        if (batchCount == 0) {
            // No events, sleep briefly
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Build StreamBatch protobuf
        domes_trace_StreamBatch msg = domes_trace_StreamBatch_init_zero;
        msg.sequence = sequence_++;
        msg.dropped = dropped_.exchange(0, std::memory_order_relaxed);

        size_t eventBytes = batchCount * sizeof(TraceEvent);
        msg.events.size = eventBytes;
        std::memcpy(msg.events.bytes, batch.data(), eventBytes);

        // Encode protobuf
        std::array<uint8_t, 512> pbBuf;
        pb_ostream_t stream = pb_ostream_from_buffer(pbBuf.data(), pbBuf.size());
        if (!pb_encode(&stream, domes_trace_StreamBatch_fields, &msg)) {
            ESP_LOGE(kTag, "Failed to encode StreamBatch");
            continue;
        }

        // Encode as frame
        std::array<uint8_t, kMaxFrameSize> frameBuf;
        size_t frameLen = 0;
        TransportError err = encodeFrame(
            static_cast<uint8_t>(MsgType::kStreamData),
            pbBuf.data(), stream.bytes_written,
            frameBuf.data(), frameBuf.size(), &frameLen);

        if (!isOk(err)) {
            ESP_LOGE(kTag, "Failed to encode stream frame");
            continue;
        }

        // Send over TCP
        ssize_t sent = send(clientSock, frameBuf.data(), frameLen, 0);
        if (sent < 0) {
            ESP_LOGW(kTag, "Send failed (client disconnected?)");
            break;
        }
    }

    // Unregister callback
    Recorder::setStreamCallback(nullptr);
    streaming_.store(false);

    ESP_LOGI(kTag, "Streaming stopped");
}

void TraceStreamServer::streamCallback(const TraceEvent& event) {
    size_t wr = writeIdx_.load(std::memory_order_relaxed);
    size_t rd = readIdx_.load(std::memory_order_relaxed);

    // Check if buffer is full
    if (wr - rd >= kStreamBufferSize) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    ringBuffer_[wr % kStreamBufferSize] = event;
    writeIdx_.store(wr + 1, std::memory_order_release);
}

}  // namespace domes::trace
