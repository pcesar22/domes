#pragma once

/**
 * @file drv2605l.hpp
 * @brief DRV2605L haptic driver
 *
 * Controls the TI DRV2605L haptic driver via I2C.
 * Supports LRA (Linear Resonance Actuator) motors.
 *
 * Features:
 * - 123 built-in haptic effects
 * - Effect sequencing (up to 8 effects)
 * - Open-loop drive tuned for the NFF schematic's LD0832AA-0099F LRA
 * - Real-time playback mode
 */

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "interfaces/iHapticDriver.hpp"

#include <algorithm>
#include <cstdint>

namespace domes {

/**
 * @brief DRV2605L haptic driver
 *
 * @code
 * Drv2605lDriver haptic(i2cBus, 0x5A);
 * haptic.init();
 * haptic.playEffect(1);  // Strong click
 * @endcode
 */
class Drv2605lDriver : public IHapticDriver {
public:
    /**
     * @brief Construct DRV2605L driver
     *
     * @param i2cBus I2C master bus handle
     * @param addr I2C address (typically 0x5A)
     */
    Drv2605lDriver(i2c_master_bus_handle_t i2cBus, uint8_t addr)
        : i2cBus_(i2cBus), addr_(addr), devHandle_(nullptr), intensity_(100), initialized_(false) {}

    ~Drv2605lDriver() override {
        if (devHandle_) {
            i2c_master_bus_rm_device(devHandle_);
            devHandle_ = nullptr;
        }
    }

    // Non-copyable
    Drv2605lDriver(const Drv2605lDriver&) = delete;
    Drv2605lDriver& operator=(const Drv2605lDriver&) = delete;

    esp_err_t init() override {
        if (initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Add device to I2C bus
        i2c_device_config_t devConfig = {};
        devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        devConfig.device_address = addr_;
        devConfig.scl_speed_hz = kI2cFreqHz;

        esp_err_t err = i2c_master_bus_add_device(i2cBus_, &devConfig, &devHandle_);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to add I2C device: %s", esp_err_to_name(err));
            return err;
        }

        // Read status register to verify communication
        uint8_t status = 0;
        err = readReg(Reg::kStatus, &status);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to read status: %s", esp_err_to_name(err));
            i2c_master_bus_rm_device(devHandle_);
            devHandle_ = nullptr;
            return err;
        }
        ESP_LOGI(kTag, "DRV2605L status: 0x%02X", status);

        // Exit standby mode
        err = writeReg(Reg::kMode, Mode::kInternalTrigger);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to set mode: %s", esp_err_to_name(err));
            return err;
        }

