/**
 * @file configCommandHandler.cpp
 * @brief Config command handler implementation
 *
 * Uses nanopb for protobuf encoding/decoding of config messages.
 */

#include "configCommandHandler.hpp"

#include "config.hpp"
#include "config.pb.h"

#include "drivers/injectableTouchDriver.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "infra/appMetadata.hpp"
#include "infra/crashDumpHandler.hpp"
#include "infra/memoryProfiler.hpp"
#include "infra/nvsConfig.hpp"
#include "infra/smokeTest.hpp"
#include "interfaces/iOtaManager.hpp"
#include "pb_decode.h"
#include "pb_encode.h"
#include "protocol/frameCodec.hpp"
#include "protocol/memoryProfileLimits.hpp"
#include "services/espNowService.hpp"
#include "services/imuService.hpp"
#include "services/ledService.hpp"
#include "trace/traceApi.hpp"
#include "transport/espNowTransport.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>

namespace {
constexpr const char* kTag = "config_cmd";
#if defined(CONFIG_DOMES_WIFI_AUTO_CONNECT) && defined(CONFIG_DOMES_OTA_AUTO_CHECK)
constexpr uint8_t kAutoUpdateDefault = 1;
#elif defined(CONFIG_DOMES_WIFI_AUTO_CONNECT)
constexpr uint8_t kAutoUpdateDefault = 0;
#endif

/// Read pod_id from NVS (0 if not set)
uint8_t readPodIdFromNvs() {
    domes::infra::NvsConfig config;
    if (config.open(domes::infra::nvs_ns::kConfig) != ESP_OK)
        return 0;
    uint8_t id = config.getOrDefault<uint8_t>(domes::infra::config_key::kPodId, 0);
    config.close();
    return id;
}

uint32_t readBootCountFromNvs() {
    domes::infra::NvsConfig stats;
    if (stats.open(domes::infra::nvs_ns::kStats) != ESP_OK)
        return 0;
    const uint32_t count = stats.getOrDefault<uint32_t>(domes::infra::stats_key::kBootCount, 0);
    stats.close();
    return count;
}

domes_config_ResetReason resetReasonToProto(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return domes_config_ResetReason_RESET_REASON_POWER_ON;
        case ESP_RST_EXT:
            return domes_config_ResetReason_RESET_REASON_EXTERNAL_PIN;
        case ESP_RST_SW:
            return domes_config_ResetReason_RESET_REASON_SOFTWARE;
        case ESP_RST_PANIC:
            return domes_config_ResetReason_RESET_REASON_PANIC;
        case ESP_RST_INT_WDT:
            return domes_config_ResetReason_RESET_REASON_INTERRUPT_WATCHDOG;
        case ESP_RST_TASK_WDT:
            return domes_config_ResetReason_RESET_REASON_TASK_WATCHDOG;
        case ESP_RST_WDT:
            return domes_config_ResetReason_RESET_REASON_WATCHDOG;
        case ESP_RST_DEEPSLEEP:
            return domes_config_ResetReason_RESET_REASON_DEEP_SLEEP;
        case ESP_RST_BROWNOUT:
            return domes_config_ResetReason_RESET_REASON_BROWNOUT;
        case ESP_RST_SDIO:
            return domes_config_ResetReason_RESET_REASON_SDIO;
        case ESP_RST_USB:
            return domes_config_ResetReason_RESET_REASON_USB;
        case ESP_RST_JTAG:
            return domes_config_ResetReason_RESET_REASON_JTAG;
        case ESP_RST_EFUSE:
            return domes_config_ResetReason_RESET_REASON_EFUSE;
        case ESP_RST_PWR_GLITCH:
            return domes_config_ResetReason_RESET_REASON_POWER_GLITCH;
        case ESP_RST_CPU_LOCKUP:
            return domes_config_ResetReason_RESET_REASON_CPU_LOCKUP;
        case ESP_RST_UNKNOWN:
        default:
            return domes_config_ResetReason_RESET_REASON_UNKNOWN;
    }
}
}  // namespace

