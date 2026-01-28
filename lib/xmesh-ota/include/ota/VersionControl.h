/**
 * @file VersionControl.h
 * @brief Firmware version management for xMESH OTA
 * 
 * Handles semantic versioning comparison and tracking for OTA updates.
 */

#pragma once

#include <Arduino.h>

namespace xmesh {
namespace ota {

/**
 * @brief Semantic version structure (major.minor.patch)
 */
struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    Version() : major(0), minor(0), patch(0) {}
    Version(uint8_t maj, uint8_t min, uint8_t pat) 
        : major(maj), minor(min), patch(pat) {}

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(const Version& other) const {
        return !(*this == other);
    }

    bool operator>(const Version& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch > other.patch;
    }

    bool operator<(const Version& other) const {
        return other > *this;
    }

    bool operator>=(const Version& other) const {
        return !(*this < other);
    }

    bool operator<=(const Version& other) const {
        return !(*this > other);
    }

    String toString() const {
        return String(major) + "." + String(minor) + "." + String(patch);
    }

    static Version fromString(const char* str) {
        Version v;
        if (str && sscanf(str, "%hhu.%hhu.%hhu", &v.major, &v.minor, &v.patch) == 3) {
            return v;
        }
        return Version();
    }
};

/**
 * @brief Version control manager
 */
class VersionControl {
public:
    /**
     * @brief Get current firmware version
     * @return Current version from app descriptor
     */
    static Version getCurrentVersion();

    /**
     * @brief Get available update version (if any)
     * @return Available version from update server
     */
    static Version getAvailableVersion();

    /**
     * @brief Set available version (called after checking server)
     * @param version Available version
     */
    static void setAvailableVersion(const Version& version);

    /**
     * @brief Compare two versions
     * @param v1 First version
     * @param v2 Second version
     * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
     */
    static int compareVersions(const Version& v1, const Version& v2);

    /**
     * @brief Check if update is available and newer
     * @return true if available version > current version
     */
    static bool isUpdateAvailable();

private:
    static Version available_version_;
};

} // namespace ota
} // namespace xmesh
