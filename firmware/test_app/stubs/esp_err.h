/**
 * @file esp_err.h
 * @brief Minimal ESP-IDF error type stubs for host unit tests
 */
#pragma once

#include <cstdint>

typedef int32_t esp_err_t;

#define ESP_OK          0
#define ESP_FAIL        (-1)
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_INVALID_SIZE    0x104
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_TIMEOUT         0x107
