#pragma once

/**
 * @file traceStrings.hpp
 * @brief Compile-time string hashing for trace event names
 *
 * Provides the TRACE_ID() macro that converts string literals to
 * 32-bit hash values at compile time. This avoids storing strings
 * in trace events, reducing memory usage.
 *
 * The hash values can be mapped back to strings on the host side
 * using a generated name table.
 *
 * Usage:
 * @code
 * TRACE_BEGIN(TRACE_ID("WiFi.Connect"), TraceCategory::kWifi);
 * // ... WiFi connection code ...
 * TRACE_END(TRACE_ID("WiFi.Connect"), TraceCategory::kWifi);
 * @endcode
 */

#include <cstddef>
#include <cstdint>

namespace domes::trace {

/**
 * @brief FNV-1a hash algorithm (constexpr)
 *
 * Computes a 32-bit hash of a string at compile time.
 * FNV-1a is a simple, fast hash with good distribution.
 *
 * @param str Null-terminated string
 * @param len Length of string (excluding null terminator)
 * @return 32-bit hash value
 */
constexpr uint32_t fnv1aHash(const char* str, size_t len) {
    constexpr uint32_t kFnvPrime = 16777619u;
    constexpr uint32_t kFnvOffsetBasis = 2166136261u;

    uint32_t hash = kFnvOffsetBasis;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(str[i]));
        hash *= kFnvPrime;
    }
    return hash;
}

/**
 * @brief Compile-time string hash helper
 *
 * Uses template to get string length automatically.
 *
 * @tparam N Array size (includes null terminator)
 * @param str String literal
 * @return 32-bit hash value
 */
template <size_t N>
constexpr uint32_t traceId(const char (&str)[N]) {
    return fnv1aHash(str, N - 1);  // Exclude null terminator
}

}  // namespace domes::trace

/**
 * @brief Compile-time string to ID conversion
 *
 * Converts a string literal to a 32-bit hash at compile time.
 * Use this for span names, event names, and counter names.
 *
 * Example:
 * @code
 * TRACE_BEGIN(TRACE_ID("Game.ProcessInput"), TraceCategory::kGame);
 * @endcode
 *
 * @param str String literal (e.g., "Module.Operation")
 * @return 32-bit hash value (constexpr)
 */
#define TRACE_ID(str) (::domes::trace::traceId(str))
