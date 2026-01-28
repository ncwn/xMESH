#include "xmesh/CostRouter.h"
#include <algorithm>
#include <esp_log.h>

static const char* TAG = "COST";

namespace xmesh {

CostRouter::CostRouter()
    : w1_hopCount(1.0f),
      w2_rssi(0.3f),
      w3_snr(0.2f),
      w4_etx(0.4f),
      w5_gatewayBias(1.0f) {}

CostRouter::CostRouter(float w1, float w2, float w3, float w4, float w5)
    : w1_hopCount(w1),
      w2_rssi(w2),
      w3_snr(w3),
      w4_etx(w4),
      w5_gatewayBias(w5) {
    if (w1 < 0.0f || w2 < 0.0f || w3 < 0.0f || w4 < 0.0f || w5 < 0.0f) {
        ESP_LOGE(TAG, "Invalid weight configuration: negative weights detected (w1=%.2f, w2=%.2f, w3=%.2f, w4=%.2f, w5=%.2f)", 
                 w1, w2, w3, w4, w5);
    }
    if (w1 + w2 + w3 + w4 + w5 == 0.0f) {
        ESP_LOGW(TAG, "All weights are zero - cost function will always return 0");
    }
}

float CostRouter::normalizeRSSI(int16_t rssi) {
    if (rssi >= RSSI_MAX) return 1.0f;
    if (rssi <= RSSI_MIN) return 0.0f;
    return static_cast<float>(rssi - RSSI_MIN) / (RSSI_MAX - RSSI_MIN);
}

float CostRouter::normalizeSNR(int8_t snr) {
    if (snr >= SNR_MAX) return 1.0f;
    if (snr <= SNR_MIN) return 0.0f;
    return static_cast<float>(snr - SNR_MIN) / (SNR_MAX - SNR_MIN);
}

float CostRouter::calculateCost(uint8_t hops, uint16_t nextHop, uint16_t destAddr,
                                int16_t rssi, int8_t snr, float etx, float gatewayBias) const {
    if (rssi < -150 || rssi > 0) {
        ESP_LOGW(TAG, "Suspicious RSSI value: %d dBm (node %04X)", rssi, nextHop);
    }
    
    if (snr < -20 || snr > 20) {
        ESP_LOGW(TAG, "Suspicious SNR value: %d dB (node %04X)", snr, nextHop);
    }
    
    if (etx < 1.0f || etx > 10.0f) {
        ESP_LOGW(TAG, "ETX out of expected range: %.2f (node %04X)", etx, nextHop);
    }
    
    float cost = 0.0f;
    
    cost += w1_hopCount * hops;
    
    float rssiNorm = normalizeRSSI(rssi);
    cost += w2_rssi * (1.0f - rssiNorm);
    
    float snrNorm = normalizeSNR(snr);
    cost += w3_snr * (1.0f - snrNorm);
    
    cost += w4_etx * (etx - 1.0f);
    
    cost += w5_gatewayBias * gatewayBias;
    
    return cost;
}

} // namespace xmesh
