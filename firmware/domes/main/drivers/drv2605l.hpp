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
 * - Auto-calibration for LRA motors
 * - Real-time playback mode
 */

#include "interfaces/iHapticDriver.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
        : i2cBus_(i2cBus),
          addr_(addr),
          devHandle_(nullptr),
          intensity_(100),
          initialized_(false) {}

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

        // Configure for ERM motor (more common, more forgiving)
        err = configureErm();
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "ERM configuration failed: %s", esp_err_to_name(err));
            return err;
        }

        // Select ROM library 7 (ERM 4.5V/5V - strongest effects)
        err = writeReg(Reg::kLibrarySelection, kErmLibraryStrong);
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "Failed to select library: %s", esp_err_to_name(err));
            return err;
        }

        initialized_ = true;
        ESP_LOGI(kTag, "DRV2605L initialized (addr=0x%02X, ERM mode)", addr_);
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
        if (err != ESP_OK) return err;

        // Load effects into waveform registers
        for (size_t i = 0; i < count; ++i) {
            err = writeReg(static_cast<Reg>(static_cast<uint8_t>(Reg::kWaveformSeq0) + i),
                          effectIds[i]);
            if (err != ESP_OK) return err;
        }

        // End sequence marker (if room)
        if (count < kMaxSequenceLen) {
            err = writeReg(static_cast<Reg>(static_cast<uint8_t>(Reg::kWaveformSeq0) + count), 0);
            if (err != ESP_OK) return err;
        }

        // Trigger playback
        err = writeReg(Reg::kGo, 1);
        if (err != ESP_OK) return err;

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
        if (!initialized_) return false;

        uint8_t go = 0;
        if (readReg(Reg::kGo, &go) != ESP_OK) return false;
        return (go & 0x01) != 0;
    }

    /**
     * @brief Run auto-calibration for LRA motor
     *
     * Should be run once with motor mounted in final enclosure.
     * Calibration values are NOT persisted - call this at init if needed.
     *
     * @return ESP_OK on success
     */
    esp_err_t runCalibration() {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(kTag, "Starting LRA auto-calibration...");

        // Enter calibration mode
        esp_err_t err = writeReg(Reg::kMode, Mode::kAutoCalibration);
        if (err != ESP_OK) return err;

        // Trigger calibration
        err = writeReg(Reg::kGo, 1);
        if (err != ESP_OK) return err;

        // Wait for calibration to complete (typically 1-2 seconds)
        for (int i = 0; i < 30; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
            uint8_t go = 0;
            err = readReg(Reg::kGo, &go);
            if (err != ESP_OK) return err;
            if ((go & 0x01) == 0) break;
        }

        // Check calibration result
        uint8_t status = 0;
        err = readReg(Reg::kStatus, &status);
        if (err != ESP_OK) return err;

        if (status & 0x08) {  // DIAG_RESULT bit
            ESP_LOGE(kTag, "Auto-calibration failed (status=0x%02X)", status);
            return ESP_FAIL;
        }

        // Read calibration results
        uint8_t compensation = 0, backEmf = 0, backEmfGain = 0;
        readReg(Reg::kAutoCalCompResult, &compensation);
        readReg(Reg::kAutoCalBackEmfResult, &backEmf);
        readReg(Reg::kFeedbackControl, &backEmfGain);

        ESP_LOGI(kTag, "Calibration complete: comp=0x%02X, backEMF=0x%02X, gain=0x%02X",
                 compensation, backEmf, backEmfGain & 0x03);

        // Return to internal trigger mode
        err = writeReg(Reg::kMode, Mode::kInternalTrigger);
        return err;
    }

private:
    static constexpr const char* kTag = "drv2605l";
    static constexpr uint32_t kI2cFreqHz = 400000;  // 400 kHz
    static constexpr uint8_t kMaxEffectId = 123;
    static constexpr size_t kMaxSequenceLen = 8;
    // Library selection (see datasheet Table 1):
    // 1 = Empty, 2 = ERM 1.3V/3V, 3-5 = ERM 3V/3V, 6 = LRA, 7 = ERM 4.5V/5V (strongest)
    static constexpr uint8_t kErmLibraryWeak = 2;    // ERM 1.3V rated, 3V overdrive
    static constexpr uint8_t kErmLibraryMedium = 3;  // ERM 3V rated, 3V overdrive
    static constexpr uint8_t kErmLibraryStrong = 7;  // ERM 4.5V rated, 5V overdrive (max!)
    static constexpr uint8_t kLraLibrary = 6;        // LRA library

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
        static constexpr uint8_t kAutoCalibration = 0x07;
        static constexpr uint8_t kStandby = 0x40;
    };

    esp_err_t configureErm() {
        esp_err_t err;

        // Voltage formulas from datasheet:
        // Rated: V = value * 5.44V / 255   (0xD3 = 4.5V for library 7)
        // OD:    V = value * 5.6V / 255    (0xE4 = 5.0V for library 7)
        // Using max values (0xFF) for strongest possible drive

        // Set rated voltage to max (5.44V)
        err = writeReg(Reg::kRatedVoltage, 0xFF);
        if (err != ESP_OK) return err;

        // Set overdrive clamp to max (5.6V)
        err = writeReg(Reg::kOverdriveClampVoltage, 0xFF);
        if (err != ESP_OK) return err;

        // Configure feedback control for ERM
        // Bit 7 (N_ERM_LRA) = 0 (ERM mode)
        // Bits 6:4 (FB_BRAKE_FACTOR) = 010 (2x brake for snappier stop)
        // Bits 3:2 (LOOP_GAIN) = 10 (medium gain)
        // Bits 1:0 (BEMF_GAIN) = 11 (highest for ERM)
        err = writeReg(Reg::kFeedbackControl, 0x2B);  // 0010 1011
        if (err != ESP_OK) return err;

        // Control1: Drive time and AC couple
        // Bit 7 = STARTUP_BOOST (1 = enable for faster start)
        // Bits 6:5 = reserved
        // Bit 4 = AC_COUPLE (0 = DC coupled)
        // Bits 3:0 = DRIVE_TIME (max for ERM)
        err = writeReg(Reg::kControl1, 0x93);
        if (err != ESP_OK) return err;

        // Control2: Sample time, blanking, IDISS
        err = writeReg(Reg::kControl2, 0xF5);
        if (err != ESP_OK) return err;

        // Control3: ERM open loop mode
        // Bit 5 = ERM_OPEN_LOOP (1 = open loop for simpler operation)
        err = writeReg(Reg::kControl3, 0x20);
        if (err != ESP_OK) return err;

        ESP_LOGI(kTag, "Configured for ERM (library 7, max voltage, open-loop)");
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
