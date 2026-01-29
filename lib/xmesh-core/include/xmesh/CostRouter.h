#ifndef XMESH_COST_ROUTER_H
#define XMESH_COST_ROUTER_H

#include <cstdint>

namespace xmesh {

/**
 * @brief Multi-metric route cost calculation combining RSSI, SNR, ETX, hop count, and gateway bias.
 * 
 * Cost formula:
 * cost = W1*hops + W2*(1-norm_RSSI) + W3*(1-norm_SNR) + W4*(ETX-1) + W5*gateway_bias
 * 
 * Where:
 * - hops: Number of hops to destination
 * - norm_RSSI: Normalized RSSI [0,1], 1=best, 0=worst
 * - norm_SNR: Normalized SNR [0,1], 1=best, 0=worst
 * - ETX: Expected Transmission Count (1.0 = perfect link)
 * - gateway_bias: Load balancing factor (positive = penalty, negative = bonus)
 */
class CostRouter {
public:
    /**
     * @brief Construct a new CostRouter with default weights
     */
    CostRouter();

    /**
     * @brief Construct a new CostRouter with custom weights
     * @param w1 Weight for hop count (default: 1.0)
     * @param w2 Weight for RSSI (default: 0.3)
     * @param w3 Weight for SNR (default: 0.2)
     * @param w4 Weight for ETX (default: 0.4)
     * @param w5 Weight for gateway bias (default: 1.0)
     */
    CostRouter(float w1, float w2, float w3, float w4, float w5);

    /**
     * @brief Calculate route cost using multi-metric formula
     * @param hops Number of hops to destination
     * @param nextHop Next hop address
     * @param destAddr Destination address
     * @param rssi RSSI value in dBm
     * @param snr SNR value in dB
     * @param etx Expected Transmission Count
     * @param gatewayBias Load balancing bias factor
     * @return Combined cost value (lower is better)
     */
    float calculateCost(uint8_t hops, uint16_t nextHop, uint16_t destAddr,
                       int16_t rssi, int8_t snr, float etx, float gatewayBias) const;

    /**
     * @brief Normalize RSSI to [0, 1] range
     * @param rssi RSSI value in dBm
     * @return Normalized value (1.0 = best, 0.0 = worst)
     */
    static float normalizeRSSI(int16_t rssi);

    /**
     * @brief Normalize SNR to [0, 1] range
     * @param snr SNR value in dB
     * @return Normalized value (1.0 = best, 0.0 = worst)
     */
    static float normalizeSNR(int8_t snr);

    // Getters for weights
    float getW1() const { return w1_hopCount; }
    float getW2() const { return w2_rssi; }
    float getW3() const { return w3_snr; }
    float getW4() const { return w4_etx; }
    float getW5() const { return w5_gatewayBias; }

private:
    // Cost function weights (from config.h)
    float w1_hopCount;      // Weight for hop count (default: 1.0)
    float w2_rssi;          // Weight for RSSI (default: 0.3)
    float w3_snr;           // Weight for SNR (default: 0.2)
    float w4_etx;           // Weight for ETX (default: 0.4)
    float w5_gatewayBias;   // Weight for gateway bias (default: 1.0)

    // RSSI/SNR normalization ranges (from config.h)
    static constexpr int16_t RSSI_MIN = -120;
    static constexpr int16_t RSSI_MAX = -30;
    static constexpr int8_t SNR_MIN = -20;
    static constexpr int8_t SNR_MAX = 10;
};

} // namespace xmesh

#endif // XMESH_COST_ROUTER_H
