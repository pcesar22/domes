#pragma once

#include "interfaces/iAudioDriver.hpp"
#include "sim/simLog.hpp"

namespace sim {

class SimAudioDriver : public domes::IAudioDriver {
public:
    SimAudioDriver(uint16_t podId, SimLog& log)
        : podId_(podId), log_(log) {}

    esp_err_t init() override {
        initialized_ = true;
        return ESP_OK;
    }

    esp_err_t start() override {
        started_ = true;
        startCount_++;
        log_.log(podId_, "audio", "start");
        return ESP_OK;
    }

    esp_err_t stop() override {
        started_ = false;
        stopCount_++;
        log_.log(podId_, "audio", "stop");
        return ESP_OK;
    }

    esp_err_t write(const int16_t* /*samples*/, size_t count,
                    size_t* written, uint32_t /*timeoutMs*/) override {
        if (written) *written = count;
        return ESP_OK;
    }

    void setVolume(uint8_t volume) override { volume_ = volume; }
    uint8_t getVolume() const override { return volume_; }
    bool isInitialized() const override { return initialized_; }
    bool isStarted() const override { return started_; }

    // Test accessors
    int startCount() const { return startCount_; }
    int stopCount() const { return stopCount_; }

private:
    uint16_t podId_;
    SimLog& log_;
    bool initialized_ = false;
    bool started_ = false;
    uint8_t volume_ = 100;
    int startCount_ = 0;
    int stopCount_ = 0;
};

}  // namespace sim
