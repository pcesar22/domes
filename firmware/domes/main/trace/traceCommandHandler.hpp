#pragma once

/**
 * @file traceCommandHandler.hpp
 * @brief Handler for trace protocol commands over serial
 *
 * Processes trace commands received via the frame protocol and
 * sends responses. Works with SerialOtaReceiver for command dispatch.
 */

#include "interfaces/iTransport.hpp"
#include "traceProtocol.hpp"

#include <cstddef>
#include <cstdint>

namespace domes::trace {

/**
 * @brief Handles trace protocol commands
 *
 * Processes incoming trace commands and generates responses.
 * Uses the same frame format as OTA protocol.
 */
class CommandHandler {
public:
    /**
     * @brief Construct command handler
     *
     * @param transport Transport to send responses on
     */
    explicit CommandHandler(ITransport& transport);

    /**
     * @brief Handle an incoming trace command
     *
     * @param type Message type (TraceMsgType value)
     * @param payload Command payload (may be nullptr)
     * @param len Payload length
     * @return true if command was handled successfully
     */
    bool handleCommand(uint8_t type, const uint8_t* payload, size_t len);

private:
    /**
     * @brief Handle TRACE_START command
     */
    void handleStart();

    /**
     * @brief Handle TRACE_STOP command
     */
    void handleStop();

    /**
     * @brief Handle TRACE_DUMP command
     */
    void handleDump();

    /**
     * @brief Handle TRACE_CLEAR command
     */
    void handleClear();

    /**
     * @brief Handle TRACE_STATUS command
     */
    void handleStatus();

    /**
     * @brief Send ACK response
     *
     * @param status Status code
     */
    void sendAck(TraceStatus status);

    /**
     * @brief Send trace metadata (first part of dump)
     *
     * @param eventCount Total events to send
     * @param droppedCount Events dropped
     * @param startTs First event timestamp
     * @param endTs Last event timestamp
     */
    void sendMetadata(uint32_t eventCount, uint32_t droppedCount, uint32_t startTs, uint32_t endTs);

    /**
     * @brief Send a chunk of trace events
     *
     * @param offset Event offset in dump
     * @param events Pointer to events array
     * @param count Number of events
     */
    void sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count);

    /**
     * @brief Send end of dump marker
     *
     * @param totalEvents Total events sent
     * @param checksum Simple checksum
     */
    void sendEnd(uint32_t totalEvents, uint32_t checksum);

    /**
     * @brief Send status response
     */
    void sendStatusResponse();

    /**
     * @brief Send a frame with given type and payload
     *
     * @param type Message type
     * @param payload Payload data
     * @param len Payload length
     * @return true on success
     */
    bool sendFrame(TraceMsgType type, const uint8_t* payload, size_t len);

    ITransport& transport_;
};

}  // namespace domes::trace
