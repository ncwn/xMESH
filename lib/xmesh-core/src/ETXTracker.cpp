#include "xmesh/ETXTracker.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "ETX";

namespace xmesh {

// LinkMetrics constructor
LinkMetrics::LinkMetrics()
    : address(0), rssi(-120), snr(-20), etx(ETX_DEFAULT),
      windowIndex(0), windowFilled(0),
      lastSeqNum(0), seqInitialized(false),
      totalTxAttempts(0), totalTxSuccess(0), totalTxFailures(0), lastUpdate(0) {
    // Initialize sliding window with default (assume 67% success = ETX 1.5)
    for (int i = 0; i < ETX_WINDOW_SIZE; i++) {
        txWindow[i] = (i % 3 != 0);  // 2 success, 1 failure pattern
    }
}

// ETXTracker constructor
ETXTracker::ETXTracker() : numTrackedLinks(0) {
}

LinkMetrics* ETXTracker::getLinkMetrics(uint16_t address) {
    for (uint8_t i = 0; i < numTrackedLinks; i++) {
        if (linkMetrics[i].address == address) {
            return &linkMetrics[i];
        }
    }
    
    if (numTrackedLinks < MAX_TRACKED_LINKS) {
        linkMetrics[numTrackedLinks].address = address;
        return &linkMetrics[numTrackedLinks++];
    }
    
    ESP_LOGW(TAG, "Link map full (%d entries), evicting oldest entry for node %04X", 
             MAX_TRACKED_LINKS, address);
    
    uint32_t oldestTime = UINT32_MAX;
    uint8_t oldestIdx = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_LINKS; i++) {
        if (linkMetrics[i].lastUpdate < oldestTime) {
            oldestTime = linkMetrics[i].lastUpdate;
            oldestIdx = i;
        }
    }
    
    ESP_LOGI(TAG, "Evicted node %04X to make room for %04X", 
             linkMetrics[oldestIdx].address, address);
    
    linkMetrics[oldestIdx] = LinkMetrics();
    linkMetrics[oldestIdx].address = address;
    return &linkMetrics[oldestIdx];
}

void ETXTracker::updateLinkMetrics(uint16_t address, int16_t rssi, int8_t snr, uint32_t seqNum) {
    if (rssi < -150 || rssi > 0) {
        ESP_LOGW(TAG, "Invalid RSSI %d from node %04X", rssi, address);
    }
    
    if (snr < -20 || snr > 20) {
        ESP_LOGW(TAG, "Invalid SNR %d from node %04X", snr, address);
    }
    
    LinkMetrics* link = getLinkMetrics(address);

    if (link->lastUpdate == 0) {
        link->rssi = rssi;
        link->snr = snr;
    } else {
        link->rssi = (int16_t)(0.7 * link->rssi + 0.3 * rssi);
        link->snr = (int8_t)(0.7 * link->snr + 0.3 * snr);
    }

    link->lastUpdate = millis();

    if (!link->seqInitialized) {
        link->lastSeqNum = seqNum;
        link->seqInitialized = true;
        updateETX(address, true);
        ESP_LOGI(TAG, "Link %04X: First packet (seq=%lu), initializing ETX tracking",
                     address, seqNum);
    } else {
        uint32_t expectedSeq = link->lastSeqNum + 1;

        if (seqNum == expectedSeq) {
            updateETX(address, true);
            link->lastSeqNum = seqNum;
        } else if (seqNum > expectedSeq) {
            uint32_t gap = seqNum - expectedSeq;

            if (gap > ETX_WINDOW_SIZE) {
                ESP_LOGW(TAG, "Excessive gap detected on link %04X: %lu packets lost (capping at %d)", 
                         address, gap, ETX_WINDOW_SIZE);
                gap = ETX_WINDOW_SIZE;
            }

            for (uint32_t i = 0; i < gap && i < ETX_WINDOW_SIZE; i++) {
                updateETX(address, false);
                link->totalTxFailures++;
            }

            updateETX(address, true);
            link->lastSeqNum = seqNum;

            ESP_LOGW(TAG, "Link %04X: GAP DETECTED! Expected seq=%lu, got seq=%lu, lost %lu packets",
                         address, expectedSeq, seqNum, gap);
        } else {
            updateETX(address, true);
            link->lastSeqNum = seqNum;
            ESP_LOGD(TAG, "Link %04X: Out-of-order packet (expected %lu, got %lu), possibly reordered",
                         address, expectedSeq, seqNum);
        }
    }

    ESP_LOGD(TAG, "Link %04X: RSSI=%d dBm, SNR=%d dB, ETX=%.2f, Seq=%lu",
                 address, link->rssi, link->snr, link->etx, seqNum);
}

void ETXTracker::updateETX(uint16_t address, bool success) {
    LinkMetrics* link = getLinkMetrics(address);
    
    link->txWindow[link->windowIndex] = success;
    link->windowIndex = (link->windowIndex + 1) % ETX_WINDOW_SIZE;
    
    if (link->windowFilled < ETX_WINDOW_SIZE) {
        link->windowFilled++;
    }
    
    link->totalTxAttempts++;
    if (success) link->totalTxSuccess++;
    
    uint8_t successCount = 0;
    for (uint8_t i = 0; i < link->windowFilled; i++) {
        if (link->txWindow[i]) successCount++;
    }
    
    float deliveryRatio = (float)successCount / link->windowFilled;
    
    float instantETX;
    if (deliveryRatio > 0.01) {
        instantETX = 1.0 / deliveryRatio;
    } else {
        ESP_LOGW(TAG, "Very poor link quality for %04X: delivery ratio %.1f%%", 
                 address, deliveryRatio * 100);
        instantETX = 100.0;
    }
    
    if (link->windowFilled >= 3) {
        link->etx = ETX_ALPHA * instantETX + (1.0 - ETX_ALPHA) * link->etx;
    } else {
        link->etx = instantETX;
    }
    
    if (link->etx < 1.0) link->etx = 1.0;
    if (link->etx > 10.0) link->etx = 10.0;
    
    if (link->totalTxAttempts % 10 == 0) {
        ESP_LOGD(TAG, "ETX updated for %04X: %.2f (window: %d/%d, instant: %.2f, lifetime: %.1f%%)",
                     address, link->etx, successCount, link->windowFilled, instantETX,
                     (float)link->totalTxSuccess / link->totalTxAttempts * 100);
    }
}

} // namespace xmesh
