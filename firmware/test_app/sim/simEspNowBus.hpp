#pragma once

#include "sim/simClock.hpp"
#include "sim/simLog.hpp"
#include "sim/simProtocol.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace sim {

struct FlowEvent {
    uint64_t timestampUs;
    uint16_t srcPod;
    uint16_t dstPod;
    pb_size_t payloadTag;
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
    uint16_t addressedPod;
    uint16_t dstPod;
    pb_size_t payloadTag;
    uint32_t sequence;
    std::vector<uint8_t> legacyBytes;

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
    class ScopedHandlerOverride {
    public:
        ScopedHandlerOverride(const ScopedHandlerOverride&) = delete;
        ScopedHandlerOverride& operator=(const ScopedHandlerOverride&) = delete;
        ScopedHandlerOverride(ScopedHandlerOverride&&) = delete;
        ScopedHandlerOverride& operator=(ScopedHandlerOverride&&) = delete;

        ~ScopedHandlerOverride() {
            if (bus_ != nullptr) {
                bus_->handlers_[podId_] = std::move(previous_);
            }
        }

    private:
        friend class SimEspNowBus;

        ScopedHandlerOverride(SimEspNowBus& bus, uint16_t podId, MessageHandler handler)
            : podId_(podId) {
            const auto existing = bus.handlers_.find(podId);
            if (existing == bus.handlers_.end()) {
                bus.codecFailure_ = domes::peer_drill::CodecError::kBadRole;
                return;
            }
            previous_ = std::move(existing->second);
            existing->second = std::move(handler);
            bus_ = &bus;
        }

        SimEspNowBus* bus_ = nullptr;
        uint16_t podId_ = 0;
        MessageHandler previous_;
    };

    SimEspNowBus(SimLog& log, SimClock& clock) : log_(log), clock_(clock) {}

    void registerPod(uint16_t podId, domes_peer_drill_PeerRole role, MessageHandler handler) {
        if (role != kMasterPeerRole && role != kSlavePeerRole) {
            codecFailure_ = domes::peer_drill::CodecError::kBadRole;
            return;
        }
        const auto existing = roles_.find(podId);
        if (existing != roles_.end() && existing->second != role) {
            codecFailure_ = domes::peer_drill::CodecError::kBadRole;
            return;
        }
        roles_[podId] = role;
        handlers_[podId] = std::move(handler);
    }

    /** Temporarily replace a registered pod's receive handler and restore it at scope exit. */
    [[nodiscard]] ScopedHandlerOverride overridePodHandler(uint16_t podId, MessageHandler handler) {
        return ScopedHandlerOverride(*this, podId, std::move(handler));
    }

    void setDeliveryPolicy(DeliveryPolicy policy) {
        policy_ = std::move(policy);
        replayMode_ = false;
        replayRecord_.clear();
        replayCursor_ = 0;
        replayMismatch_ = false;
    }

    void setReplayRecord(std::vector<DeliveryDecision> record) {
        policy_ = {};
        replayMode_ = true;
        replayRecord_ = std::move(record);
        replayCursor_ = 0;
        replayMismatch_ = false;
    }

    domes::peer_drill::CodecError send(SimMessage message) {
        const auto expectedSenderMac = senderMacForPod(message.srcPodId);
        if (message.semantic.sender_mac.size != expectedSenderMac.size() ||
            !std::equal(expectedSenderMac.begin(), expectedSenderMac.end(),
                        message.semantic.sender_mac.bytes)) {
            codecFailure_ = domes::peer_drill::CodecError::kMalformed;
            return *codecFailure_;
        }

        const auto registeredRole = roles_.find(message.srcPodId);
        if (registeredRole == roles_.end()) {
            codecFailure_ = domes::peer_drill::CodecError::kBadRole;
            return *codecFailure_;
        }
        const auto validation =
            domes::peer_drill::validateForSenderRole(message.semantic, registeredRole->second);
        if (validation != domes::peer_drill::CodecError::kOk) {
            codecFailure_ = validation;
            return validation;
        }

        message.sentAtUs = clock_.nowUs();
        message.sequence = nextSequence_++;
        message.semantic.timestamp_us = static_cast<uint32_t>(message.sentAtUs);
        const auto result = domes::peer_drill::encodeLegacyV1(message.semantic, message.legacy);
        if (result != domes::peer_drill::CodecError::kOk) {
            codecFailure_ = result;
            return result;
        }

        std::ostringstream stream;
        stream << "espnow.send " << messageTypeName(messageTag(message)) << " pod"
               << message.srcPodId << "->";
        if (message.dstPodId == kBroadcastPodId) {
            stream << "ALL";
        } else {
            stream << "pod" << message.dstPodId;
        }
        log_.log(message.srcPodId, "espnow", stream.str());

        pending_.push_back(std::move(message));
        return domes::peer_drill::CodecError::kOk;
    }