namespace domes::config {

ConfigCommandHandler::ConfigCommandHandler(ITransport& transport, FeatureManager& features)
    : transport_(transport), features_(features) {}

bool ConfigCommandHandler::handleCommand(uint8_t type, const uint8_t* payload, size_t len) {
    TRACE_SCOPE(TRACE_ID("Config.HandleCommand"), domes::trace::Category::kTransport);
    auto msgType = static_cast<MsgType>(type);

    // Only explicit actions count as activity. Observability polling must not
    // mutate IDLE into TRIAGE or keep TRIAGE alive indefinitely.
    if (modeManager_ && commandRecordsActivity(msgType)) {
        modeManager_->resetActivityTimer();
        if (modeManager_->currentMode() == SystemMode::kIdle) {
            modeManager_->transitionTo(SystemMode::kTriage);
        }
    }

    switch (msgType) {
        case MsgType::kListFeaturesReq:
            ESP_LOGD(kTag, "Received LIST_FEATURES");
            handleListFeatures();
            return true;

        case MsgType::kSetFeatureReq:
            ESP_LOGD(kTag, "Received SET_FEATURE");
            handleSetFeature(payload, len);
            return true;

        case MsgType::kGetFeatureReq:
            ESP_LOGD(kTag, "Received GET_FEATURE");
            handleGetFeature(payload, len);
            return true;

        case MsgType::kSetLedPatternReq:
            ESP_LOGD(kTag, "Received SET_LED_PATTERN");
            handleSetLedPattern(payload, len);
            return true;

        case MsgType::kGetLedPatternReq:
            ESP_LOGD(kTag, "Received GET_LED_PATTERN");
            handleGetLedPattern();
            return true;

        case MsgType::kSetImuTriageReq:
            ESP_LOGD(kTag, "Received SET_IMU_TRIAGE");
            handleSetImuTriage(payload, len);
            return true;

        case MsgType::kGetModeReq:
            ESP_LOGD(kTag, "Received GET_MODE");
            handleGetMode();
            return true;

        case MsgType::kSetModeReq:
            ESP_LOGD(kTag, "Received SET_MODE");
            handleSetMode(payload, len);
            return true;

        case MsgType::kGetSystemInfoReq:
            ESP_LOGD(kTag, "Received GET_SYSTEM_INFO");
            handleGetSystemInfo();
            return true;

        case MsgType::kSetPodIdReq:
            ESP_LOGD(kTag, "Received SET_POD_ID");
            handleSetPodId(payload, len);
            return true;

        case MsgType::kGetHealthReq:
            ESP_LOGD(kTag, "Received GET_HEALTH");
            handleGetHealth();
            return true;

        case MsgType::kGetEspNowStatusReq:
            ESP_LOGD(kTag, "Received GET_ESPNOW_STATUS");
            handleGetEspNowStatus();
            return true;

        case MsgType::kEspNowBenchReq:
            ESP_LOGD(kTag, "Received ESPNOW_BENCH");
            handleEspNowBench(payload, len);
            return true;

        case MsgType::kGetCrashDumpReq:
            ESP_LOGD(kTag, "Received GET_CRASH_DUMP");
            handleGetCrashDump();
            return true;

        case MsgType::kClearCrashDumpReq:
            ESP_LOGD(kTag, "Received CLEAR_CRASH_DUMP");
            handleClearCrashDump();
            return true;

        case MsgType::kGetMemoryProfileReq:
            ESP_LOGD(kTag, "Received GET_MEMORY_PROFILE");
            handleGetMemoryProfile();
            return true;

        case MsgType::kSelfTestReq:
            ESP_LOGI(kTag, "Received SELF_TEST");
            handleSelfTest();
            return true;

        case MsgType::kCheckUpdateReq:
            ESP_LOGI(kTag, "Received CHECK_UPDATE");
            handleCheckUpdate();
            return true;

        case MsgType::kSetAutoUpdateReq:
            ESP_LOGD(kTag, "Received SET_AUTO_UPDATE");
            handleSetAutoUpdate(payload, len);
            return true;

        case MsgType::kSimulateTouchReq:
            ESP_LOGI(kTag, "Received SIMULATE_TOUCH");
            handleSimulateTouch(payload, len);
            return true;

        case MsgType::kSetSimModeReq:
            ESP_LOGI(kTag, "Received SET_SIM_MODE");
            handleSetSimMode(payload, len);
            return true;

        default:
            ESP_LOGW(kTag, "Unknown config command: 0x%02X", type);
            return false;
    }
}

bool ConfigCommandHandler::sendTouchEvent(uint8_t podId, uint8_t padIndex, uint64_t timestampUs) {
    domes_config_TouchEventNotification event = domes_config_TouchEventNotification_init_zero;
    event.pod_id = podId;
    event.pad_index = padIndex;
    event.timestamp_us = timestampUs;

    std::array<uint8_t, domes_config_TouchEventNotification_size> payload{};
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());
    if (!pb_encode(&stream, domes_config_TouchEventNotification_fields, &event)) {
        ESP_LOGE(kTag, "Failed to encode TouchEventNotification: %s", PB_GET_ERROR(&stream));
        return false;
    }

    return sendFrame(MsgType::kTouchEventNtf, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::handleListFeatures() {
    sendListFeaturesResponse();
}

void ConfigCommandHandler::handleSetFeature(const uint8_t* payload, size_t len) {
    // Decode protobuf message
    domes_config_SetFeatureRequest req = domes_config_SetFeatureRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetFeatureRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_FEATURE: %s", PB_GET_ERROR(&stream));
        sendSetFeatureResponse(Status::kError, Feature::kUnknown, false);
        return;
    }

    auto feature = static_cast<Feature>(req.feature);
    bool enabled = req.enabled;

    ESP_LOGI(kTag, "Setting feature %s (%d) to %s", featureToString(feature),
             static_cast<int>(req.feature), enabled ? "enabled" : "disabled");

    if (!features_.isSupported(feature)) {
        ESP_LOGW(kTag, "Invalid or unsupported feature ID: %d", static_cast<int>(req.feature));
        sendSetFeatureResponse(Status::kInvalidFeature, feature, false);
        return;
    }

    // A WiFi/TCP request must put its response on the wire before disabling
    // the station tears down the connection carrying that response. The
    // FeatureManager barrier preserves command ordering across transports.
    if (feature == Feature::kWifi && !enabled) {
        const bool updated = features_.setEnabled(feature, false, [this, feature] {
            sendSetFeatureResponse(Status::kOk, feature, false);
            const TransportError err = transport_.flush();
            if (!isOk(err)) {
                ESP_LOGW(kTag, "Failed to flush WiFi-disable response: %s",
                         transportErrorToString(err));
            }
        });
        if (!updated) {
            sendSetFeatureResponse(Status::kInvalidFeature, feature, false);
        }
        return;
    }

    if (!features_.setEnabled(feature, enabled)) {
        ESP_LOGW(kTag, "Failed to update feature ID: %d", static_cast<int>(req.feature));
        sendSetFeatureResponse(Status::kInvalidFeature, feature, false);
        return;
    }

    sendSetFeatureResponse(Status::kOk, feature, enabled);
}

void ConfigCommandHandler::handleGetFeature(const uint8_t* payload, size_t len) {
    domes_config_GetFeatureRequest req = domes_config_GetFeatureRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_GetFeatureRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode GET_FEATURE: %s", PB_GET_ERROR(&stream));
        sendGetFeatureResponse(Status::kError, Feature::kUnknown, false);
        return;
    }

