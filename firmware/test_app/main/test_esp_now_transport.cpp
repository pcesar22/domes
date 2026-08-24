#include "esp_now.h"
#include "transport/espNowTransport.hpp"
#include "transport/physicalEspNowRadio.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

namespace sim {
std::vector<domes::trace::TraceEvent>& globalTraceEvents();
}

namespace domes {
namespace {

struct PhysicalCallbackCapture {
    EspNowCorrelationToken receiveToken = 0;
    EspNowCorrelationToken sendToken = 0;
    EspNowReceiveMetadata metadata{};
    std::vector<uint8_t> payload;
};

void captureReceive(void* context, EspNowCorrelationToken token,
                    const EspNowReceiveMetadata& metadata, const uint8_t* data, size_t len) {
    auto& capture = *static_cast<PhysicalCallbackCapture*>(context);
    capture.receiveToken = token;
    capture.metadata = metadata;
    capture.payload.assign(data, data + len);
}

void captureSend(void* context, EspNowCorrelationToken token, const EspNowAddress&,
                 EspNowRadioSendStatus) {
    static_cast<PhysicalCallbackCapture*>(context)->sendToken = token;
}

TEST(PhysicalEspNowRadioTest, OwnsVendorCallbacksMetadataAndSubmissionTokens) {
    esp_now_test_stub::reset();
    PhysicalEspNowRadio radio;
    PhysicalCallbackCapture capture;
    ASSERT_EQ(radio.init(&capture, captureReceive, captureSend), EspNowRadioResult::kOk);

    EspNowAddress source = {1, 2, 3, 4, 5, 6};
    wifi_pkt_rx_ctrl_t rxControl{.rssi = -61};
    esp_now_recv_info_t info{.src_addr = source.data(), .rx_ctrl = &rxControl};
    const std::array<uint8_t, 3> payload = {9, 8, 7};
    esp_now_test_stub::receiveCallback(&info, payload.data(), payload.size());
    source.fill(0);
    EXPECT_NE(capture.receiveToken, 0u);
    EXPECT_EQ(capture.metadata.source, (EspNowAddress{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(capture.metadata.rssi, -61);
    EXPECT_EQ(capture.payload, (std::vector<uint8_t>{9, 8, 7}));

    constexpr EspNowCorrelationToken kSendToken = 91;
    ASSERT_EQ(radio.send(kEspNowBroadcastAddress, payload.data(), payload.size(), kSendToken),
              EspNowRadioResult::kOk);
    esp_now_test_stub::sendCallback(kEspNowBroadcastAddress.data(), ESP_NOW_SEND_SUCCESS);
    EXPECT_EQ(capture.sendToken, kSendToken);
    radio.deinit();
}

class FakeEspNowRadio final : public IEspNowRadio {
public:
    enum class SendBehavior { kCompleteSuccess, kCompleteFailure, kSynchronousFailure, kTimeout };

    EspNowRadioResult init(void* context, ReceiveCallback receiveCallback,
                           SendCallback sendCallback) override {
        ++initCalls;
        if (initResult != EspNowRadioResult::kOk) {
            return initResult;
        }
        context_ = context;
        receiveCallback_ = receiveCallback;
        sendCallback_ = sendCallback;
        active = true;
        return EspNowRadioResult::kOk;
    }

    void deinit() override {
        ++deinitCalls;
        active = false;
        context_ = nullptr;
        receiveCallback_ = nullptr;
        sendCallback_ = nullptr;
    }

    EspNowRadioResult addPeer(const EspNowAddress& address) override {
        if (addResult != EspNowRadioResult::kOk) {
            return addResult;
        }
        if (peerExists(address)) {
            return EspNowRadioResult::kAlreadyExists;
        }
        peers.push_back(address);
        return EspNowRadioResult::kOk;
    }

    EspNowRadioResult removePeer(const EspNowAddress& address) override {
        const auto found = std::find(peers.begin(), peers.end(), address);
        if (found == peers.end()) {
            return EspNowRadioResult::kNotFound;
        }
        peers.erase(found);
        return EspNowRadioResult::kOk;
    }

    EspNowRadioResult getPeerCounts(EspNowPeerCounts& counts) const override {
        counts.total = static_cast<uint8_t>(peers.size());
        return EspNowRadioResult::kOk;
    }

    bool peerExists(const EspNowAddress& address) const override {
        return std::find(peers.begin(), peers.end(), address) != peers.end();
    }

    EspNowRadioResult send(const EspNowAddress& destination, const uint8_t*, size_t len,
                           EspNowCorrelationToken token) override {
        sentDestination = destination;
        sentLength = len;
        sentTokens.push_back(token);
        if (sendBehavior == SendBehavior::kSynchronousFailure) {
            return EspNowRadioResult::kError;
        }
        if (emitUnownedCompletions) {
            sendCallback_(context_, token + 1, destination, EspNowRadioSendStatus::kSuccess);
        }
        if (sendBehavior != SendBehavior::kTimeout) {
            sendCallback_(context_, token, destination,
                          sendBehavior == SendBehavior::kCompleteSuccess
                              ? EspNowRadioSendStatus::kSuccess
                              : EspNowRadioSendStatus::kFailure);
        }
        if (emitUnownedCompletions) {
            sendCallback_(context_, token, destination, EspNowRadioSendStatus::kSuccess);
        }
        return EspNowRadioResult::kOk;
    }

    void emitReceive(EspNowCorrelationToken token, const EspNowReceiveMetadata& metadata,
                     const uint8_t* data, size_t len) {
        receiveCallback_(context_, token, metadata, data, len);
    }

    EspNowRadioResult initResult = EspNowRadioResult::kOk;
    EspNowRadioResult addResult = EspNowRadioResult::kOk;
    SendBehavior sendBehavior = SendBehavior::kCompleteSuccess;
    int initCalls = 0;
    int deinitCalls = 0;
    bool active = false;
    bool emitUnownedCompletions = false;
    size_t sentLength = 0;
    EspNowAddress sentDestination{};
    std::vector<EspNowCorrelationToken> sentTokens;
    std::vector<EspNowAddress> peers;

private:
    void* context_ = nullptr;
    ReceiveCallback receiveCallback_ = nullptr;
    SendCallback sendCallback_ = nullptr;
};

TEST(EspNowTransportTest, OwnsRadioLifecycleAndBroadcastPeer) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);

    EXPECT_EQ(transport.init(), TransportError::kOk);
    EXPECT_TRUE(radio.active);
    ASSERT_EQ(radio.peers.size(), 1u);
    EXPECT_EQ(radio.peers.front(), kEspNowBroadcastAddress);
    EXPECT_TRUE(transport.isConnected());

    transport.disconnect();
    EXPECT_FALSE(radio.active);
    EXPECT_FALSE(transport.isConnected());
}

TEST(EspNowTransportTest, PreservesPeerOperationsAndExcludesBroadcastFromCount) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    const EspNowAddress peer = {1, 2, 3, 4, 5, 6};

    EXPECT_EQ(transport.addPeer(peer.data()), TransportError::kOk);
    EXPECT_EQ(transport.addPeer(peer.data()), TransportError::kOk);
    EXPECT_EQ(transport.getPeerCount(), 1u);
    EXPECT_EQ(transport.removePeer(peer.data()), TransportError::kOk);
    EXPECT_EQ(transport.removePeer(peer.data()), TransportError::kOk);
    EXPECT_EQ(transport.getPeerCount(), 0u);
}

TEST(EspNowTransportTest, SeparatesSynchronousFailureAndAsynchronousCompletion) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    constexpr std::array<uint8_t, 3> payload = {7, 8, 9};

    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kSynchronousFailure;
    EXPECT_EQ(transport.send(payload.data(), payload.size()), TransportError::kIoError);
    EXPECT_TRUE(transport.isConnected());

    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kCompleteSuccess;
    EXPECT_EQ(transport.send(payload.data(), payload.size()), TransportError::kOk);
    ASSERT_EQ(radio.sentTokens.size(), 2u);
    EXPECT_NE(radio.sentTokens[0], 0u);
    EXPECT_GT(radio.sentTokens[1], radio.sentTokens[0]);

