#pragma once
#include "freertos/FreeRTOS.h"
#include <cstddef>

typedef void* RingbufHandle_t;

typedef enum {
    RINGBUF_TYPE_NOSPLIT = 0,
    RINGBUF_TYPE_ALLOWSPLIT,
    RINGBUF_TYPE_BYTEBUF,
    RINGBUF_TYPE_MAX,
} RingbufferType_t;

// These are declared but never called — hostTraceBuffer.cpp provides the real TraceBuffer implementation
inline RingbufHandle_t xRingbufferCreate(size_t, RingbufferType_t) { return nullptr; }
inline void vRingbufferDelete(RingbufHandle_t) {}
inline BaseType_t xRingbufferSend(RingbufHandle_t, const void*, size_t, TickType_t) { return pdFALSE; }
inline BaseType_t xRingbufferSendFromISR(RingbufHandle_t, const void*, size_t, BaseType_t*) { return pdFALSE; }
inline void* xRingbufferReceive(RingbufHandle_t, size_t*, TickType_t) { return nullptr; }
inline void vRingbufferReturnItem(RingbufHandle_t, void*) {}
inline size_t xRingbufferGetCurFreeSize(RingbufHandle_t) { return 0; }
