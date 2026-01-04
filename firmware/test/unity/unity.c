/* Unity Test Framework - Minimal Implementation
 * Based on ThrowTheSwitch/Unity
 */

#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

/* Internal state */
static struct {
    const char* current_file;
    const char* current_test;
    int current_line;
    int test_count;
    int test_failures;
    int test_ignores;
    jmp_buf abort_jmp;
} Unity;

void UnityBegin(const char* filename) {
    Unity.current_file = filename;
    Unity.current_test = NULL;
    Unity.current_line = 0;
    Unity.test_count = 0;
    Unity.test_failures = 0;
    Unity.test_ignores = 0;
    printf("\n-----------------------\n");
    printf("DOMES Firmware Tests\n");
    printf("-----------------------\n\n");
}

int UnityEnd(void) {
    printf("\n-----------------------\n");
    printf("%d Tests %d Failures %d Ignored\n",
           Unity.test_count, Unity.test_failures, Unity.test_ignores);

    if (Unity.test_failures == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    printf("-----------------------\n");

    return Unity.test_failures;
}

static void UnityPrintFail(void) {
    printf("FAIL\n");
    printf("  %s:%d: %s\n", Unity.current_file, Unity.current_line, Unity.current_test);
}

void UnityDefaultTestRun(void (*func)(void), const char* name, int line) {
    Unity.current_test = name;
    Unity.current_line = line;
    Unity.test_count++;

    printf("  %s... ", name);
    fflush(stdout);

    if (setjmp(Unity.abort_jmp) == 0) {
        func();
        printf("PASS\n");
    }
}

static void UnityAbort(void) {
    Unity.test_failures++;
    longjmp(Unity.abort_jmp, 1);
}

void UnityAssertTrue(int condition, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (!condition) {
        UnityPrintFail();
        printf("    %s\n", msg);
        UnityAbort();
    }
}

void UnityAssertFalse(int condition, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (condition) {
        UnityPrintFail();
        printf("    %s\n", msg);
        UnityAbort();
    }
}

void UnityAssertNull(const void* pointer, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (pointer != NULL) {
        UnityPrintFail();
        printf("    %s (pointer was %p)\n", msg, pointer);
        UnityAbort();
    }
}

void UnityAssertNotNull(const void* pointer, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (pointer == NULL) {
        UnityPrintFail();
        printf("    %s\n", msg);
        UnityAbort();
    }
}

void UnityAssertEqualNumber(long long expected, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (expected != actual) {
        UnityPrintFail();
        printf("    Expected: %lld, Actual: %lld", expected, actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertNotEqual(long long expected, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (expected == actual) {
        UnityPrintFail();
        printf("    Values should not be equal: %lld", actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertGreaterThan(long long threshold, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (actual <= threshold) {
        UnityPrintFail();
        printf("    Expected > %lld, Actual: %lld", threshold, actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertLessThan(long long threshold, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (actual >= threshold) {
        UnityPrintFail();
        printf("    Expected < %lld, Actual: %lld", threshold, actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertGreaterOrEqual(long long threshold, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (actual < threshold) {
        UnityPrintFail();
        printf("    Expected >= %lld, Actual: %lld", threshold, actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertLessOrEqual(long long threshold, long long actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (actual > threshold) {
        UnityPrintFail();
        printf("    Expected <= %lld, Actual: %lld", threshold, actual);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertEqualString(const char* expected, const char* actual, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (expected == NULL && actual == NULL) return;
    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0) {
        UnityPrintFail();
        printf("    Expected: \"%s\", Actual: \"%s\"",
               expected ? expected : "NULL",
               actual ? actual : "NULL");
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityAssertEqualMemory(const void* expected, const void* actual, size_t len, const char* msg, unsigned int line) {
    Unity.current_line = line;
    if (memcmp(expected, actual, len) != 0) {
        UnityPrintFail();
        printf("    Memory mismatch at byte %zu", len);
        if (msg) printf(" (%s)", msg);
        printf("\n");
        UnityAbort();
    }
}

void UnityFail(const char* msg, unsigned int line) {
    Unity.current_line = line;
    UnityPrintFail();
    printf("    %s\n", msg);
    UnityAbort();
}
