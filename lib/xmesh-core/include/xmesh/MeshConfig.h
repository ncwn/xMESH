#ifndef XMESH_MESH_CONFIG_H
#define XMESH_MESH_CONFIG_H

#include <cstdint>

namespace xmesh {

/**
 * @brief Configuration constants for xMESH routing parameters (Trickle intervals, cost weights, etc.).
 * 
 * This struct centralizes routing parameters to keep xmesh-core hardware-agnostic.
 * Defaults are pulled from the production firmware config.
 */
struct MeshConfig {
    uint32_t trickle_i_min = 60000;
    uint32_t trickle_i_max = 600000;
    uint8_t trickle_k = 1;
    bool trickle_enabled = true;

    float w1_hop = 1.0f;
    float w2_rssi = 0.3f;
    float w3_snr = 0.2f;
    float w4_etx = 0.4f;
    float w5_gateway_bias = 1.0f;

    int16_t rssi_min = -120;
    int16_t rssi_max = -30;
    int8_t snr_min = -20;
    int8_t snr_max = 10;

    uint8_t etx_window_size = 10;
    float etx_default = 1.5f;
    float etx_alpha = 0.3f;

    uint32_t load_window_ms = 1000;
    float switch_threshold = 0.25f;
    uint8_t max_candidates = 10;

    uint32_t detection_ms = 360000;
    uint32_t warning_ms = 180000;

    static constexpr MeshConfig defaults() {
        return MeshConfig{};
    }
};

} // namespace xmesh

#endif // XMESH_MESH_CONFIG_H