        // The NFF design source specifies an LD0832AA-0099F rated for 1.8 Vrms at 235 Hz.
        // Keep this profile bounded and deterministic until it is verified on populated hardware.
        err = configureLra();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "LRA configuration failed: %s", esp_err_to_name(err));
            return err;
        }

        err = writeReg(Reg::kLibrarySelection, kLraLibrary);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to select library: %s", esp_err_to_name(err));
            return err;
        }

        initialized_ = true;
        ESP_LOGI(kTag, "DRV2605L initialized (addr=0x%02X, LRA open-loop at 236 Hz)", addr_);
        return ESP_OK;
    }

    esp_err_t playEffect(uint8_t effectId) override {
        if (!initialized_) {
            ESP_LOGE(kTag, "playEffect: not initialized");
            return ESP_ERR_INVALID_STATE;
        }
        if (effectId == 0 || effectId > kMaxEffectId) {
            ESP_LOGW(kTag, "Invalid effect ID: %u", effectId);
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI(kTag, "Playing effect %u...", effectId);

        // Set mode to internal trigger (clear standby)
        esp_err_t err = writeReg(Reg::kMode, Mode::kInternalTrigger);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to set mode: %s", esp_err_to_name(err));
            return err;
        }

        // Load effect into waveform register 0
        err = writeReg(Reg::kWaveformSeq0, effectId);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to write waveform: %s", esp_err_to_name(err));
            return err;
        }

        // End sequence marker
        err = writeReg(Reg::kWaveformSeq1, 0);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to write end marker: %s", esp_err_to_name(err));
            return err;
        }

        // Trigger playback
        err = writeReg(Reg::kGo, 1);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to trigger GO: %s", esp_err_to_name(err));
            return err;
        }

        // Verify GO bit was set
        uint8_t go = 0;
        readReg(Reg::kGo, &go);
        ESP_LOGI(kTag, "Effect %u triggered (GO=0x%02X)", effectId, go);

        return ESP_OK;
    }

    esp_err_t playSequence(const uint8_t* effectIds, size_t count) override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!effectIds || count == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        // Clamp to max sequence length
        count = std::min(count, kMaxSequenceLen);

        // Set mode to internal trigger
        esp_err_t err = writeReg(Reg::kMode, Mode::kInternalTrigger);
        if (err != ESP_OK)
            return err;

        // Load effects into waveform registers
        for (size_t i = 0; i < count; ++i) {
            err = writeReg(static_cast<Reg>(static_cast<uint8_t>(Reg::kWaveformSeq0) + i),
                           effectIds[i]);
            if (err != ESP_OK)
                return err;
        }

        // End sequence marker (if room)
        if (count < kMaxSequenceLen) {
            err = writeReg(static_cast<Reg>(static_cast<uint8_t>(Reg::kWaveformSeq0) + count), 0);
            if (err != ESP_OK)
                return err;
        }

        // Trigger playback
        err = writeReg(Reg::kGo, 1);
        if (err != ESP_OK)
            return err;

        ESP_LOGD(kTag, "Playing sequence of %zu effects", count);
        return ESP_OK;
    }

    esp_err_t stop() override {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        // Clear GO bit
        return writeReg(Reg::kGo, 0);
    }

    void setIntensity(uint8_t intensity) override {
        intensity_ = std::min(intensity, static_cast<uint8_t>(100));
        // Note: DRV2605L doesn't have direct intensity control
        // This would require modifying the overdrive voltage or
        // using real-time playback mode with scaled values
    }

    uint8_t getIntensity() const override { return intensity_; }

    bool isInitialized() const override { return initialized_; }

    bool isPlaying() const override {
        if (!initialized_)
            return false;

        uint8_t go = 0;
        if (readReg(Reg::kGo, &go) != ESP_OK)
            return false;
        return (go & 0x01) != 0;
    }

    /**
     * @brief Reject auto-calibration while using the fixed NFF actuator profile
     *
     * Calibration changes the operating model and requires motor-specific hardware validation.
     * This driver intentionally remains in its bounded open-loop profile until that work is done.
     *
     * @return ESP_ERR_NOT_SUPPORTED for this board configuration
     */
    esp_err_t runCalibration() {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGE(kTag, "Auto-calibration is unsupported by the fixed NFF LRA profile");
        return ESP_ERR_NOT_SUPPORTED;
    }

