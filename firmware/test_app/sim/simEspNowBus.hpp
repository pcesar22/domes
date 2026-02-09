#pragma once

#include "sim/simProtocol.hpp"
#include "sim/simLog.hpp"
#include "esp_timer.h"

#include <functional>
#include <map>
#include <vector>
#include <sstream>

namespace sim {

struct FlowEvent {
    uint64_t timestampUs;
    uint16_t srcPod;
    uint16_t dstPod;
    SimMessageType type;
    uint32_t sequence;
};

using MessageHandler = std::function<void(const SimMessage&)>;

class SimEspNowBus {
public:
    explicit SimEspNowBus(SimLog& log) : log_(log) {}

    void registerPod(uint16_t podId, MessageHandler handler) {
        handlers_[podId] = std::move(handler);
    }

    void send(SimMessage msg) {
        auto& hdr = getMutableHeader(msg);
        hdr.timestampUs = static_cast<uint64_t>(test_stubs::mock_time_us.load());
        hdr.sequence = nextSequence_++;

        // Log the send
        std::ostringstream oss;
        oss << "espnow.send " << messageTypeName(hdr.type)
            << " pod" << hdr.srcPodId << "->";
        if (hdr.dstPodId == kBroadcastPodId) oss << "ALL";
        else oss << "pod" << hdr.dstPodId;
        log_.log(hdr.srcPodId, "espnow", oss.str());

        pending_.push_back(std::move(msg));
    }

    void deliverPending() {
        auto messages = std::move(pending_);
        pending_.clear();

        for (const auto& msg : messages) {
            const auto& header = getHeader(msg);

            if (header.dstPodId == kBroadcastPodId) {
                // Broadcast: deliver to all except sender
                for (auto& [podId, handler] : handlers_) {
                    if (podId != header.srcPodId) {
                        flowEvents_.push_back({header.timestampUs, header.srcPodId,
                                               podId, header.type, header.sequence});
                        handler(msg);
                    }
                }
            } else {
                // Unicast: deliver to specific pod
                auto it = handlers_.find(header.dstPodId);
                if (it != handlers_.end()) {
                    flowEvents_.push_back({header.timestampUs, header.srcPodId,
                                           header.dstPodId, header.type, header.sequence});
                    it->second(msg);
                }
            }
        }
    }

    const std::vector<FlowEvent>& flowEvents() const { return flowEvents_; }
    void clearFlowEvents() { flowEvents_.clear(); }
    size_t pendingCount() const { return pending_.size(); }

private:
    SimLog& log_;
    std::map<uint16_t, MessageHandler> handlers_;
    std::vector<SimMessage> pending_;
    std::vector<FlowEvent> flowEvents_;
    uint32_t nextSequence_ = 0;
};

}  // namespace sim