    auto feature = static_cast<Feature>(req.feature);

    // Check if feature is valid
    if (!features_.isSupported(feature)) {
        ESP_LOGW(kTag, "Invalid or unsupported feature ID: %d", static_cast<int>(req.feature));
        sendGetFeatureResponse(Status::kInvalidFeature, feature, false);
        return;
    }

    bool enabled = features_.isEnabled(feature);
    sendGetFeatureResponse(Status::kOk, feature, enabled);
}

void ConfigCommandHandler::sendListFeaturesResponse() {
    // Build protobuf response
    domes_config_ListFeaturesResponse resp = domes_config_ListFeaturesResponse_init_zero;

    // Include pod identity
    resp.pod_id = readPodIdFromNvs();

    resp.features_count = static_cast<pb_size_t>(features_.getAll(resp.features));

    // Encode to buffer
    std::array<uint8_t, domes_config_ListFeaturesResponse_size + 10> payload;
    pb_ostream_t stream = pb_ostream_from_buffer(payload.data(), payload.size());

    if (!pb_encode(&stream, domes_config_ListFeaturesResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode ListFeaturesResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kListFeaturesRsp, payload.data(), stream.bytes_written);
}

void ConfigCommandHandler::sendSetFeatureResponse(Status status, Feature feature, bool enabled) {
    // Build protobuf response
    domes_config_SetFeatureResponse resp = domes_config_SetFeatureResponse_init_zero;
    resp.has_feature = true;
    resp.feature.feature = static_cast<domes_config_Feature>(feature);
    resp.feature.enabled = enabled;

    // Encode to buffer: [status_byte][SetFeatureResponse_proto]
    std::array<uint8_t, domes_config_SetFeatureResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t resp_stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&resp_stream, domes_config_SetFeatureResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetFeatureResponse: %s", PB_GET_ERROR(&resp_stream));
        return;
    }

    sendFrame(MsgType::kSetFeatureRsp, payload.data(), 1 + resp_stream.bytes_written);
}

void ConfigCommandHandler::sendGetFeatureResponse(Status status, Feature feature, bool enabled) {
    domes_config_GetFeatureResponse resp = domes_config_GetFeatureResponse_init_zero;
    resp.has_feature = true;
    resp.feature.feature = static_cast<domes_config_Feature>(feature);
    resp.feature.enabled = enabled;

    std::array<uint8_t, domes_config_GetFeatureResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetFeatureResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetFeatureResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetFeatureRsp, payload.data(), 1 + stream.bytes_written);
}

bool ConfigCommandHandler::sendFrame(MsgType type, const uint8_t* payload, size_t len) {
    std::array<uint8_t, kMaxFrameSize> frameBuf;
    size_t frameLen = 0;

    TransportError err = encodeFrame(static_cast<uint8_t>(type), payload, len, frameBuf.data(),
                                     frameBuf.size(), &frameLen);

    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to encode frame");
        return false;
    }

    err = transport_.send(frameBuf.data(), frameLen);
    if (!isOk(err)) {
        ESP_LOGE(kTag, "Failed to send frame: %s", transportErrorToString(err));
        return false;
    }

    return true;
}

void ConfigCommandHandler::handleSetLedPattern(const uint8_t* payload, size_t len) {
    if (!ledService_) {
        ESP_LOGW(kTag, "LED service not available");
        sendLedPatternResponse(Status::kError);
        return;
    }

    // Decode protobuf message
    domes_config_SetLedPatternRequest req = domes_config_SetLedPatternRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetLedPatternRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_LED_PATTERN: %s", PB_GET_ERROR(&stream));
        sendLedPatternResponse(Status::kError);
        return;
    }
    if (!isValidLedPattern(req.pattern)) {
        ESP_LOGW(kTag, "Rejected invalid LED pattern fields");
        sendLedPatternResponse(Status::kInvalidPattern);
        return;
    }

    ESP_LOGI(kTag, "Setting LED pattern: type=%d, period=%lu, brightness=%lu", req.pattern.type,
             req.pattern.period_ms, req.pattern.brightness);

    esp_err_t err = ledService_->setPattern(req.pattern);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to set LED pattern: %s", esp_err_to_name(err));
        sendLedPatternResponse(Status::kInvalidPattern);
        return;
    }

    sendLedPatternResponse(Status::kOk);
}

