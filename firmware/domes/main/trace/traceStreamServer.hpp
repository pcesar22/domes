#pragma once

/**
 * @file traceStreamServer.hpp
 * @brief TCP server for real-time trace event streaming
 *
 * Listens on port 5001 and streams trace events to a connected client
 * in real-time using the StreamBatch protobuf message format.
 */

#include "interfaces/iTaskRunner.hpp"
#include "traceEvent.hpp"

#include <atomic>
#include <cstdint>

namespace domes::trace {

/// TCP port for trace streaming
constexpr uint16_t kStreamPort = 5001;

/// Max events in the stream ring buffer
constexpr size_t kStreamBufferSize = 256;

/// Max events per StreamBatch message
constexpr size_t kStreamBatchSize = 16;

/**
 * @brief TCP server for live trace streaming
 *
 * Accepts a single client connection. When connected, registers
 * a callback with the Recorder to receive events. Events are
 * buffered in a lock-free ring buffer and sent in batches over TCP.
 *
 * Usage:
 * @code
 * TraceStreamServer server;
 * xTaskCreate(streamTask, "trace_stream", 4096, &server, 3, nullptr);
 * @endcode
 */
class TraceStreamServer : public ITaskRunner {
public:
    TraceStreamServer() = default;
    ~TraceStreamServer() override = default;

    TraceStreamServer(const TraceStreamServer&) = delete;
    TraceStreamServer& operator=(const TraceStreamServer&) = delete;

    // ITaskRunner implementation
    void run() override;
    esp_err_t requestStop() override;
    bool shouldRun() const override;

    /**
     * @brief Check if a client is connected and receiving streams
     */
    bool isStreaming() const { return streaming_.load(); }

private:
    /**
     * @brief Handle a connected client — send batched events until disconnect
     */
    void handleClient(int clientSock);

    /**
     * @brief Static callback registered with Recorder
     *
     * Called from any task context — must be fast and non-blocking.
     * Writes events into the lock-free ring buffer.
     */
    static void streamCallback(const TraceEvent& event);

    // Lock-free MPSC ring buffer (multiple producers via record callback, single consumer = send task)
    static TraceEvent ringBuffer_[kStreamBufferSize];
    static std::atomic<size_t> writeIdx_;
    static std::atomic<size_t> readIdx_;
    static std::atomic<uint32_t> dropped_;

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> streaming_{false};
    uint32_t sequence_{0};
};

}  // namespace domes::trace
