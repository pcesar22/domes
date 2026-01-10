/**
 * @file simpleOtaSender.cpp
 * @brief CLI tool to send OTA updates to ESP32 via serial
 *
 * Usage: simple_ota_sender <port> <firmware.bin> [version]
 *
 * Example:
 *   ./simple_ota_sender /dev/ttyACM0 domes.bin v1.2.3
 */

#include "transport/serialTransport.hpp"
#include "protocol/otaProtocol.hpp"
#include "protocol/frameCodec.hpp"
#include "utils/crc32.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

// SHA256 - use OpenSSL EVP API (OpenSSL 3.0+)
#include <openssl/evp.h>

using namespace domes;
using namespace domes::host;

namespace {

/**
 * @brief Read firmware file into memory
 */
bool readFirmwareFile(const char* path, std::vector<uint8_t>& data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "Error: Cannot open file '%s'\n", path);
        return false;
    }

    auto size = file.tellg();
    if (size <= 0) {
        std::fprintf(stderr, "Error: Empty or invalid file\n");
        return false;
    }

    data.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!file) {
        std::fprintf(stderr, "Error: Failed to read file\n");
        return false;
    }

    return true;
}

/**
 * @brief Compute SHA256 hash of data using OpenSSL EVP API
 */
void computeSha256(const uint8_t* data, size_t len, uint8_t* hash) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data, len);
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);
}

/**
 * @brief Send a framed message and wait for ACK
 */
TransportError sendAndWaitAck(
    ITransport& transport,
    OtaMsgType msgType,
    const uint8_t* payload,
    size_t payloadLen,
    OtaStatus* outStatus,
    uint32_t* outNextOffset,
    uint32_t timeoutMs = 5000
) {
    // Encode frame
    std::array<uint8_t, kMaxFrameSize> frameBuf{};
    size_t frameLen = 0;

    TransportError err = encodeFrame(
        static_cast<uint8_t>(msgType),
        payload,
        payloadLen,
        frameBuf.data(),
        frameBuf.size(),
        &frameLen
    );
    if (!isOk(err)) {
        return err;
    }

    // Send frame
    err = transport.send(frameBuf.data(), frameLen);
    if (!isOk(err)) {
        return err;
    }

    err = transport.flush();
    if (!isOk(err)) {
        return err;
    }

    // Wait for response
    FrameDecoder decoder;
    uint32_t elapsed = 0;
    constexpr uint32_t pollInterval = 10;  // ms

    while (elapsed < timeoutMs) {
        std::array<uint8_t, 256> rxBuf;
        size_t rxLen = rxBuf.size();

        err = transport.receive(rxBuf.data(), &rxLen, pollInterval);
        if (err == TransportError::kTimeout) {
            elapsed += pollInterval;
            continue;
        }
        if (!isOk(err)) {
            return err;
        }

        // Feed bytes to decoder
        for (size_t i = 0; i < rxLen; ++i) {
            decoder.feedByte(rxBuf[i]);

            if (decoder.isComplete()) {
                auto type = static_cast<OtaMsgType>(decoder.getType());

                if (type == OtaMsgType::kAck) {
                    return deserializeOtaAck(
                        decoder.getPayload(),
                        decoder.getPayloadLen(),
                        outStatus,
                        outNextOffset
                    );
                } else if (type == OtaMsgType::kAbort) {
                    OtaStatus reason;
                    err = deserializeOtaAbort(
                        decoder.getPayload(),
                        decoder.getPayloadLen(),
                        &reason
                    );
                    if (isOk(err)) {
                        *outStatus = reason;
                        *outNextOffset = 0;
                    }
                    return TransportError::kProtocolError;
                }

                decoder.reset();
            } else if (decoder.isError()) {
                decoder.reset();
            }
        }
    }

    return TransportError::kTimeout;
}

/**
 * @brief Print progress bar
 */