void ConfigCommandHandler::handleGetLedPattern() {
    if (!ledService_) {
        ESP_LOGW(kTag, "LED service not available");
        sendLedPatternResponse(Status::kError);
        return;
    }

    // Build response with current pattern
    domes_config_GetLedPatternResponse resp = domes_config_GetLedPatternResponse_init_zero;
    resp.has_pattern = true;
    ledService_->getPattern(resp.pattern);

    // Encode to buffer: [status_byte][GetLedPatternResponse_proto]
    std::array<uint8_t, domes_config_GetLedPatternResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_GetLedPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetLedPatternResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kGetLedPatternRsp, payload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::sendLedPatternResponse(Status status) {
    // Build response with current pattern
    domes_config_SetLedPatternResponse resp = domes_config_SetLedPatternResponse_init_zero;

    if (ledService_ && status == Status::kOk) {
        resp.has_pattern = true;
        ledService_->getPattern(resp.pattern);
    }

    // Encode to buffer: [status_byte][SetLedPatternResponse_proto]
    std::array<uint8_t, domes_config_SetLedPatternResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetLedPatternResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetLedPatternResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetLedPatternRsp, payload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleSetImuTriage(const uint8_t* payload, size_t len) {
    if (!imuService_) {
        ESP_LOGW(kTag, "IMU service not available");
        sendImuTriageResponse(Status::kError, false);
        return;
    }

    // Decode protobuf message
    domes_config_SetImuTriageRequest req = domes_config_SetImuTriageRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetImuTriageRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_IMU_TRIAGE: %s", PB_GET_ERROR(&stream));
        sendImuTriageResponse(Status::kError, false);
        return;
    }

    ESP_LOGI(kTag, "Setting IMU triage mode to %s", req.enabled ? "enabled" : "disabled");

    imuService_->setTriageMode(req.enabled);
    sendImuTriageResponse(Status::kOk, req.enabled);
}

void ConfigCommandHandler::sendImuTriageResponse(Status status, bool enabled) {
    // Build protobuf response
    domes_config_SetImuTriageResponse resp = domes_config_SetImuTriageResponse_init_zero;
    resp.enabled = enabled;

    // Encode to buffer: [status_byte][SetImuTriageResponse_proto]
    std::array<uint8_t, domes_config_SetImuTriageResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t ostream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetImuTriageResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetImuTriageResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetImuTriageRsp, payload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleGetMode() {
    if (!modeManager_) {
        ESP_LOGW(kTag, "Mode manager not available");
        // Send error response
        std::array<uint8_t, 1> payload;
        payload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kGetModeRsp, payload.data(), 1);
        return;
    }

    domes_config_GetModeResponse resp = domes_config_GetModeResponse_init_zero;
    resp.mode = static_cast<domes_config_SystemMode>(modeManager_->currentMode());
    resp.time_in_mode_ms = modeManager_->timeInModeMs();

    // Encode to buffer: [status_byte][protobuf]
    std::array<uint8_t, domes_config_GetModeResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetModeResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetModeResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetModeRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleSetMode(const uint8_t* payload, size_t len) {
    if (!modeManager_) {
        ESP_LOGW(kTag, "Mode manager not available");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetModeRsp, errPayload.data(), 1);
        return;
    }

    // Decode protobuf
    domes_config_SetModeRequest req = domes_config_SetModeRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetModeRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_MODE: %s", PB_GET_ERROR(&stream));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetModeRsp, errPayload.data(), 1);
        return;
    }

    auto targetMode = static_cast<SystemMode>(req.mode);
    ESP_LOGI(kTag, "Set mode request: %s", systemModeToString(targetMode));

    bool ok = modeManager_->transitionTo(targetMode);

    // Build response
    domes_config_SetModeResponse resp = domes_config_SetModeResponse_init_zero;
    resp.mode = static_cast<domes_config_SystemMode>(modeManager_->currentMode());
    resp.transition_ok = ok;

    std::array<uint8_t, domes_config_SetModeResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetModeResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetModeResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetModeRsp, respPayload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleGetSystemInfo() {
    domes_config_GetSystemInfoResponse resp = domes_config_GetSystemInfoResponse_init_zero;

    // Firmware version
    strncpy(resp.firmware_version, infra::firmwareVersion(), sizeof(resp.firmware_version) - 1);

    // Uptime in seconds
    resp.uptime_s = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);

    // Free heap
    constexpr uint32_t kInternalHeapCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    resp.free_heap = static_cast<uint32_t>(heap_caps_get_free_size(kInternalHeapCaps));

    resp.boot_count = readBootCountFromNvs();
    resp.reset_reason = resetReasonToProto(esp_reset_reason());

    // Mode and feature mask
    if (modeManager_) {
        resp.mode = static_cast<domes_config_SystemMode>(modeManager_->currentMode());
    } else {
        resp.mode = domes_config_SystemMode_SYSTEM_MODE_BOOTING;
    }
    resp.feature_mask = features_.getMask();

    // Pod identity
    resp.pod_id = readPodIdFromNvs();

    // Encode to buffer: [status_byte][protobuf]
    std::array<uint8_t, domes_config_GetSystemInfoResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetSystemInfoResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetSystemInfoResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetSystemInfoRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleSetPodId(const uint8_t* payload, size_t len) {
    domes_config_SetPodIdRequest req = domes_config_SetPodIdRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetPodIdRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_POD_ID: %s", PB_GET_ERROR(&stream));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetPodIdRsp, errPayload.data(), 1);
        return;
    }

    if (req.pod_id == 0 || req.pod_id > 255) {
        ESP_LOGW(kTag, "Invalid pod_id: %lu (must be 1-255)", req.pod_id);
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetPodIdRsp, errPayload.data(), 1);
        return;
    }

    // Write to NVS
    infra::NvsConfig config;
    if (config.open(infra::nvs_ns::kConfig) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS for pod_id write");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetPodIdRsp, errPayload.data(), 1);
        return;
    }

    esp_err_t err = config.setU8(infra::config_key::kPodId, static_cast<uint8_t>(req.pod_id));
    if (err == ESP_OK) {
        err = config.commit();
    }
    config.close();

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to persist pod_id: %s", esp_err_to_name(err));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetPodIdRsp, errPayload.data(), 1);
        return;
    }

    ESP_LOGI(kTag, "Pod ID set to %lu (reboot to apply BLE name change)", req.pod_id);

    // Send response
    domes_config_SetPodIdResponse resp = domes_config_SetPodIdResponse_init_zero;
    resp.pod_id = req.pod_id;

    std::array<uint8_t, domes_config_SetPodIdResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetPodIdResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetPodIdResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetPodIdRsp, respPayload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleGetHealth() {
    domes_config_GetHealthResponse resp = domes_config_GetHealthResponse_init_zero;

    // Heap info
    constexpr uint32_t kInternalHeapCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    resp.free_heap = static_cast<uint32_t>(heap_caps_get_free_size(kInternalHeapCaps));
    resp.min_free_heap = static_cast<uint32_t>(heap_caps_get_minimum_free_size(kInternalHeapCaps));

    // Uptime
    resp.uptime_seconds = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);

    // WiFi RSSI (0 if not connected)
    wifi_ap_record_t apInfo;
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
        resp.wifi_rssi = apInfo.rssi;
    } else {
        resp.wifi_rssi = 0;
    }

    // FreeRTOS task info
    static constexpr UBaseType_t kTaskSnapshotSlack = 8;
    static constexpr uint8_t kTaskSnapshotAttempts = 3;
    std::unique_ptr<TaskStatus_t[]> taskStatuses;
    UBaseType_t capacity = 0;
    UBaseType_t got = 0;

    for (uint8_t attempt = 0; attempt < kTaskSnapshotAttempts && got == 0; ++attempt) {
        capacity = uxTaskGetNumberOfTasks() + kTaskSnapshotSlack;
        taskStatuses.reset(new (std::nothrow) TaskStatus_t[capacity]);
        if (!taskStatuses) {
            ESP_LOGE(kTag, "Failed to allocate health task snapshot");
            break;
        }

        got = uxTaskGetSystemState(taskStatuses.get(), capacity, nullptr);
        if (got == 0) {
            vTaskDelay(1);
        }
    }

    if (got > 0) {
        // Put the tasks closest to stack exhaustion first so the bounded
        // response retains the most actionable entries.
        std::sort(taskStatuses.get(), taskStatuses.get() + got,
                  [](const TaskStatus_t& left, const TaskStatus_t& right) {
                      if (left.usStackHighWaterMark != right.usStackHighWaterMark) {
                          return left.usStackHighWaterMark < right.usStackHighWaterMark;
                      }
                      return left.uxCurrentPriority > right.uxCurrentPriority;
                  });
    } else if (taskStatuses) {
        ESP_LOGW(kTag, "Unable to capture a stable FreeRTOS task snapshot");
    }

    resp.tasks_count = 0;
    for (UBaseType_t i = 0; i < got && resp.tasks_count < 16; ++i) {
        auto& t = resp.tasks[resp.tasks_count];
        strncpy(t.name, taskStatuses[i].pcTaskName, sizeof(t.name) - 1);
        t.stack_high_water = taskStatuses[i].usStackHighWaterMark;
        t.priority = taskStatuses[i].uxCurrentPriority;
#if defined(configTASKLIST_INCLUDE_COREID) && (configTASKLIST_INCLUDE_COREID == 1)
        const BaseType_t coreId = taskStatuses[i].xCoreID;
        t.core = coreId == tskNO_AFFINITY ? 0xFF : static_cast<uint32_t>(coreId);
#elif defined(configUSE_CORE_AFFINITY) && (configUSE_CORE_AFFINITY == 1) && \
    (configNUMBER_OF_CORES > 1)
        // Convert affinity mask to core number (0=core0, 1=core1, 0xFF=any)
        auto mask = taskStatuses[i].uxCoreAffinityMask;
        if (mask == 0x01)
            t.core = 0;
        else if (mask == 0x02)
            t.core = 1;
        else
            t.core = 0xFF;  // tskNO_AFFINITY or both cores
#else
        t.core = 0;
#endif
        resp.tasks_count++;
    }

    if (got > resp.tasks_count) {
        ESP_LOGW(kTag, "Health response retained %u of %u tasks by lowest stack headroom",
                 static_cast<unsigned>(resp.tasks_count), static_cast<unsigned>(got));
    }

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_GetHealthResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetHealthResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetHealthResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetHealthRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleGetEspNowStatus() {
    domes_config_GetEspNowStatusResponse resp = domes_config_GetEspNowStatusResponse_init_zero;

    if (espNowTransport_) {
        resp.peer_count = espNowTransport_->getPeerCount();
        resp.tx_count = espNowTransport_->getTxCount();
        resp.rx_count = espNowTransport_->getRxCount();
        resp.tx_fail_count = espNowTransport_->getTxFailCount();
    }

    // WiFi channel
    uint8_t primary = 0;
    wifi_second_chan_t secondary;
    if (esp_wifi_get_channel(&primary, &secondary) == ESP_OK) {
        resp.channel = primary;
    }

    if (espNowService_) {
        resp.last_rtt_us = espNowService_->lastRttUs();
        strncpy(resp.discovery_state, espNowService_->discoveryState(),
                sizeof(resp.discovery_state) - 1);

        // Copy peer info
        DiscoveredPeer peers[8];
        uint8_t count = espNowService_->getPeers(peers, 8);
        resp.peers_count = count;

        int64_t nowUs = esp_timer_get_time();
        for (uint8_t i = 0; i < count; ++i) {
            auto& p = resp.peers[i];
            p.mac.size = 6;
            std::memcpy(p.mac.bytes, peers[i].mac, 6);
            p.rssi = peers[i].hasRssi ? peers[i].rssi : 0;
            // Convert last seen from absolute us to relative ms
            if (peers[i].lastSeenUs > 0) {
                p.last_seen_ms = static_cast<uint32_t>((nowUs - peers[i].lastSeenUs) / 1000);
            }
        }
    } else {
        strncpy(resp.discovery_state, "disabled", sizeof(resp.discovery_state) - 1);
    }

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_GetEspNowStatusResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetEspNowStatusResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetEspNowStatusResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetEspNowStatusRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleEspNowBench(const uint8_t* payload, size_t len) {
    if (!espNowService_) {
        ESP_LOGW(kTag, "ESP-NOW service not available for benchmark");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kEspNowBenchRsp, errPayload.data(), 1);
        return;
    }

    // Decode request
    domes_config_EspNowBenchRequest req = domes_config_EspNowBenchRequest_init_zero;
    if (len > 0) {
        pb_istream_t stream = pb_istream_from_buffer(payload, len);
        if (!pb_decode(&stream, domes_config_EspNowBenchRequest_fields, &req)) {
            ESP_LOGW(kTag, "Failed to decode ESPNOW_BENCH: %s", PB_GET_ERROR(&stream));
        }
    }
    uint32_t rounds = (req.rounds > 0) ? req.rounds : 100;

    ESP_LOGI(kTag, "Starting ESP-NOW benchmark: %lu rounds", static_cast<unsigned long>(rounds));

    // Start benchmark
    if (!espNowService_->startBenchmark(rounds)) {
        ESP_LOGW(kTag, "Failed to start benchmark (no peer or already running)");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kBusy);
        sendFrame(MsgType::kEspNowBenchRsp, errPayload.data(), 1);
        return;
    }

    // Poll beyond the service's 45-second deadline so cancellation has time to settle.
    static constexpr uint32_t kBenchPollMs = 50;
    static constexpr uint32_t kBenchTimeoutMs = 60000;
    uint32_t waited = 0;
    while (!espNowService_->isBenchmarkDone() && waited < kBenchTimeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(kBenchPollMs));
        waited += kBenchPollMs;
    }

    if (!espNowService_->isBenchmarkDone()) {
        ESP_LOGW(kTag, "Benchmark timed out after %lums", static_cast<unsigned long>(waited));
        espNowService_->cancelBenchmark();
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kEspNowBenchRsp, errPayload.data(), 1);
        return;
    }

    // Get results
    BenchmarkResult benchResult;
    if (!espNowService_->takeBenchmarkResult(benchResult)) {
        ESP_LOGW(kTag, "Benchmark result was not available");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kEspNowBenchRsp, errPayload.data(), 1);
        return;
    }

    domes_config_EspNowBenchResponse resp = domes_config_EspNowBenchResponse_init_zero;
    resp.rounds_completed = benchResult.roundsCompleted;
    resp.rounds_failed = benchResult.roundsFailed;
    resp.min_rtt_us = benchResult.minRttUs;
    resp.max_rtt_us = benchResult.maxRttUs;
    resp.mean_rtt_us = benchResult.meanRttUs;
    resp.p50_rtt_us = benchResult.p50RttUs;
    resp.p95_rtt_us = benchResult.p95RttUs;
    resp.p99_rtt_us = benchResult.p99RttUs;

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_EspNowBenchResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_EspNowBenchResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode EspNowBenchResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kEspNowBenchRsp, respPayload.data(), 1 + ostream.bytes_written);
}

