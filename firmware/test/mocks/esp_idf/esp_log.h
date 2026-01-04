/**
 * @file esp_log.h
 * @brief Mock ESP-IDF logging for host testing
 */

#ifndef ESP_LOG_H
#define ESP_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

/* Logging macros - print to stdout in host mode */
#define ESP_LOGE(tag, format, ...) \
    printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)

#define ESP_LOGW(tag, format, ...) \
    printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)

#define ESP_LOGI(tag, format, ...) \
    printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)

#define ESP_LOGD(tag, format, ...) \
    printf("[D][%s] " format "\n", tag, ##__VA_ARGS__)

#define ESP_LOGV(tag, format, ...) \
    printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)

/* Level control - no-op in mock */
static inline void esp_log_level_set(const char* tag, esp_log_level_t level) {
    (void)tag;
    (void)level;
}

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_H */