    const EspNowAddress peer = {6, 5, 4, 3, 2, 1};
    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kCompleteFailure;
    EXPECT_EQ(transport.sendTo(peer.data(), payload.data(), payload.size()),
              TransportError::kIoError);
}

TEST(EspNowTransportTest, UnknownAndDuplicateCompletionsDoNotReleaseOwnership) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    radio.emitUnownedCompletions = true;
    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kCompleteFailure;
    const uint8_t payload = 1;
    const EspNowAddress peer = {6, 5, 4, 3, 2, 1};
    EXPECT_EQ(transport.sendTo(peer.data(), &payload, 1), TransportError::kIoError);
    EXPECT_EQ(transport.getTxCount(), 0u);
    EXPECT_EQ(transport.getTxFailCount(), 1u);
}

TEST(EspNowTransportTest, CopiesReceiveMetadataPayloadAndCorrelationToken) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    const std::array<uint8_t, 4> payload = {10, 20, 30, 40};
    EspNowReceiveMetadata metadata{};
    metadata.source = {0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0};
    metadata.sourceValid = true;
    metadata.rssi = -47;
    metadata.rssiValid = true;

    radio.emitReceive(73, metadata, payload.data(), payload.size());
    metadata.source.fill(0);
    std::array<uint8_t, 8> received{};
    size_t receivedSize = received.size();
    ASSERT_EQ(transport.receive(received.data(), &receivedSize, 0), TransportError::kOk);
    EXPECT_EQ(receivedSize, payload.size());
    EXPECT_TRUE(std::equal(payload.begin(), payload.end(), received.begin()));
    EXPECT_EQ(transport.lastReceivedCorrelationToken(), 73u);
    int8_t rssi = 0;
    EXPECT_TRUE(transport.lastReceivedRssi(rssi));
    EXPECT_EQ(rssi, -47);
    EspNowAddress source{};
    EXPECT_TRUE(transport.lastReceivedSource(source.data()));
    EXPECT_EQ(source, (EspNowAddress{0x94, 0xA9, 0x90, 0x0A, 0xEB, 0xC0}));
}