// ============================================================================
// Clean-restart snapshot handlers (legacy crash-dump protocol names)
// ============================================================================

void ConfigCommandHandler::handleGetCrashDump() {
    domes_config_CrashDumpResponse resp = domes_config_CrashDumpResponse_init_zero;

    infra::CrashDumpData dump;
    const esp_err_t loadErr = infra::ShutdownDumpHandler::loadDump(dump);
    Status status = Status::kOk;
    if (loadErr == ESP_OK) {
        resp.has_dump = true;
        std::strncpy(resp.reason, dump.reason, sizeof(resp.reason) - 1);
        std::strncpy(resp.task_name, dump.taskName, sizeof(resp.task_name) - 1);
        resp.uptime_s = dump.uptimeS;
        resp.free_heap = dump.freeHeap;
        resp.boot_count = dump.bootCount;
        std::strncpy(resp.firmware_version, dump.firmwareVersion,
                     sizeof(resp.firmware_version) - 1);
        resp.format_version = dump.formatVersion;
        std::strncpy(resp.elf_sha256, dump.elfSha256, sizeof(resp.elf_sha256) - 1);

        resp.backtrace_count = dump.backtraceDepth;
        for (uint8_t i = 0; i < dump.backtraceDepth && i < 16; ++i) {
            resp.backtrace[i] = dump.backtrace[i];
        }
    } else if (loadErr == ESP_ERR_NOT_FOUND) {
        resp.has_dump = false;
    } else {
        ESP_LOGE(kTag, "Failed to load restart snapshot: %s", esp_err_to_name(loadErr));
        status = Status::kError;
    }

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_CrashDumpResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(status);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_CrashDumpResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode CrashDumpResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetCrashDumpRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleClearCrashDump() {
    esp_err_t err = infra::ShutdownDumpHandler::clearDump();

    domes_config_ClearCrashDumpResponse resp = domes_config_ClearCrashDumpResponse_init_zero;
    resp.cleared = (err == ESP_OK);

    std::array<uint8_t, domes_config_ClearCrashDumpResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(err == ESP_OK ? Status::kOk : Status::kError);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_ClearCrashDumpResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode ClearCrashDumpResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kClearCrashDumpRsp, payload.data(), 1 + stream.bytes_written);
}

