/**
 * @file test_main_standalone.cpp
 * @brief Standalone Unity test runner (no ESP-IDF)
 *
 * This file provides the entry point for running unit tests
 * without ESP-IDF dependencies.
 */

#include "unity.h"

#include <cstdio>

// Unity setup/teardown - called before/after each test
void setUp() {}
void tearDown() {}

// External test declarations - Unity doesn't auto-register in standalone mode
// We need to manually declare and run them

// CRC32 tests
extern "C" void test_crc32_empty_buffer_returns_0x00000000(void);
extern "C" void test_crc32_of_123456789_matches_IEEE_802_3_check_value(void);
extern "C" void test_crc32_single_byte_0x00(void);
extern "C" void test_crc32_single_byte_0xFF(void);
extern "C" void test_crc32_all_zeros_4_bytes(void);
extern "C" void test_crc32_all_ones_4_bytes(void);
extern "C" void test_crc32_incremental_matches_single_shot(void);
extern "C" void test_crc32_incremental_byte_by_byte_matches_single_shot(void);
extern "C" void test_crc32_different_data_produces_different_CRC(void);
extern "C" void test_crc32_detects_single_bit_flip(void);
extern "C" void test_crc32_large_buffer(void);

// Frame codec tests
extern "C" void test_encodeFrame_minimal_frame_no_payload(void);
extern "C" void test_encodeFrame_with_payload(void);
extern "C" void test_encodeFrame_buffer_too_small_returns_error(void);
extern "C" void test_encodeFrame_payload_too_large_returns_error(void);
extern "C" void test_encodeFrame_max_payload_size_succeeds(void);
extern "C" void test_FrameDecoder_initial_state_is_kWaitStart0(void);
extern "C" void test_FrameDecoder_detects_start_bytes(void);
extern "C" void test_FrameDecoder_handles_0xAA_0xAA_0x55_sequence(void);
extern "C" void test_FrameDecoder_rejects_non_start_byte_after_0xAA(void);
extern "C" void test_FrameDecoder_rejects_zero_length(void);
extern "C" void test_FrameDecoder_rejects_oversized_length(void);
extern "C" void test_FrameDecoder_reset_clears_state(void);
extern "C" void test_Frame_round_trip_encode_then_decode(void);
extern "C" void test_Frame_round_trip_no_payload(void);
extern "C" void test_Frame_round_trip_max_payload(void);
extern "C" void test_FrameDecoder_detects_CRC_mismatch(void);
extern "C" void test_FrameDecoder_detects_payload_corruption(void);
extern "C" void test_FrameDecoder_handles_garbage_before_sync(void);
extern "C" void test_FrameDecoder_requires_reset_after_complete(void);

// OTA protocol tests
extern "C" void test_OTA_BEGIN_serialize_deserialize_round_trip(void);
extern "C" void test_OTA_BEGIN_serialize_with_null_SHA256(void);
extern "C" void test_OTA_BEGIN_serialize_with_null_version(void);
extern "C" void test_OTA_BEGIN_buffer_too_small_returns_error(void);
extern "C" void test_OTA_BEGIN_deserialize_with_short_payload_returns_error(void);
extern "C" void test_OTA_DATA_serialize_deserialize_round_trip(void);
extern "C" void test_OTA_DATA_serialize_empty_payload(void);
extern "C" void test_OTA_DATA_serialize_max_chunk_size(void);
extern "C" void test_OTA_DATA_serialize_oversized_chunk_returns_error(void);
extern "C" void test_OTA_DATA_deserialize_with_truncated_payload_returns_error(void);
extern "C" void test_OTA_END_serialize_produces_zero_length_payload(void);
extern "C" void test_OTA_END_serialize_with_null_outLen_returns_error(void);
extern "C" void test_OTA_ACK_serialize_deserialize_round_trip(void);
extern "C" void test_OTA_ACK_all_status_codes(void);
extern "C" void test_OTA_ACK_buffer_too_small_returns_error(void);
extern "C" void test_OTA_ABORT_serialize_deserialize_round_trip(void);
extern "C" void test_OTA_ABORT_deserialize_with_null_reason_returns_error(void);
extern "C" void test_OTA_ABORT_deserialize_with_empty_payload_returns_error(void);
extern "C" void test_All_serializers_reject_null_buffer(void);
extern "C" void test_All_serializers_reject_null_outLen(void);
extern "C" void test_All_deserializers_reject_null_required_outputs(void);

