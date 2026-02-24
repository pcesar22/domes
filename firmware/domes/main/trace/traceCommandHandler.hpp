#pragma once

/**
 * @file traceCommandHandler.hpp
 * @brief Handler for trace protocol commands over serial
 *
 * Processes trace commands received via the frame protocol and
 * sends responses using protobuf encoding (nanopb).
 */

#include "interfaces/iTransport.hpp"
#include "traceProtocol.hpp"

#include <cstddef>
#include <cstdint>

namespace domes::trace {

/**
 * @brief Handles trace protocol commands
 *
 * Processes incoming trace commands and generates protobuf-encoded responses.
 * Uses the same frame format as OTA and config protocols.
 */
class CommandHandler {
public:
    /**
     * @brief Construct command handler
     *
     * @param transport Transport to send responses on
     * @param podId Pod identity (from NVS, 0 if unset)
     */
    explicit CommandHandler(ITransport& transport, uint8_t podId = 0);

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
    void handleStart();
    void handleStop();
    void handleDump();
    void handleClear();
    void handleStatus();

    /**
     * @brief Send ACK response (protobuf AckResponse)
     */
    void sendAck(Status status);

    /**
     * @brief Send session info (protobuf TraceSessionInfo)
     */
    void sendSessionInfo(uint32_t eventCount, uint32_t droppedCount,
                         uint32_t startTs, uint32_t endTs);

    /**
     * @brief Send a chunk of trace events (protobuf TraceDataChunk)
     */
    void sendDataChunk(uint32_t offset, const TraceEvent* events, size_t count);

    /**
     * @brief Send end of dump marker (protobuf TraceDumpComplete)
     */
    void sendDumpComplete(uint32_t totalEvents, uint32_t checksum);

    /**
     * @brief Send status response (protobuf TraceStatusResponse)
     */
    void sendStatusResponse();

    /**
     * @brief Send a frame with given type and payload
     */
    bool sendFrame(MsgType type, const uint8_t* payload, size_t len);

    ITransport& transport_;
    uint8_t podId_;
};

}  // namespace domes::trace
