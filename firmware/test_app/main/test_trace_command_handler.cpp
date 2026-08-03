#include "interfaces/iTransport.hpp"
#include "pb_decode.h"
#include "protocol/frameCodec.hpp"
#include "trace/traceCommandHandler.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace domes::trace {
namespace {

class CapturingTransport final : public ITransport {
public:
    TransportError init() override { return TransportError::kOk; }

    TransportError send(const uint8_t* data, size_t len) override {
        if (data == nullptr || len == 0) {
            return TransportError::kInvalidArg;
        }
        sentFrames.emplace_back(data, data + len);
        return TransportError::kOk;
    }

    TransportError receive(uint8_t*, size_t*, uint32_t) override {
        return TransportError::kTimeout;
    }

    bool isConnected() const override { return true; }
    void disconnect() override {}

    std::vector<std::vector<uint8_t>> sentFrames;
};

Status decodeAckStatus(const std::vector<uint8_t>& frame) {
    FrameDecoder decoder;
    for (uint8_t byte : frame) {
        decoder.feedByte(byte);
    }
    EXPECT_TRUE(decoder.isComplete());
    EXPECT_EQ(static_cast<uint8_t>(MsgType::kAck), decoder.getType());

    domes_trace_AckResponse response = domes_trace_AckResponse_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(decoder.getPayload(), decoder.getPayloadLen());
    EXPECT_TRUE(pb_decode(&stream, domes_trace_AckResponse_fields, &response));
    return static_cast<Status>(response.status);
}

class EmptyPayloadTraceCommandTest : public testing::TestWithParam<MsgType> {};

std::string commandName(const testing::TestParamInfo<MsgType>& info) {
    switch (info.param) {
        case MsgType::kStart:
            return "Start";
        case MsgType::kStop:
            return "Stop";
        case MsgType::kDump:
            return "Dump";
        case MsgType::kClear:
            return "Clear";
        case MsgType::kStatusReq:
            return "Status";
        default:
            return "Unknown";
    }
}

TEST_P(EmptyPayloadTraceCommandTest, RejectsNonemptyPayloadWithErrorAck) {
    CapturingTransport transport;
    CommandHandler handler(transport);
    constexpr std::array<uint8_t, 1> payload{0x01};

    EXPECT_TRUE(
        handler.handleCommand(static_cast<uint8_t>(GetParam()), payload.data(), payload.size()));
    ASSERT_EQ(1u, transport.sentFrames.size());
    EXPECT_EQ(Status::kError, decodeAckStatus(transport.sentFrames.front()));
}

INSTANTIATE_TEST_SUITE_P(ControlRequests, EmptyPayloadTraceCommandTest,
                         testing::Values(MsgType::kStart, MsgType::kStop, MsgType::kDump,
                                         MsgType::kClear, MsgType::kStatusReq),
                         commandName);

TEST(TraceCommandHandler, AcceptsEmptyControlRequestPayload) {
    CapturingTransport transport;
    CommandHandler handler(transport);

    EXPECT_TRUE(handler.handleCommand(static_cast<uint8_t>(MsgType::kStart), nullptr, 0));
    ASSERT_EQ(1u, transport.sentFrames.size());
    EXPECT_EQ(Status::kNotInit, decodeAckStatus(transport.sentFrames.front()));
}

}  // namespace
}  // namespace domes::trace
