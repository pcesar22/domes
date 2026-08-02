#pragma once

#include <cstdint>

namespace domes {

/** Parsed firmware version from a release tag or `git describe`. */
struct FirmwareVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    char gitHash[41] = {};
    bool dirty = false;
    bool valid = false;

    /** Compare semantic-version core fields. Invalid versions sort before valid versions. */
    int compare(const FirmwareVersion& other) const;

    /** Return true only when both versions are valid and `other` is newer. */
    bool isUpdateAvailable(const FirmwareVersion& other) const;
};

/**
 * Parse `vMAJOR.MINOR.PATCH`, with optional `-dirty` or
 * `-COMMITS-gHEX[-dirty]` suffixes emitted by `git describe`.
 */
FirmwareVersion parseVersion(const char* versionString);

/** Return true when two parser-valid firmware version strings are byte-for-byte equal. */
bool firmwareVersionsMatchExactly(const char* declaredVersion, const char* embeddedVersion);

}  // namespace domes
