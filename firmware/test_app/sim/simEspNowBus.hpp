#pragma once

#include "sim/simClock.hpp"
#include "sim/simLog.hpp"
#include "sim/simProtocol.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace sim {

struct FlowEvent {
    uint64_t timestampUs;
    uint16_t srcPod;
    uint16_t dstPod;
    SimMessageType type;
    uint32_t sequence;

    bool operator==(const FlowEvent&) const = default;
};

enum class DeliveryAction : uint8_t {
    kDeliver,
    kDrop,
    kDuplicate,
};

struct DeliveryContext {
    uint64_t sentAtUs;
    uint16_t srcPod;
    uint16_t dstPod;
    SimMessageType type;
    uint32_t sequence;

    bool operator==(const DeliveryContext&) const = default;
};

struct DeliveryDirective {
    DeliveryAction action = DeliveryAction::kDeliver;
    uint64_t delayUs = 0;

    bool operator==(const DeliveryDirective&) const = default;
};

struct DeliveryDecision {
    DeliveryContext context;
    DeliveryDirective directive;

    bool operator==(const DeliveryDecision&) const = default;
};

using MessageHandler = std::function<void(const SimMessage&)>;
using DeliveryPolicy = std::function<DeliveryDirective(const DeliveryContext&)>;

class SimEspNowBus {
public:
    SimEspNowBus(SimLog& log, SimClock& clock) : log_(log), clock_(clock) {}

    void registerPod(uint16_t podId, MessageHandler handler) {
        handlers_[podId] = std::move(handler);
    }

    void setDeliveryPolicy(DeliveryPolicy policy) {
        policy_ = std::move(policy);
        replayRecord_.clear();
        replayCursor_ = 0;
        replayMismatch_ = false;
    }

    void setReplayRecord(std::vector<DeliveryDecision> record) {
        policy_ = {};
        replayRecord_ = std::move(record);
        replayCursor_ = 0;
        replayMismatch_ = false;
    }

    void send(SimMessage msg) {
        auto& hdr = getMutableHeader(msg);
        hdr.timestampUs = clock_.nowUs();
        hdr.sequence = nextSequence_++;

        std::ostringstream oss;
        oss << "espnow.send " << messageTypeName(hdr.type) << " pod" << hdr.srcPodId << "->";
        if (hdr.dstPodId == kBroadcastPodId)
            oss << "ALL";
        else
            oss << "pod" << hdr.dstPodId;
        log_.log(hdr.srcPodId, "espnow", oss.str());

        pending_.push_back(std::move(msg));
    }

    void deliverPending() {
        resolvePendingTransmissions();

        std::stable_sort(scheduled_.begin(), scheduled_.end(),
                         [](const ScheduledDelivery& lhs, const ScheduledDelivery& rhs) {
                             if (lhs.deliverAtUs != rhs.deliverAtUs)
                                 return lhs.deliverAtUs < rhs.deliverAtUs;
                             return lhs.order < rhs.order;
                         });

        std::vector<ScheduledDelivery> waiting;
        waiting.reserve(scheduled_.size());
        for (auto& delivery : scheduled_) {
            if (delivery.deliverAtUs > clock_.nowUs()) {
                waiting.push_back(std::move(delivery));
                continue;
            }

            auto handler = handlers_.find(delivery.dstPod);
            if (handler == handlers_.end())
                continue;

            const auto& header = getHeader(delivery.message);
            flowEvents_.push_back(
                {clock_.nowUs(), header.srcPodId, delivery.dstPod, header.type, header.sequence});
            handler->second(delivery.message);
        }
        scheduled_ = std::move(waiting);
    }

    const std::vector<FlowEvent>& flowEvents() const { return flowEvents_; }
    const std::vector<DeliveryDecision>& deliveryRecord() const { return deliveryRecord_; }

    bool replayComplete() const {
        return !replayRecord_.empty() && !replayMismatch_ && replayCursor_ == replayRecord_.size();
    }

    bool replayMismatch() const { return replayMismatch_; }

    void clearFlowEvents() { flowEvents_.clear(); }
    void clearDeliveryRecord() { deliveryRecord_.clear(); }
    size_t pendingCount() const { return pending_.size() + scheduled_.size(); }

private:
    struct ScheduledDelivery {
        uint64_t deliverAtUs;
        uint64_t order;
        uint16_t dstPod;
        SimMessage message;
    };

    void resolvePendingTransmissions() {
        auto messages = std::move(pending_);
        pending_.clear();

        for (const auto& message : messages) {
            const auto& header = getHeader(message);
            if (header.dstPodId == kBroadcastPodId) {
                for (const auto& [podId, handler] : handlers_) {
                    (void)handler;
                    if (podId != header.srcPodId)
                        resolveDelivery(message, podId);
                }
            } else if (handlers_.contains(header.dstPodId)) {
                resolveDelivery(message, header.dstPodId);
            }
        }
    }

    void resolveDelivery(const SimMessage& message, uint16_t dstPod) {
        const auto& header = getHeader(message);
        DeliveryContext context{header.timestampUs, header.srcPodId, dstPod, header.type,
                                header.sequence};
        DeliveryDirective directive = chooseDirective(context);
        deliveryRecord_.push_back({context, directive});

        if (directive.action == DeliveryAction::kDrop)
            return;

        schedule(message, dstPod, directive.delayUs);
        if (directive.action == DeliveryAction::kDuplicate)
            schedule(message, dstPod, directive.delayUs);
    }

    DeliveryDirective chooseDirective(const DeliveryContext& context) {
        if (!replayRecord_.empty()) {
            if (replayCursor_ >= replayRecord_.size()) {
                replayMismatch_ = true;
                return {.action = DeliveryAction::kDrop};
            }

            const auto& expected = replayRecord_[replayCursor_++];
            if (expected.context.sentAtUs != context.sentAtUs ||
                expected.context.srcPod != context.srcPod ||
                expected.context.dstPod != context.dstPod ||
                expected.context.type != context.type ||
                expected.context.sequence != context.sequence) {
                replayMismatch_ = true;
            }
            return expected.directive;
        }

        return policy_ ? policy_(context) : DeliveryDirective{};
    }

    void schedule(const SimMessage& message, uint16_t dstPod, uint64_t delayUs) {
        scheduled_.push_back({clock_.nowUs() + delayUs, nextDeliveryOrder_++, dstPod, message});
    }

    SimLog& log_;
    SimClock& clock_;
    std::map<uint16_t, MessageHandler> handlers_;
    std::vector<SimMessage> pending_;
    std::vector<ScheduledDelivery> scheduled_;
    std::vector<FlowEvent> flowEvents_;
    std::vector<DeliveryDecision> deliveryRecord_;
    DeliveryPolicy policy_;
    std::vector<DeliveryDecision> replayRecord_;
    size_t replayCursor_ = 0;
    bool replayMismatch_ = false;
    uint32_t nextSequence_ = 0;
    uint64_t nextDeliveryOrder_ = 0;
};

}  // namespace sim