void printProgress(size_t current, size_t total) {
    constexpr int barWidth = 40;
    float progress = static_cast<float>(current) / static_cast<float>(total);
    int pos = static_cast<int>(barWidth * progress);

    std::printf("\r[");
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::printf("=");
        else if (i == pos) std::printf(">");
        else std::printf(" ");
    }
    std::printf("] %zu / %zu bytes (%.1f%%)", current, total, progress * 100.0f);
    std::fflush(stdout);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::printf("DOMES OTA Sender - Send firmware updates via serial\n\n");
        std::printf("Usage: %s <port> <firmware.bin> [version]\n\n", argv[0]);
        std::printf("Arguments:\n");
        std::printf("  port          Serial port (e.g., /dev/ttyACM0)\n");
        std::printf("  firmware.bin  Path to firmware binary\n");
        std::printf("  version       Optional version string (default: \"unknown\")\n");
        return 1;
    }

    const char* portPath = argv[1];
    const char* firmwarePath = argv[2];
    const char* version = argc > 3 ? argv[3] : "unknown";

    // Read firmware file
    std::printf("Reading firmware from '%s'...\n", firmwarePath);
    std::vector<uint8_t> firmware;
    if (!readFirmwareFile(firmwarePath, firmware)) {
        return 1;
    }
    std::printf("Firmware size: %zu bytes\n", firmware.size());

    // Compute SHA256
    std::printf("Computing SHA256...\n");
    std::array<uint8_t, kSha256Size> sha256;
    computeSha256(firmware.data(), firmware.size(), sha256.data());

    std::printf("SHA256: ");
    for (uint8_t byte : sha256) {
        std::printf("%02x", byte);
    }
    std::printf("\n");

    // Open serial port
    std::printf("Opening serial port '%s'...\n", portPath);
    SerialTransport transport(portPath, 115200);

    TransportError err = transport.init();
    if (!isOk(err)) {
        std::fprintf(stderr, "Error: Failed to open serial port: %s\n",
                     transportErrorToString(err));
        return 1;
    }
    std::printf("Serial port opened.\n");

    // Send OTA_BEGIN
    std::printf("Sending OTA_BEGIN (version: %s)...\n", version);

    // Buffer for OTA messages (OTA_DATA needs offset + len + data = 6 + kOtaChunkSize bytes)
    std::array<uint8_t, kOtaChunkSize + 64> payloadBuf;
    size_t payloadLen = 0;

    err = serializeOtaBegin(
        static_cast<uint32_t>(firmware.size()),
        sha256.data(),
        version,
        payloadBuf.data(),
        payloadBuf.size(),
        &payloadLen
    );
    if (!isOk(err)) {
        std::fprintf(stderr, "Error: Failed to serialize OTA_BEGIN\n");
        return 1;
    }

    OtaStatus status;
    uint32_t nextOffset;

    err = sendAndWaitAck(transport, OtaMsgType::kBegin, payloadBuf.data(), payloadLen, &status, &nextOffset);
    if (!isOk(err)) {
        std::fprintf(stderr, "Error: OTA_BEGIN failed: %s\n", transportErrorToString(err));
        return 1;
    }
    if (status != OtaStatus::kOk) {
        std::fprintf(stderr, "Error: Device rejected OTA_BEGIN: %s\n", otaStatusToString(status));
        return 1;
    }
    std::printf("Device accepted OTA_BEGIN.\n");

    // Send firmware chunks
    std::printf("Sending firmware data...\n");
    size_t offset = 0;

    while (offset < firmware.size()) {
        size_t chunkSize = std::min(kOtaChunkSize, firmware.size() - offset);

        err = serializeOtaData(
            static_cast<uint32_t>(offset),
            firmware.data() + offset,
            chunkSize,
            payloadBuf.data(),
            payloadBuf.size(),
            &payloadLen
        );
        if (!isOk(err)) {
            std::fprintf(stderr, "\nError: Failed to serialize OTA_DATA\n");
            return 1;
        }

        err = sendAndWaitAck(transport, OtaMsgType::kData, payloadBuf.data(), payloadLen, &status, &nextOffset);
        if (!isOk(err)) {
            std::fprintf(stderr, "\nError: OTA_DATA failed at offset %zu: %s\n",
                         offset, transportErrorToString(err));
            return 1;
        }
        if (status != OtaStatus::kOk) {
            std::fprintf(stderr, "\nError: Device rejected chunk at offset %zu: %s\n",
                         offset, otaStatusToString(status));
            return 1;
        }

        offset += chunkSize;
        printProgress(offset, firmware.size());
    }

    std::printf("\n");

    // Send OTA_END
    std::printf("Sending OTA_END...\n");
    err = serializeOtaEnd(payloadBuf.data(), payloadBuf.size(), &payloadLen);

    err = sendAndWaitAck(transport, OtaMsgType::kEnd, payloadBuf.data(), payloadLen, &status, &nextOffset, 30000);
    if (!isOk(err)) {
        std::fprintf(stderr, "Error: OTA_END failed: %s\n", transportErrorToString(err));
        return 1;
    }
    if (status != OtaStatus::kOk) {
        std::fprintf(stderr, "Error: Device rejected OTA_END: %s\n", otaStatusToString(status));
        return 1;
    }

    std::printf("\nOTA complete! Device will reboot.\n");

    transport.disconnect();
    return 0;
}
