#pragma once

#include "config.pb.h"

#include "config/configProtocol.hpp"
#include "pb_decode.h"
#include "pb_encode.h"
#include "services/feedbackController.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace domes::config {

/** Decodes and routes the bounded feedback commands and builds status envelopes. */
class FeedbackCommandHandler {
public:
    static constexpr size_t kMaxPayloadSize = domes_config_GetAudioVolumeResponse_size +
                                              domes_config_SetAudioVolumeResponse_size +
                                              domes_config_TriggerFeedbackResponse_size + 1;

    struct Response {
        MsgType type = MsgType::kUnknown;
        std::array<uint8_t, kMaxPayloadSize> payload{};
        size_t length = 0;
    };

    explicit FeedbackCommandHandler(FeedbackController* controller) : controller_(controller) {}

    /** Return false only when the message type is outside this bounded command family. */
    bool handle(MsgType type, const uint8_t* payload, size_t length, Response& response) const {
        switch (type) {
            case MsgType::kGetAudioVolumeReq:
                return handleGetVolume(response);
            case MsgType::kSetAudioVolumeReq:
                return handleSetVolume(payload, length, response);
            case MsgType::kTriggerFeedbackReq:
                return handleTrigger(payload, length, response);
            default:
                return false;
        }
    }

private:
    static Status toStatus(FeedbackController::Result result) {
        switch (result) {
            case FeedbackController::Result::kOk:
                return Status::kOk;
            case FeedbackController::Result::kInvalid:
                return Status::kInvalidValue;
            case FeedbackController::Result::kDisabled:
                return Status::kDisabled;
            case FeedbackController::Result::kRejected:
                return Status::kRejected;
            case FeedbackController::Result::kStorageError:
                return Status::kStorageError;
            case FeedbackController::Result::kUnavailable:
            default:
                return Status::kError;
        }
    }

    static void beginResponse(Response& response, MsgType type, Status status) {
        response = {};
        response.type = type;
        response.payload[0] = static_cast<uint8_t>(status);
        response.length = 1;
    }

    static bool encodeVolume(Response& response, uint8_t volume, bool get) {
        pb_ostream_t stream =
            pb_ostream_from_buffer(response.payload.data() + 1, response.payload.size() - 1);
        if (get) {
            domes_config_GetAudioVolumeResponse body =
                domes_config_GetAudioVolumeResponse_init_zero;
            body.volume = volume;
            if (!pb_encode(&stream, domes_config_GetAudioVolumeResponse_fields, &body)) {
                return false;
            }
        } else {
            domes_config_SetAudioVolumeResponse body =
                domes_config_SetAudioVolumeResponse_init_zero;
            body.volume = volume;
            if (!pb_encode(&stream, domes_config_SetAudioVolumeResponse_fields, &body)) {
                return false;
            }
        }
        response.length = stream.bytes_written + 1;
        return true;
    }

    bool handleGetVolume(Response& response) const {
        uint8_t volume = 0;
        const auto result =
            controller_ ? controller_->getVolume(volume) : FeedbackController::Result::kUnavailable;
        beginResponse(response, MsgType::kGetAudioVolumeRsp, toStatus(result));
        return encodeVolume(response, volume, true);
    }

    bool handleSetVolume(const uint8_t* payload, size_t length, Response& response) const {
        domes_config_SetAudioVolumeRequest request = domes_config_SetAudioVolumeRequest_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(payload, length);
        uint8_t applied = 0;
        FeedbackController::Result result = FeedbackController::Result::kInvalid;
        if (pb_decode(&stream, domes_config_SetAudioVolumeRequest_fields, &request)) {
            result = controller_ ? controller_->setVolume(request.volume, applied)
                                 : FeedbackController::Result::kUnavailable;
        }
        beginResponse(response, MsgType::kSetAudioVolumeRsp, toStatus(result));
        return encodeVolume(response, applied, false);
    }

    bool handleTrigger(const uint8_t* payload, size_t length, Response& response) const {
        domes_config_TriggerFeedbackRequest request = domes_config_TriggerFeedbackRequest_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(payload, length);
        FeedbackProbe probe = FeedbackProbe::kUnknown;
        FeedbackController::ProbeResult result{FeedbackController::Result::kInvalid, false};
        if (pb_decode(&stream, domes_config_TriggerFeedbackRequest_fields, &request)) {
            probe = static_cast<FeedbackProbe>(request.probe);
            result = controller_ ? controller_->trigger(probe)
                                 : FeedbackController::ProbeResult{
                                       FeedbackController::Result::kUnavailable, false};
        }

        beginResponse(response, MsgType::kTriggerFeedbackRsp, toStatus(result.result));
        domes_config_TriggerFeedbackResponse body = domes_config_TriggerFeedbackResponse_init_zero;
        body.probe = static_cast<domes_config_FeedbackProbe>(probe);
        body.accepted = result.accepted;
        pb_ostream_t output =
            pb_ostream_from_buffer(response.payload.data() + 1, response.payload.size() - 1);
        if (!pb_encode(&output, domes_config_TriggerFeedbackResponse_fields, &body)) {
            return false;
        }
        response.length = output.bytes_written + 1;
        return true;
    }

    FeedbackController* controller_;
};

}  // namespace domes::config
