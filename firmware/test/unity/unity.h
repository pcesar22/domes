/* Unity Test Framework - Minimal Header
 * Based on ThrowTheSwitch/Unity
 * https://github.com/ThrowTheSwitch/Unity
 *
 * This is a minimal subset for DOMES firmware testing
 */

#ifndef UNITY_H
#define UNITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Test Control */
void UnityBegin(const char* filename);
int UnityEnd(void);
void UnityDefaultTestRun(void (*func)(void), const char* name, int line);

/* Assertions */
void UnityAssertTrue(int condition, const char* msg, unsigned int line);
void UnityAssertFalse(int condition, const char* msg, unsigned int line);
void UnityAssertNull(const void* pointer, const char* msg, unsigned int line);
void UnityAssertNotNull(const void* pointer, const char* msg, unsigned int line);
void UnityAssertEqualNumber(long long expected, long long actual, const char* msg, unsigned int line);
void UnityAssertNotEqual(long long expected, long long actual, const char* msg, unsigned int line);
void UnityAssertGreaterThan(long long threshold, long long actual, const char* msg, unsigned int line);
void UnityAssertLessThan(long long threshold, long long actual, const char* msg, unsigned int line);
void UnityAssertGreaterOrEqual(long long threshold, long long actual, const char* msg, unsigned int line);
void UnityAssertLessOrEqual(long long threshold, long long actual, const char* msg, unsigned int line);
void UnityAssertEqualString(const char* expected, const char* actual, const char* msg, unsigned int line);
void UnityAssertEqualMemory(const void* expected, const void* actual, size_t len, const char* msg, unsigned int line);
void UnityFail(const char* msg, unsigned int line);

/* Macros */
#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END() UnityEnd()

#define TEST_ASSERT(condition) \
    UnityAssertTrue((condition), "Expression was FALSE", __LINE__)

#define TEST_ASSERT_TRUE(condition) \
    UnityAssertTrue((condition), "Expected TRUE but was FALSE", __LINE__)

#define TEST_ASSERT_FALSE(condition) \
    UnityAssertFalse((condition), "Expected FALSE but was TRUE", __LINE__)

#define TEST_ASSERT_NULL(pointer) \
    UnityAssertNull((pointer), "Expected NULL", __LINE__)

#define TEST_ASSERT_NOT_NULL(pointer) \
    UnityAssertNotNull((pointer), "Expected Not NULL", __LINE__)

#define TEST_ASSERT_EQUAL(expected, actual) \
    UnityAssertEqualNumber((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    UnityAssertEqualNumber((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual) \
    UnityAssertEqualNumber((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) \
    UnityAssertEqualNumber((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) \
    UnityAssertEqualNumber((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    UnityAssertNotEqual((long long)(expected), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_GREATER_THAN(threshold, actual) \
    UnityAssertGreaterThan((long long)(threshold), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_LESS_THAN(threshold, actual) \
    UnityAssertLessThan((long long)(threshold), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual) \
    UnityAssertGreaterOrEqual((long long)(threshold), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual) \
    UnityAssertLessOrEqual((long long)(threshold), (long long)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    UnityAssertEqualString((expected), (actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    UnityAssertEqualMemory((expected), (actual), (len), NULL, __LINE__)

#define TEST_FAIL() \
    UnityFail("Test failed", __LINE__)

#define TEST_FAIL_MESSAGE(msg) \
    UnityFail((msg), __LINE__)

#define RUN_TEST(func) \
    UnityDefaultTestRun(func, #func, __LINE__)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
