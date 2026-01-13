#pragma once

/**
 * @file configCommandHandler.hpp
 * @brief Handler for config protocol commands over serial
 *
 * Processes config commands received via the frame protocol and
 * sends responses. Works with SerialOtaReceiver for command dispatch.
 */

#include "configProtocol.hpp"
#include "featureManager.hpp"
#include "interfaces/iTransport.hpp"
#include "utils/rgbPatternController.hpp"

#include <cstdint>
#include <cstddef>

namespace domes::config {

/**
 * @brief Handles config protocol commands
 *
 * Processes incoming config commands and generates responses.
 * Uses the same frame format as OTA protocol.
 */
class ConfigCommandHandler {
public:
    /**
     * @brief Construct command handler
     *
     * @param transport Transport to send responses on
     * @param features Feature manager for toggle state
     * @param rgbController Optional RGB pattern controller (may be nullptr)
     */
    ConfigCommandHandler(ITransport& transport, FeatureManager& features,
                         RgbPatternController* rgbController = nullptr);

    /**
     * @brief Handle an incoming config command
     *
     * @param type Message type (ConfigMsgType value)
     * @param payload Command payload (may be nullptr)
     * @param len Payload length
     * @return true if command was handled successfully
     */
    bool handleCommand(uint8_t type, const uint8_t* payload, size_t len);

private:
    /**
     * @brief Handle LIST_FEATURES request
     */
    void handleListFeatures();

    /**
     * @brief Handle SET_FEATURE request
     *
     * @param payload Payload containing SetFeatureRequest
     * @param len Payload length
     */
    void handleSetFeature(const uint8_t* payload, size_t len);

    /**
     * @brief Handle GET_FEATURE request
     *
     * @param payload Payload containing GetFeatureRequest
     * @param len Payload length
     */
    void handleGetFeature(const uint8_t* payload, size_t len);

    // =========================================================================
    // RGB Pattern handlers
    // =========================================================================

    /**
     * @brief Handle SET_RGB_PATTERN request
     */
    void handleSetRgbPattern(const uint8_t* payload, size_t len);

    /**
     * @brief Handle GET_RGB_PATTERN request
     */
    void handleGetRgbPattern();

    /**
     * @brief Handle LIST_RGB_PATTERNS request
     */
    void handleListRgbPatterns();

    /**
     * @brief Send set RGB pattern response
     */
    void sendSetRgbPatternResponse(RgbPattern pattern);

    /**
     * @brief Send get RGB pattern response
     */
    void sendGetRgbPatternResponse();

    /**
     * @brief Send list RGB patterns response
     */
    void sendListRgbPatternsResponse();

    /**
     * @brief Send list features response
     */
    void sendListFeaturesResponse();

    /**
     * @brief Send set feature response
     *
     * @param status Status code
     * @param feature Feature that was set
     * @param enabled New enabled state
     */
    void sendSetFeatureResponse(ConfigStatus status, Feature feature, bool enabled);

    /**
     * @brief Send get feature response
     *
     * @param status Status code
     * @param feature Feature that was queried
     * @param enabled Current enabled state
     */
    void sendGetFeatureResponse(ConfigStatus status, Feature feature, bool enabled);

    /**
     * @brief Send a frame with given type and payload
     *
     * @param type Message type
     * @param payload Payload data
     * @param len Payload length
     * @return true on success
     */
    bool sendFrame(ConfigMsgType type, const uint8_t* payload, size_t len);

    ITransport& transport_;
    FeatureManager& features_;
    RgbPatternController* rgbController_;
};

}  // namespace domes::config