private:
    static constexpr const char* kTag = "drv2605l";
    static constexpr uint32_t kI2cFreqHz = 400000;  // 400 kHz
    static constexpr uint8_t kMaxEffectId = 123;
    static constexpr size_t kMaxSequenceLen = 8;
    static constexpr uint8_t kLraLibrary = 6;
    // 235 Hz period / 98.46 us per register step = 43.22, rounded to 43 (236.20 Hz).
    static constexpr uint8_t kLraOpenLoopPeriod235Hz = 43;
    // Datasheet equation 7 gives 1.786 Vrms at 236.20 Hz for OD_CLAMP=93.
    static constexpr uint8_t kLraOverdriveClamp1_8Vrms = 93;

    // Register addresses
    enum class Reg : uint8_t {
        kStatus = 0x00,
        kMode = 0x01,
        kRtpInput = 0x02,
        kLibrarySelection = 0x03,
        kWaveformSeq0 = 0x04,
        kWaveformSeq1 = 0x05,
        kWaveformSeq2 = 0x06,
        kWaveformSeq3 = 0x07,
        kWaveformSeq4 = 0x08,
        kWaveformSeq5 = 0x09,
        kWaveformSeq6 = 0x0A,
        kWaveformSeq7 = 0x0B,
        kGo = 0x0C,
        kOverdriveTimeOffset = 0x0D,
        kSustainPosOffset = 0x0E,
        kSustainNegOffset = 0x0F,
        kBrakeTimeOffset = 0x10,
        kAudioToVibeCtrl = 0x11,
        kAudioToVibeMinInput = 0x12,
        kAudioToVibeMaxInput = 0x13,
        kAudioToVibeMinOutput = 0x14,
        kAudioToVibeMaxOutput = 0x15,
        kRatedVoltage = 0x16,
        kOverdriveClampVoltage = 0x17,
        kAutoCalCompResult = 0x18,
        kAutoCalBackEmfResult = 0x19,
        kFeedbackControl = 0x1A,
        kControl1 = 0x1B,
        kControl2 = 0x1C,
        kControl3 = 0x1D,
        kControl4 = 0x1E,
        kControl5 = 0x1F,
        kLraOpenLoopPeriod = 0x20,
        kVbatVoltageMonitor = 0x21,
        kLraResonancePeriod = 0x22,
    };

    // Mode register values
    struct Mode {
        static constexpr uint8_t kInternalTrigger = 0x00;
        static constexpr uint8_t kExternalTriggerEdge = 0x01;
        static constexpr uint8_t kExternalTriggerLevel = 0x02;
        static constexpr uint8_t kPwmAnalogInput = 0x03;
        static constexpr uint8_t kAudioToVibe = 0x04;
        static constexpr uint8_t kRealTimePlayback = 0x05;
        static constexpr uint8_t kDiagnostics = 0x06;
        static constexpr uint8_t kStandby = 0x40;
    };

    esp_err_t configureLra() {
        esp_err_t err;

        // In LRA open-loop mode OD_CLAMP is the full-scale reference. This value follows
        // DRV2605L equation 7 for the actuator's 1.8 Vrms rating at 235 Hz.
        err = writeReg(Reg::kOverdriveClampVoltage, kLraOverdriveClamp1_8Vrms);
        if (err != ESP_OK)
            return err;

        // Select LRA mode while preserving the datasheet defaults for the feedback fields.
        err = writeReg(Reg::kFeedbackControl, 0xB6);
        if (err != ESP_OK)
            return err;

        // Half-period at 235 Hz is 2.13 ms. DRIVE_TIME=16 selects 2.1 ms.
        err = writeReg(Reg::kControl1, 0x90);
        if (err != ESP_OK)
            return err;

        err = writeReg(Reg::kLraOpenLoopPeriod, kLraOpenLoopPeriod235Hz);
        if (err != ESP_OK)
            return err;

        // Preserve the default noise gate and enable LRA open-loop mode (bit 0).
        err = writeReg(Reg::kControl3, 0xA1);
        if (err != ESP_OK)
            return err;

        ESP_LOGI(kTag, "Configured LD0832AA-0099F LRA (1.79 Vrms, 236.20 Hz open-loop)");
        return ESP_OK;
    }

    esp_err_t writeReg(Reg reg, uint8_t value) const {
        uint8_t data[2] = {static_cast<uint8_t>(reg), value};
        return i2c_master_transmit(devHandle_, data, sizeof(data), kI2cTimeoutMs);
    }

    esp_err_t readReg(Reg reg, uint8_t* value) const {
        uint8_t regAddr = static_cast<uint8_t>(reg);
        return i2c_master_transmit_receive(devHandle_, &regAddr, 1, value, 1, kI2cTimeoutMs);
    }

    static constexpr int kI2cTimeoutMs = 100;

    i2c_master_bus_handle_t i2cBus_;
    uint8_t addr_;
    i2c_master_dev_handle_t devHandle_;
    uint8_t intensity_;
    bool initialized_;
};

// Common DRV2605L effect IDs for convenience
namespace HapticEffect {
constexpr uint8_t kStrongClick100 = 1;
constexpr uint8_t kStrongClick60 = 2;
constexpr uint8_t kStrongClick30 = 3;
constexpr uint8_t kSharpClick100 = 4;
constexpr uint8_t kSharpClick60 = 5;
constexpr uint8_t kSharpClick30 = 6;
constexpr uint8_t kSoftBump100 = 7;
constexpr uint8_t kSoftBump60 = 8;
constexpr uint8_t kSoftBump30 = 9;
constexpr uint8_t kDoubleClick100 = 10;
constexpr uint8_t kDoubleClick60 = 11;
constexpr uint8_t kTripleClick = 12;
constexpr uint8_t kSoftFuzz60 = 13;
constexpr uint8_t kStrongBuzz100 = 14;
constexpr uint8_t kAlert750ms = 15;
constexpr uint8_t kAlert1000ms = 16;
constexpr uint8_t kStrongClick1_100 = 17;
constexpr uint8_t kStrongClick2_80 = 18;
constexpr uint8_t kStrongClick3_60 = 19;
constexpr uint8_t kStrongClick4_30 = 20;
constexpr uint8_t kMediumClick1_100 = 21;
constexpr uint8_t kMediumClick2_80 = 22;
constexpr uint8_t kMediumClick3_60 = 23;
constexpr uint8_t kSharpTick1_100 = 24;
constexpr uint8_t kSharpTick2_80 = 25;
constexpr uint8_t kSharpTick3_60 = 26;
constexpr uint8_t kLongBuzz = 47;
constexpr uint8_t kBuzzShort = 49;
constexpr uint8_t kPulsing1 = 52;
constexpr uint8_t kPulsing2 = 58;
constexpr uint8_t kTransitionClick = 64;
constexpr uint8_t kTransitionHum = 70;
constexpr uint8_t kRampUp = 82;
constexpr uint8_t kRampDown = 86;
}  // namespace HapticEffect

}  // namespace domes