TEST(EspNowTransportTest, PreservesSevenMaximumFramesAndDropsSaturationExcess) {
    static_assert(kEspNowRxBaselineMaxFrames == 7);
    static_assert(kEspNowRxMaxFrames == kEspNowRxBaselineMaxFrames);
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    std::array<uint8_t, kEspNowMaxPayload> payload{};
    EspNowReceiveMetadata metadata{};

    for (size_t index = 0; index < kEspNowRxMaxFrames + 1; ++index) {
        radio.emitReceive(static_cast<uint32_t>(index + 1), metadata, payload.data(),
                          payload.size());
    }
    EXPECT_EQ(transport.getRxCount(), kEspNowRxMaxFrames);

    for (size_t index = 0; index < kEspNowRxMaxFrames; ++index) {
        std::array<uint8_t, kEspNowMaxPayload> received{};
        size_t receivedSize = received.size();
        EXPECT_EQ(transport.receive(received.data(), &receivedSize, 0), TransportError::kOk);
        EXPECT_EQ(transport.lastReceivedCorrelationToken(), index + 1);
    }
}

TEST(EspNowTransportTest, TimeoutPoisonsSessionUntilLifecycleRecovery) {
    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    const uint8_t payload = 42;
    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kTimeout;

    EXPECT_EQ(transport.send(&payload, 1), TransportError::kTimeout);
    EXPECT_FALSE(transport.isConnected());
    EXPECT_EQ(transport.send(&payload, 1), TransportError::kDisconnected);

    transport.disconnect();
    radio.sendBehavior = FakeEspNowRadio::SendBehavior::kCompleteSuccess;
    EXPECT_EQ(transport.init(), TransportError::kOk);
    EXPECT_EQ(transport.send(&payload, 1), TransportError::kOk);
}