// ============================================================================
// Memory profile handler
// ============================================================================

void ConfigCommandHandler::handleGetMemoryProfile() {
    domes_config_GetMemoryProfileResponse resp = domes_config_GetMemoryProfileResponse_init_zero;

    // Current stats
    auto current = infra::MemoryProfiler::currentStats();
    resp.current_free_heap = current.freeHeap;
    resp.current_min_free_heap = current.minFreeHeap;
    resp.current_largest_block = current.largestBlock;
    resp.total_heap = infra::MemoryProfiler::totalHeapSize();

    // Historical samples
    std::array<infra::HeapSample, memory_profile::kMaxSamples> samples;
    size_t count = infra::MemoryProfiler::getSamples(samples.data(), samples.size());

    resp.samples_count = static_cast<pb_size_t>(count);
    for (size_t i = 0; i < count; ++i) {
        resp.samples[i].timestamp_s = samples[i].timestampS;
        resp.samples[i].free_heap = samples[i].freeHeap;
        resp.samples[i].largest_block = samples[i].largestBlock;
        resp.samples[i].min_free_heap = samples[i].minFreeHeap;
    }

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes::kMaxPayloadSize> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_GetMemoryProfileResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode GetMemoryProfileResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kGetMemoryProfileRsp, payload.data(), 1 + stream.bytes_written);
}

