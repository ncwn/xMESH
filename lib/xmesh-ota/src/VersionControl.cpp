#include "ota/VersionControl.h"
#include <esp_ota_ops.h>
#include <esp_app_format.h>

namespace xmesh {
namespace ota {

Version VersionControl::available_version_;

Version VersionControl::getCurrentVersion() {
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    if (app_desc) {
        return Version::fromString(app_desc->version);
    }
    return Version();
}

Version VersionControl::getAvailableVersion() {
    return available_version_;
}

void VersionControl::setAvailableVersion(const Version& version) {
    available_version_ = version;
}

int VersionControl::compareVersions(const Version& v1, const Version& v2) {
    if (v1 > v2) return 1;
    if (v1 < v2) return -1;
    return 0;
}

bool VersionControl::isUpdateAvailable() {
    return available_version_ > getCurrentVersion();
}

}
}
