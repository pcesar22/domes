#pragma once

/**
 * @file mockTransport.hpp
 * @brief Mock transport for unit testing
 *
 * Provides controllable transport behavior for testing protocol
 * code that depends on communication.
 */

#include "interfaces/iTransport.hpp"

#include <cstring>
#include <deque>
#include <vector>

namespace domes {

/**
 * @brief Mock transport for unit testing
 *
 * Allows tests to inject data and verify sent data.
 *
 * @code
 * MockTransport mockTransport;
 *
 * // Inject data to be received
 * mockTransport.injectReceiveData({0x01, 0x02, 0x03});
 *
 * Protocol protocol(mockTransport);
 * protocol.readFrame();
 *
 * // Verify sent data
 * TEST_ASSERT_EQUAL_MEMORY(expected, mockTransport.sentData.data(), 3);
 * @endcode
 */
class MockTransport : public ITransport {
public:
    MockTransport() {
        reset();
    }

    /**
     * @brief Reset all mock state
     */
    void reset() {
        initCalled = false;
        sendCalled = false;
        receiveCalled = false;
        disconnectCalled = false;
        flushCalled = false;

        initReturnValue = TransportError::kOk;
        sendReturnValue = TransportError::kOk;
        receiveReturnValue = TransportError::kOk;
        flushReturnValue = TransportError::kOk;

        connected_ = false;
        sendCount = 0;
        receiveCount = 0;

        sentData.clear();
        receiveQueue.clear();
    }

    // ITransport implementation
    TransportError init() override {
        initCalled = true;
        if (initReturnValue == TransportError::kOk) {
            connected_ = true;
        }
        return initReturnValue;
    }

    TransportError send(const uint8_t* data, size_t len) override {
        sendCalled = true;
        sendCount++;

        if (!data || len == 0) {
            return TransportError::kInvalidArg;
        }
        if (!connected_) {
            return TransportError::kNotInitialized;
        }

        if (sendReturnValue == TransportError::kOk) {
            sentData.insert(sentData.end(), data, data + len);
        }
        return sendReturnValue;
    }

    TransportError receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override {
        receiveCalled = true;
        receiveCount++;
        lastReceiveTimeout = timeoutMs;

        if (!buf || !len || *len == 0) {
            return TransportError::kInvalidArg;
        }
        if (!connected_) {
            return TransportError::kNotInitialized;
        }

        if (receiveReturnValue != TransportError::kOk) {
            return receiveReturnValue;
        }

        if (receiveQueue.empty()) {
            *len = 0;
            return TransportError::kTimeout;
        }

        // Copy from receive queue
        size_t toCopy = std::min(*len, receiveQueue.size());
        for (size_t i = 0; i < toCopy; ++i) {
            buf[i] = receiveQueue.front();
            receiveQueue.pop_front();
        }
        *len = toCopy;

        return TransportError::kOk;
    }

    bool isConnected() const override {
        return connected_;
    }

    void disconnect() override {
        disconnectCalled = true;
        connected_ = false;
    }

    TransportError flush() override {
        flushCalled = true;
        return flushReturnValue;
    }

    size_t available() const override {
        return receiveQueue.size();
    }

    // Test control methods

    /**
     * @brief Inject data to be received by the mock
     */
    void injectReceiveData(const std::vector<uint8_t>& data) {
        receiveQueue.insert(receiveQueue.end(), data.begin(), data.end());
    }

    /**
     * @brief Inject data to be received by the mock
     */
    void injectReceiveData(const uint8_t* data, size_t len) {
        receiveQueue.insert(receiveQueue.end(), data, data + len);
    }

    /**
     * @brief Clear any pending receive data
     */
    void clearReceiveQueue() {
        receiveQueue.clear();
    }

    /**
     * @brief Clear sent data buffer
     */
    void clearSentData() {
        sentData.clear();
    }

    /**
     * @brief Set connected state directly
     */
    void setConnected(bool connected) {
        connected_ = connected;
    }

    // Test inspection - method calls
    bool initCalled;
    bool sendCalled;
    bool receiveCalled;
    bool disconnectCalled;
    bool flushCalled;
    uint32_t sendCount;
    uint32_t receiveCount;

    // Test control - return values
    TransportError initReturnValue;
    TransportError sendReturnValue;
    TransportError receiveReturnValue;
    TransportError flushReturnValue;

    // Test inspection - captured data
    std::vector<uint8_t> sentData;
    uint32_t lastReceiveTimeout = 0;

private:
    bool connected_ = false;
    std::deque<uint8_t> receiveQueue;
};

}  // namespace domes