// ============================================================================
// Self-test handler
// ============================================================================

void ConfigCommandHandler::handleSelfTest() {
    ESP_LOGI(kTag, "Running self-test suite...");

    domes_config_SelfTestResponse resp;
    domes::infra::runSmokeTests(resp);

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_SelfTestResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_SelfTestResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SelfTestResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kSelfTestRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleCheckUpdate() {
    domes_config_CheckUpdateResponse resp = domes_config_CheckUpdateResponse_init_zero;

    // Read auto-update NVS setting
    infra::NvsConfig config;
#ifdef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (config.open(infra::nvs_ns::kConfig) == ESP_OK) {
        resp.auto_update_enabled =
            config.getOrDefault<uint8_t>(infra::config_key::kAutoUpdate, kAutoUpdateDefault) != 0;
        config.close();
    }
#else
    resp.auto_update_enabled = false;
#endif

    if (!otaManager_) {
        ESP_LOGW(kTag, "OTA manager not available");
        // Still return current version info
        strncpy(resp.current_version, infra::firmwareVersion(), sizeof(resp.current_version) - 1);

        std::array<uint8_t, domes_config_CheckUpdateResponse_size + 10> payload;
        payload[0] = static_cast<uint8_t>(Status::kError);

        pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
        if (pb_encode(&stream, domes_config_CheckUpdateResponse_fields, &resp)) {
            sendFrame(MsgType::kCheckUpdateRsp, payload.data(), 1 + stream.bytes_written);
        }
        return;
    }

    // Check for update (this queries GitHub API - may take a few seconds)
    OtaCheckResult result;
    esp_err_t err = otaManager_->checkForUpdate(result);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Update check failed: %s", esp_err_to_name(err));
        strncpy(resp.current_version, infra::firmwareVersion(), sizeof(resp.current_version) - 1);

        std::array<uint8_t, domes_config_CheckUpdateResponse_size + 10> payload;
        payload[0] = static_cast<uint8_t>(Status::kError);

        pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
        if (pb_encode(&stream, domes_config_CheckUpdateResponse_fields, &resp)) {
            sendFrame(MsgType::kCheckUpdateRsp, payload.data(), 1 + stream.bytes_written);
        }
        return;
    }

    resp.update_available = result.updateAvailable;

    // Format version strings
    snprintf(resp.current_version, sizeof(resp.current_version), "%lu.%lu.%lu",
             static_cast<unsigned long>(result.currentVersion.major),
             static_cast<unsigned long>(result.currentVersion.minor),
             static_cast<unsigned long>(result.currentVersion.patch));

    if (result.updateAvailable) {
        snprintf(resp.available_version, sizeof(resp.available_version), "%lu.%lu.%lu",
                 static_cast<unsigned long>(result.availableVersion.major),
                 static_cast<unsigned long>(result.availableVersion.minor),
                 static_cast<unsigned long>(result.availableVersion.patch));
        resp.firmware_size = static_cast<uint32_t>(result.firmwareSize);
    }

    // Encode: [status_byte][protobuf]
    std::array<uint8_t, domes_config_CheckUpdateResponse_size + 10> payload;
    payload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t stream = pb_ostream_from_buffer(payload.data() + 1, payload.size() - 1);
    if (!pb_encode(&stream, domes_config_CheckUpdateResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode CheckUpdateResponse: %s", PB_GET_ERROR(&stream));
        return;
    }

    sendFrame(MsgType::kCheckUpdateRsp, payload.data(), 1 + stream.bytes_written);
}

void ConfigCommandHandler::handleSetAutoUpdate(const uint8_t* payload, size_t len) {
    domes_config_SetAutoUpdateRequest req = domes_config_SetAutoUpdateRequest_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, len);

    if (!pb_decode(&stream, domes_config_SetAutoUpdateRequest_fields, &req)) {
        ESP_LOGW(kTag, "Failed to decode SET_AUTO_UPDATE: %s", PB_GET_ERROR(&stream));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetAutoUpdateRsp, errPayload.data(), 1);
        return;
    }

#ifndef CONFIG_DOMES_WIFI_AUTO_CONNECT
    if (req.enabled) {
        ESP_LOGW(kTag, "Auto-update requires a build with WiFi auto-connect support");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetAutoUpdateRsp, errPayload.data(), 1);
        return;
    }
