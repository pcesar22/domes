/**
 * @file test_trace_stream_writer.cpp
 * @brief Unit tests for complete trace-stream frame writes
 */

#include "trace/traceStreamWriter.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

using domes::trace::detail::sendAll;
using domes::trace::detail::SendAllResult;

constexpr std::array<uint8_t, 7> kPayload = {0xAA, 0x55, 0x04, 0x00, 0x10, 0x20, 0x30};

TEST(TraceStreamWriter, SendsWholeBuffer) {
    std::vector<uint8_t> written;

    auto result =
        sendAll(kPayload.data(), kPayload.size(), [&written](const uint8_t* data, size_t len) {
            written.insert(written.end(), data, data + len);
            return static_cast<std::ptrdiff_t>(len);
        });

    EXPECT_EQ(result, SendAllResult::kOk);
    EXPECT_TRUE(std::equal(written.begin(), written.end(), kPayload.begin(), kPayload.end()));
}

TEST(TraceStreamWriter, RetriesUntilAllPartialWritesComplete) {
    std::vector<uint8_t> written;
    std::vector<size_t> requestedLengths;

    auto result = sendAll(kPayload.data(), kPayload.size(),
                          [&written, &requestedLengths](const uint8_t* data, size_t len) {
                              requestedLengths.push_back(len);
                              const size_t chunk = std::min<size_t>(2, len);
                              written.insert(written.end(), data, data + chunk);
                              return static_cast<std::ptrdiff_t>(chunk);
                          });

    EXPECT_EQ(result, SendAllResult::kOk);
    EXPECT_EQ(requestedLengths, (std::vector<size_t>{7, 5, 3, 1}));
    EXPECT_TRUE(std::equal(written.begin(), written.end(), kPayload.begin(), kPayload.end()));
}

TEST(TraceStreamWriter, ReportsClosedConnectionMidFrame) {
    size_t calls = 0;

    auto result = sendAll(
        kPayload.data(), kPayload.size(),
        [&calls](const uint8_t*, size_t) -> std::ptrdiff_t { return calls++ == 0 ? 3 : 0; });

    EXPECT_EQ(result, SendAllResult::kClosed);
    EXPECT_EQ(calls, 2u);
}

TEST(TraceStreamWriter, ReportsWriteErrorMidFrame) {
    size_t calls = 0;

    auto result = sendAll(
        kPayload.data(), kPayload.size(),
        [&calls](const uint8_t*, size_t) -> std::ptrdiff_t { return calls++ == 0 ? 3 : -1; });

    EXPECT_EQ(result, SendAllResult::kError);
    EXPECT_EQ(calls, 2u);
}

TEST(TraceStreamWriter, RejectsInvalidWriterProgress) {
    auto result = sendAll(kPayload.data(), kPayload.size(), [](const uint8_t*, size_t len) {
        return static_cast<std::ptrdiff_t>(len + 1);
    });

    EXPECT_EQ(result, SendAllResult::kError);
}

TEST(TraceStreamWriter, AcceptsEmptyBufferWithoutWriting) {
    bool called = false;

    auto result = sendAll(nullptr, 0, [&called](const uint8_t*, size_t) {
        called = true;
        return std::ptrdiff_t{0};
    });

    EXPECT_EQ(result, SendAllResult::kOk);
    EXPECT_FALSE(called);
}

}  // namespace