TEST(EspNowTransportTest, EmitsOneCorrelationTokenAtEveryCallbackToTaskBoundary) {
    trace::Recorder::shutdown();
    ASSERT_EQ(trace::Recorder::init(4096), ESP_OK);
    ASSERT_TRUE(trace::Recorder::registerTask(xTaskGetCurrentTaskHandle(), "espnow_test", 7, 1, 0));
    trace::Recorder::finalizeTaskCatalog();
    sim::globalTraceEvents().clear();

    FakeEspNowRadio radio;
    EspNowTransport transport(radio);
    ASSERT_EQ(transport.init(), TransportError::kOk);
    ASSERT_TRUE(trace::Recorder::setEnabled(true));
    constexpr EspNowCorrelationToken kRxToken = 417;
    const uint8_t payload = 3;
    EspNowReceiveMetadata metadata{};
    radio.emitReceive(kRxToken, metadata, &payload, 1);
    uint8_t received = 0;
    size_t receivedSize = 1;
    ASSERT_EQ(transport.receive(&received, &receivedSize, 0), TransportError::kOk);
    ASSERT_EQ(transport.send(&payload, 1), TransportError::kOk);
    ASSERT_FALSE(radio.sentTokens.empty());
    const EspNowCorrelationToken txToken = radio.sentTokens.back();

    const auto& events = sim::globalTraceEvents();
    struct ExpectedBoundary {
        trace::EventType type;
        uint32_t objectId;
        uint8_t context;
        uint16_t taskId;
        trace::ObjectKind objectKind;
        bool operator==(const ExpectedBoundary&) const = default;
    };
    const auto causalBoundariesForToken = [&events](EspNowCorrelationToken token) {
        std::vector<ExpectedBoundary> boundaries;
        for (const trace::TraceEvent& event : events) {
            if (event.arg2 == token && event.type() >= trace::EventType::kSemTake &&
                event.type() <= trace::EventType::kCausalComplete) {
                EXPECT_EQ(event.category(), trace::Category::kKernel)
                    << "causal trace type " << static_cast<int>(event.type())
                    << " must use the kernel category";
                trace::ObjectKind kind = trace::ObjectKind::kUnknown;
                for (const auto& object : trace::KernelTrace::objects()) {
                    if (object.valid && object.objectId == event.arg1) {
                        kind = object.kind;
                        break;
                    }
                }
                boundaries.push_back(
                    {event.type(), event.arg1, event.contextCode(), event.taskId, kind});
            }
        }
        return boundaries;
    };
    const std::array rxTypes = {
        ExpectedBoundary{trace::EventType::kCallbackBegin, kEspNowCallbackTraceId, 2, 0,
                         trace::ObjectKind::kCallback},
        ExpectedBoundary{trace::EventType::kSchedQueueSend, kEspNowQueueTraceId, 2, 0,
                         trace::ObjectKind::kQueue},
        ExpectedBoundary{trace::EventType::kSemGive, kEspNowReadyTraceId, 2, 0,
                         trace::ObjectKind::kSemaphore},
        ExpectedBoundary{trace::EventType::kCallbackEnd, kEspNowCallbackTraceId, 2, 0,
                         trace::ObjectKind::kCallback},
        ExpectedBoundary{trace::EventType::kSemTake, kEspNowReadyTraceId, 0, 7,
                         trace::ObjectKind::kSemaphore},
        ExpectedBoundary{trace::EventType::kSchedQueueReceive, kEspNowQueueTraceId, 0, 7,
                         trace::ObjectKind::kQueue},
        ExpectedBoundary{trace::EventType::kCausalComplete, kEspNowCompleteTraceId, 0, 7,
                         trace::ObjectKind::kAction},
    };
    EXPECT_EQ(causalBoundariesForToken(kRxToken),
              (std::vector<ExpectedBoundary>(rxTypes.begin(), rxTypes.end())));
    const std::array txTypes = {
        ExpectedBoundary{trace::EventType::kSchedQueueSend, kEspNowQueueTraceId, 0, 7,
                         trace::ObjectKind::kQueue},
        ExpectedBoundary{trace::EventType::kCallbackBegin, kEspNowCallbackTraceId, 2, 0,
                         trace::ObjectKind::kCallback},
        ExpectedBoundary{trace::EventType::kSemGive, kEspNowReadyTraceId, 2, 0,
                         trace::ObjectKind::kSemaphore},
        ExpectedBoundary{trace::EventType::kCallbackEnd, kEspNowCallbackTraceId, 2, 0,
                         trace::ObjectKind::kCallback},
        ExpectedBoundary{trace::EventType::kSemTake, kEspNowReadyTraceId, 0, 7,
                         trace::ObjectKind::kSemaphore},
        ExpectedBoundary{trace::EventType::kCausalComplete, kEspNowCompleteTraceId, 0, 7,
                         trace::ObjectKind::kAction},
    };
    EXPECT_EQ(causalBoundariesForToken(txToken),
              (std::vector<ExpectedBoundary>(txTypes.begin(), txTypes.end())));

    trace::Recorder::setEnabled(false);
    trace::Recorder::shutdown();
}

}  // namespace
}  // namespace domes