#endif

    // Write to NVS
    infra::NvsConfig config;
    if (config.open(infra::nvs_ns::kConfig) != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS for auto_update write");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetAutoUpdateRsp, errPayload.data(), 1);
        return;
    }

    esp_err_t err = config.setU8(infra::config_key::kAutoUpdate, req.enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = config.commit();
    }
    config.close();

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to persist auto_update: %s", esp_err_to_name(err));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetAutoUpdateRsp, errPayload.data(), 1);
        return;
    }

    ESP_LOGI(kTag, "Auto-update %s", req.enabled ? "enabled" : "disabled");

    // Send response
    domes_config_SetAutoUpdateResponse resp = domes_config_SetAutoUpdateResponse_init_zero;
    resp.enabled = req.enabled;

    std::array<uint8_t, domes_config_SetAutoUpdateResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetAutoUpdateResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetAutoUpdateResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetAutoUpdateRsp, respPayload.data(), 1 + ostream.bytes_written);
}

// ============================================================================
// Touch injection handlers
// ============================================================================

void ConfigCommandHandler::handleSimulateTouch(const uint8_t* payload, size_t len) {
    domes_config_SimulateTouchRequest req = domes_config_SimulateTouchRequest_init_zero;
    if (len > 0) {
        pb_istream_t stream = pb_istream_from_buffer(payload, len);
        if (!pb_decode(&stream, domes_config_SimulateTouchRequest_fields, &req)) {
            ESP_LOGW(kTag, "Failed to decode SIMULATE_TOUCH: %s", PB_GET_ERROR(&stream));
            std::array<uint8_t, 1> errPayload;
            errPayload[0] = static_cast<uint8_t>(Status::kError);
            sendFrame(MsgType::kSimulateTouchRsp, errPayload.data(), 1);
            return;
        }
    }

    ESP_LOGI(kTag, "Injecting touch on pad %lu", static_cast<unsigned long>(req.pad_index));

    if (!injectableTouch_) {
        ESP_LOGW(kTag, "InjectableTouchDriver not available");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSimulateTouchRsp, errPayload.data(), 1);
        return;
    }

    if (req.pad_index >= injectableTouch_->getPadCount()) {
        ESP_LOGW(kTag, "Invalid simulated touch pad: %lu",
                 static_cast<unsigned long>(req.pad_index));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSimulateTouchRsp, errPayload.data(), 1);
        return;
    }

    injectableTouch_->injectTouch(static_cast<uint8_t>(req.pad_index));

    // Send OK response
    domes_config_SimulateTouchResponse resp = domes_config_SimulateTouchResponse_init_zero;

    std::array<uint8_t, domes_config_SimulateTouchResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SimulateTouchResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SimulateTouchResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSimulateTouchRsp, respPayload.data(), 1 + ostream.bytes_written);
}

void ConfigCommandHandler::handleSetSimMode(const uint8_t* payload, size_t len) {
    domes_config_SetSimModeRequest req = domes_config_SetSimModeRequest_init_zero;
    if (len > 0) {
        pb_istream_t stream = pb_istream_from_buffer(payload, len);
        if (!pb_decode(&stream, domes_config_SetSimModeRequest_fields, &req)) {
            ESP_LOGW(kTag, "Failed to decode SET_SIM_MODE: %s", PB_GET_ERROR(&stream));
            std::array<uint8_t, 1> errPayload;
            errPayload[0] = static_cast<uint8_t>(Status::kError);
            sendFrame(MsgType::kSetSimModeRsp, errPayload.data(), 1);
            return;
        }
    }

    ESP_LOGI(kTag, "Set sim mode: enabled=%d delay_ms=%lu pad=%lu", req.enabled,
             static_cast<unsigned long>(req.delay_ms), static_cast<unsigned long>(req.pad_index));

    if (!espNowService_) {
        ESP_LOGW(kTag, "ESP-NOW service not available for sim mode");
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetSimModeRsp, errPayload.data(), 1);
        return;
    }

    if (req.pad_index >= pins::kTouchPadCount || req.delay_ms > EspNowService::kMaxSimDelayMs) {
        ESP_LOGW(kTag, "Invalid sim mode parameters: delay_ms=%lu pad=%lu",
                 static_cast<unsigned long>(req.delay_ms),
                 static_cast<unsigned long>(req.pad_index));
        std::array<uint8_t, 1> errPayload;
        errPayload[0] = static_cast<uint8_t>(Status::kError);
        sendFrame(MsgType::kSetSimModeRsp, errPayload.data(), 1);
        return;
    }

    espNowService_->setSimMode(req.enabled, req.delay_ms, static_cast<uint8_t>(req.pad_index));

    // Send response echoing current state
    domes_config_SetSimModeResponse resp = domes_config_SetSimModeResponse_init_zero;
    resp.enabled = espNowService_->isSimMode();
    resp.delay_ms = espNowService_->simDelayMs();
    resp.pad_index = espNowService_->simPadIndex();

    std::array<uint8_t, domes_config_SetSimModeResponse_size + 10> respPayload;
    respPayload[0] = static_cast<uint8_t>(Status::kOk);

    pb_ostream_t ostream = pb_ostream_from_buffer(respPayload.data() + 1, respPayload.size() - 1);
    if (!pb_encode(&ostream, domes_config_SetSimModeResponse_fields, &resp)) {
        ESP_LOGE(kTag, "Failed to encode SetSimModeResponse: %s", PB_GET_ERROR(&ostream));
        return;
    }

    sendFrame(MsgType::kSetSimModeRsp, respPayload.data(), 1 + ostream.bytes_written);
}

}  // namespace domes::config
