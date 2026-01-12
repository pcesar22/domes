#pragma once

/**
 * @file mockTaskRunner.hpp
 * @brief Mock task runner for unit testing
 *
 * Provides controllable task behavior for testing services
 * that use task runners.
 */

#include "interfaces/iTaskRunner.hpp"

namespace domes {

/**
 * @brief Mock task runner for unit testing
 *
 * Allows tests to control task execution and verify lifecycle.
 *
 * @code
 * MockTaskRunner mockTask;
 *
 * TaskManager manager;
 * manager.createTask(mockTask, config);
 *
 * TEST_ASSERT_TRUE(mockTask.runCalled);
 * @endcode
 */
class MockTaskRunner : public ITaskRunner {
public:
    MockTaskRunner() {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        runCalled = false;
        requestStopCalled = false;

        requestStopReturnValue = ESP_OK;

        running_ = true;
        runCount = 0;
    }

    // ITaskRunner implementation
    void run() override {
        runCalled = true;
        runCount++;

        // Simulate work if callback is set
        if (runCallback) {
            runCallback();
        }
    }

    esp_err_t requestStop() override {
        requestStopCalled = true;
        if (requestStopReturnValue == ESP_OK) {
            running_ = false;
        }
        return requestStopReturnValue;
    }

    bool shouldRun() const override {
        return running_;
    }

    // Test control methods

    /**
     * @brief Set whether task should continue running
     */
    void setRunning(bool running) {
        running_ = running;
    }

    /**
     * @brief Set callback to execute during run()
     */
    void setRunCallback(std::function<void()> callback) {
        runCallback = std::move(callback);
    }

    // Test inspection - method calls
    bool runCalled;
    bool requestStopCalled;
    uint32_t runCount;

    // Test control - return values
    esp_err_t requestStopReturnValue;

    // Test control - behavior
    std::function<void()> runCallback;

private:
    volatile bool running_ = true;
};

}  // namespace domes
