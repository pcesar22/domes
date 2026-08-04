#pragma once

#include "interfaces/iLedDriver.hpp"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace sim {

enum class SimMessageType : uint8_t {
    kSetColor = 0x02,
    kArmTouch = 0x03,
    kPlaySound = 0x04,
    kStopAll = 0x06,
    kTouchEvent = 0x10,
    kTimeoutEvent = 0x11,
    kJoinGame = 0xE0,
};

struct SimMessageHeader {
    uint16_t srcPodId = 0;
    uint16_t dstPodId = 0;  // 0xFFFF = broadcast
    SimMessageType type = SimMessageType::kSetColor;
    uint64_t timestampUs = 0;
    uint32_t sequence = 0;
};

struct SetColorCommand {
    SimMessageHeader header;
    uint8_t r = 0, g = 0, b = 0;
};

struct ArmTouchCommand {
    SimMessageHeader header;
    uint32_t timeoutMs = 3000;
    uint8_t feedbackMode = 0x03;
    uint64_t roundToken = 0;
};

struct PlaySoundCommand {
    SimMessageHeader header;
    std::string soundName;
};

struct StopAllCommand {
    SimMessageHeader header;
};

struct TouchEventMsg {
    SimMessageHeader header;
    uint32_t reactionTimeUs = 0;
    uint8_t padIndex = 0;
    uint64_t roundToken = 0;
};

struct TimeoutEventMsg {
    SimMessageHeader header;
    uint64_t roundToken = 0;
};

struct JoinGameCommand {
    SimMessageHeader header;
};

using SimMessage = std::variant<SetColorCommand, ArmTouchCommand, PlaySoundCommand, StopAllCommand,
                                TouchEventMsg, TimeoutEventMsg, JoinGameCommand>;

// Helper to get header from any message variant
inline const SimMessageHeader& getHeader(const SimMessage& msg) {
    return std::visit([](const auto& m) -> const SimMessageHeader& { return m.header; }, msg);
}

// Helper to get mutable header from any message variant
inline SimMessageHeader& getMutableHeader(SimMessage& msg) {
    return std::visit([](auto& m) -> SimMessageHeader& { return m.header; }, msg);
}

inline std::vector<uint8_t> canonicalPayload(const SimMessage& msg) {
    std::vector<uint8_t> payload = {static_cast<uint8_t>(msg.index())};
    auto appendU32 = [&payload](uint32_t value) {
        for (size_t i = 0; i < sizeof(value); i++) {
            payload.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };
    auto appendU64 = [&payload](uint64_t value) {
        for (size_t i = 0; i < sizeof(value); i++) {
            payload.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };

    std::visit(
        [&payload, &appendU32, &appendU64](const auto& message) {
            using Message = std::decay_t<decltype(message)>;
            if constexpr (std::is_same_v<Message, SetColorCommand>) {
                payload.insert(payload.end(), {message.r, message.g, message.b});
            } else if constexpr (std::is_same_v<Message, ArmTouchCommand>) {
                appendU32(message.timeoutMs);
                payload.push_back(message.feedbackMode);
                appendU64(message.roundToken);
            } else if constexpr (std::is_same_v<Message, PlaySoundCommand>) {
                payload.insert(payload.end(), message.soundName.begin(), message.soundName.end());
            } else if constexpr (std::is_same_v<Message, TouchEventMsg>) {
                appendU32(message.reactionTimeUs);
                payload.push_back(message.padIndex);
                appendU64(message.roundToken);
            } else if constexpr (std::is_same_v<Message, TimeoutEventMsg>) {
                appendU64(message.roundToken);
            }
        },
        msg);
    return payload;
}

// Helper to get message type name for logging
inline const char* messageTypeName(SimMessageType type) {
    switch (type) {
        case SimMessageType::kSetColor:
            return "SET_COLOR";
        case SimMessageType::kArmTouch:
            return "ARM_TOUCH";
        case SimMessageType::kPlaySound:
            return "PLAY_SOUND";
        case SimMessageType::kStopAll:
            return "STOP_ALL";
        case SimMessageType::kTouchEvent:
            return "TOUCH_EVENT";
        case SimMessageType::kTimeoutEvent:
            return "TIMEOUT_EVENT";
        case SimMessageType::kJoinGame:
            return "JOIN_GAME";
        default:
            return "UNKNOWN";
    }
}

constexpr uint16_t kBroadcastPodId = 0xFFFF;

}  // namespace sim