    void deliverPending() {
        resolvePendingTransmissions();

        std::stable_sort(scheduled_.begin(), scheduled_.end(),
                         [](const ScheduledDelivery& lhs, const ScheduledDelivery& rhs) {
                             if (lhs.deliverAtUs != rhs.deliverAtUs) {
                                 return lhs.deliverAtUs < rhs.deliverAtUs;
                             }
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
            if (handler == handlers_.end()) {
                continue;
            }

            SimMessage decoded = delivery.message;
            decoded.semantic = domes_peer_drill_PeerMessage_init_zero;
            const auto result =
                domes::peer_drill::decodeLegacyV1(decoded.legacy.view(), decoded.semantic);
            if (result != domes::peer_drill::CodecError::kOk) {
                codecFailure_ = result;
                continue;
            }

            flowEvents_.push_back({clock_.nowUs(), decoded.srcPodId, delivery.dstPod,
                                   messageTag(decoded), decoded.sequence});
            handler->second(decoded);
        }
        scheduled_ = std::move(waiting);
    }

    const std::vector<FlowEvent>& flowEvents() const { return flowEvents_; }
    const std::vector<DeliveryDecision>& deliveryRecord() const { return deliveryRecord_; }

    bool replayComplete() const {
        return replayMode_ && !replayMismatch_ && replayCursor_ == replayRecord_.size() &&
               pending_.empty() && scheduled_.empty();
    }

    bool replayMismatch() const { return replayMismatch_; }
    std::optional<domes::peer_drill::CodecError> codecFailure() const { return codecFailure_; }

    std::optional<uint64_t> nextDeliveryTimeUs() const {
        if (scheduled_.empty()) {
            return std::nullopt;
        }

        auto next =
            std::min_element(scheduled_.begin(), scheduled_.end(),
                             [](const ScheduledDelivery& lhs, const ScheduledDelivery& rhs) {
                                 return lhs.deliverAtUs < rhs.deliverAtUs;
                             });
        return next->deliverAtUs;
    }

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
            if (message.dstPodId == kBroadcastPodId) {
                for (const auto& [podId, handler] : handlers_) {
                    (void)handler;
                    if (podId != message.srcPodId) {
                        resolveDelivery(message, podId);
                    }
                }
            } else if (handlers_.contains(message.dstPodId)) {
                resolveDelivery(message, message.dstPodId);
            }
        }
    }

    void resolveDelivery(const SimMessage& message, uint16_t dstPod) {
        DeliveryContext context{
            message.sentAtUs,
            message.srcPodId,
            message.dstPodId,
            dstPod,
            messageTag(message),
            message.sequence,
            {message.legacy.view().begin(), message.legacy.view().end()},
        };
        auto directive = chooseDirective(context);
        if (!directive) {
            return;
        }
        deliveryRecord_.push_back({context, *directive});

        if (directive->action == DeliveryAction::kDrop) {
            return;
        }

        schedule(message, dstPod, context.sentAtUs, directive->delayUs);
        if (directive->action == DeliveryAction::kDuplicate) {
            schedule(message, dstPod, context.sentAtUs, directive->delayUs);
        }
    }

    std::optional<DeliveryDirective> chooseDirective(const DeliveryContext& context) {
        if (replayMode_) {
            if (replayMismatch_) {
                return std::nullopt;
            }
            if (replayCursor_ >= replayRecord_.size()) {
                replayMismatch_ = true;
                return std::nullopt;
            }

            const auto& expected = replayRecord_[replayCursor_++];
            if (expected.context != context) {
                replayMismatch_ = true;
                return std::nullopt;
            }
            return expected.directive;
        }

        return policy_ ? policy_(context) : DeliveryDirective{};
    }

    void schedule(const SimMessage& message, uint16_t dstPod, uint64_t sentAtUs, uint64_t delayUs) {
        scheduled_.push_back({sentAtUs + delayUs, nextDeliveryOrder_++, dstPod, message});
    }

    SimLog& log_;
    SimClock& clock_;
    std::map<uint16_t, MessageHandler> handlers_;
    std::map<uint16_t, domes_peer_drill_PeerRole> roles_;
    std::vector<SimMessage> pending_;
    std::vector<ScheduledDelivery> scheduled_;
    std::vector<FlowEvent> flowEvents_;
    std::vector<DeliveryDecision> deliveryRecord_;
    DeliveryPolicy policy_;
    bool replayMode_ = false;
    std::vector<DeliveryDecision> replayRecord_;
    size_t replayCursor_ = 0;
    bool replayMismatch_ = false;
    std::optional<domes::peer_drill::CodecError> codecFailure_;
    uint32_t nextSequence_ = 0;
    uint64_t nextDeliveryOrder_ = 0;
};

}  // namespace sim