// Version parser tests
extern "C" void test_parseVersion_handles_simple_version(void);
extern "C" void test_parseVersion_handles_version_without_v_prefix(void);
extern "C" void test_parseVersion_handles_dirty_flag(void);
extern "C" void test_parseVersion_handles_git_describe_output(void);
extern "C" void test_parseVersion_handles_git_describe_with_dirty(void);
extern "C" void test_parseVersion_handles_zero_version(void);
extern "C" void test_parseVersion_handles_large_version_numbers(void);
extern "C" void test_parseVersion_handles_null_input(void);
extern "C" void test_parseVersion_handles_empty_string(void);
extern "C" void test_parseVersion_handles_invalid_format(void);
extern "C" void test_FirmwareVersion_compare_equal_versions(void);
extern "C" void test_FirmwareVersion_compare_major_difference(void);
extern "C" void test_FirmwareVersion_compare_minor_difference(void);
extern "C" void test_FirmwareVersion_compare_patch_difference(void);
extern "C" void test_FirmwareVersion_isUpdateAvailable(void);
extern "C" void test_FirmwareVersion_compare_ignores_dirty_flag(void);
extern "C" void test_FirmwareVersion_compare_ignores_git_hash(void);

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("========================================\n");
    printf("DOMES Firmware Unit Tests (Standalone)\n");
    printf("========================================\n");
    printf("\n");

    UNITY_BEGIN();

    // CRC32 tests
    printf("\n--- CRC32 Tests ---\n");
    RUN_TEST(test_crc32_empty_buffer_returns_0x00000000);
    RUN_TEST(test_crc32_of_123456789_matches_IEEE_802_3_check_value);
    RUN_TEST(test_crc32_single_byte_0x00);
    RUN_TEST(test_crc32_single_byte_0xFF);
    RUN_TEST(test_crc32_all_zeros_4_bytes);
    RUN_TEST(test_crc32_all_ones_4_bytes);
    RUN_TEST(test_crc32_incremental_matches_single_shot);
    RUN_TEST(test_crc32_incremental_byte_by_byte_matches_single_shot);
    RUN_TEST(test_crc32_different_data_produces_different_CRC);
    RUN_TEST(test_crc32_detects_single_bit_flip);
    RUN_TEST(test_crc32_large_buffer);

    // Frame codec tests
    printf("\n--- Frame Codec Tests ---\n");
    RUN_TEST(test_encodeFrame_minimal_frame_no_payload);
    RUN_TEST(test_encodeFrame_with_payload);
    RUN_TEST(test_encodeFrame_buffer_too_small_returns_error);
    RUN_TEST(test_encodeFrame_payload_too_large_returns_error);
    RUN_TEST(test_encodeFrame_max_payload_size_succeeds);
    RUN_TEST(test_FrameDecoder_initial_state_is_kWaitStart0);
    RUN_TEST(test_FrameDecoder_detects_start_bytes);
    RUN_TEST(test_FrameDecoder_handles_0xAA_0xAA_0x55_sequence);
    RUN_TEST(test_FrameDecoder_rejects_non_start_byte_after_0xAA);
    RUN_TEST(test_FrameDecoder_rejects_zero_length);
    RUN_TEST(test_FrameDecoder_rejects_oversized_length);
    RUN_TEST(test_FrameDecoder_reset_clears_state);
    RUN_TEST(test_Frame_round_trip_encode_then_decode);
    RUN_TEST(test_Frame_round_trip_no_payload);
    RUN_TEST(test_Frame_round_trip_max_payload);
    RUN_TEST(test_FrameDecoder_detects_CRC_mismatch);
    RUN_TEST(test_FrameDecoder_detects_payload_corruption);
    RUN_TEST(test_FrameDecoder_handles_garbage_before_sync);
    RUN_TEST(test_FrameDecoder_requires_reset_after_complete);

    // OTA protocol tests
    printf("\n--- OTA Protocol Tests ---\n");
    RUN_TEST(test_OTA_BEGIN_serialize_deserialize_round_trip);
    RUN_TEST(test_OTA_BEGIN_serialize_with_null_SHA256);
    RUN_TEST(test_OTA_BEGIN_serialize_with_null_version);
    RUN_TEST(test_OTA_BEGIN_buffer_too_small_returns_error);
    RUN_TEST(test_OTA_BEGIN_deserialize_with_short_payload_returns_error);
    RUN_TEST(test_OTA_DATA_serialize_deserialize_round_trip);
    RUN_TEST(test_OTA_DATA_serialize_empty_payload);
    RUN_TEST(test_OTA_DATA_serialize_max_chunk_size);
    RUN_TEST(test_OTA_DATA_serialize_oversized_chunk_returns_error);
    RUN_TEST(test_OTA_DATA_deserialize_with_truncated_payload_returns_error);
    RUN_TEST(test_OTA_END_serialize_produces_zero_length_payload);
    RUN_TEST(test_OTA_END_serialize_with_null_outLen_returns_error);
    RUN_TEST(test_OTA_ACK_serialize_deserialize_round_trip);
    RUN_TEST(test_OTA_ACK_all_status_codes);
    RUN_TEST(test_OTA_ACK_buffer_too_small_returns_error);
    RUN_TEST(test_OTA_ABORT_serialize_deserialize_round_trip);
    RUN_TEST(test_OTA_ABORT_deserialize_with_null_reason_returns_error);
    RUN_TEST(test_OTA_ABORT_deserialize_with_empty_payload_returns_error);
    RUN_TEST(test_All_serializers_reject_null_buffer);
    RUN_TEST(test_All_serializers_reject_null_outLen);
    RUN_TEST(test_All_deserializers_reject_null_required_outputs);

    // Version parser tests
    printf("\n--- Version Parser Tests ---\n");
    RUN_TEST(test_parseVersion_handles_simple_version);
    RUN_TEST(test_parseVersion_handles_version_without_v_prefix);
    RUN_TEST(test_parseVersion_handles_dirty_flag);
    RUN_TEST(test_parseVersion_handles_git_describe_output);
    RUN_TEST(test_parseVersion_handles_git_describe_with_dirty);
    RUN_TEST(test_parseVersion_handles_zero_version);
    RUN_TEST(test_parseVersion_handles_large_version_numbers);
    RUN_TEST(test_parseVersion_handles_null_input);
    RUN_TEST(test_parseVersion_handles_empty_string);
    RUN_TEST(test_parseVersion_handles_invalid_format);
    RUN_TEST(test_FirmwareVersion_compare_equal_versions);
    RUN_TEST(test_FirmwareVersion_compare_major_difference);
    RUN_TEST(test_FirmwareVersion_compare_minor_difference);
    RUN_TEST(test_FirmwareVersion_compare_patch_difference);
    RUN_TEST(test_FirmwareVersion_isUpdateAvailable);
    RUN_TEST(test_FirmwareVersion_compare_ignores_dirty_flag);
    RUN_TEST(test_FirmwareVersion_compare_ignores_git_hash);

    int failures = UNITY_END();

    printf("\n");
    if (failures == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("%d test(s) FAILED!\n", failures);
    }

    return failures;
}
