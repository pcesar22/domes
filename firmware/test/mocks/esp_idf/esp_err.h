/**
 * @file esp_err.h
 * @brief Mock ESP-IDF error codes for host testing
 */

#ifndef ESP_ERR_H
#define ESP_ERR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t esp_err_t;

/* Common error codes */
#define ESP_OK          0       /*!< Success */
#define ESP_FAIL        -1      /*!< Generic failure */

#define ESP_ERR_NO_MEM              0x101   /*!< Out of memory */
#define ESP_ERR_INVALID_ARG         0x102   /*!< Invalid argument */
#define ESP_ERR_INVALID_STATE       0x103   /*!< Invalid state */
#define ESP_ERR_INVALID_SIZE        0x104   /*!< Invalid size */
#define ESP_ERR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define ESP_ERR_NOT_SUPPORTED       0x106   /*!< Operation not supported */
#define ESP_ERR_TIMEOUT             0x107   /*!< Operation timed out */
#define ESP_ERR_INVALID_RESPONSE    0x108   /*!< Received response invalid */
#define ESP_ERR_INVALID_CRC         0x109   /*!< CRC or checksum invalid */
#define ESP_ERR_INVALID_VERSION     0x10A   /*!< Version invalid */
#define ESP_ERR_INVALID_MAC         0x10B   /*!< MAC address invalid */
#define ESP_ERR_NOT_FINISHED        0x10C   /*!< Operation not finished yet */

/* Mock function for error name lookup */
static inline const char* esp_err_to_name(esp_err_t code) {
    switch (code) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
        case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        default: return "UNKNOWN_ERROR";
    }
}

/* Error checking macros */
#define ESP_ERROR_CHECK(x) do { \
    esp_err_t __err = (x); \
    (void)__err; \
} while(0)

#define ESP_RETURN_ON_ERROR(x, tag, msg) do { \
    esp_err_t __err = (x); \
    if (__err != ESP_OK) { \
        return __err; \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* ESP_ERR_H */
