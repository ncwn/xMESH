#include "xmesh/ETXTracker.h"
#include <Arduino.h>

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
    // Search for existing link
    for (uint8_t i = 0; i < numTrackedLinks; i++) {
        if (linkMetrics[i].address == address) {
            return &linkMetrics[i];
        }
    }
    
    // Not found - create new entry if space available
    if (numTrackedLinks < MAX_TRACKED_LINKS) {
        linkMetrics[numTrackedLinks].address = address;
        return &linkMetrics[numTrackedLinks++];
    }
    
    // No space available - replace oldest entry (simple FIFO eviction)
    uint32_t oldestTime = UINT32_MAX;
    uint8_t oldestIdx = 0;
    for (uint8_t i = 0; i < MAX_TRACKED_LINKS; i++) {
        if (linkMetrics[i].lastUpdate < oldestTime) {
            oldestTime = linkMetrics[i].lastUpdate;
            oldestIdx = i;
        }
    }
    
    // Reinitialize the entry
    linkMetrics[oldestIdx] = LinkMetrics();
    linkMetrics[oldestIdx].address = address;
    return &linkMetrics[oldestIdx];
}

void ETXTracker::updateLinkMetrics(uint16_t address, int16_t rssi, int8_t snr, uint32_t seqNum) {
    LinkMetrics* link = getLinkMetrics(address);

    // Update RSSI/SNR with exponential moving average (alpha = 0.3)
    if (link->lastUpdate == 0) {
        // First measurement
        link->rssi = rssi;
        link->snr = snr;
    } else {
        // Exponential moving average
        link->rssi = (int16_t)(0.7 * link->rssi + 0.3 * rssi);
        link->snr = (int8_t)(0.7 * link->snr + 0.3 * snr);
    }

    link->lastUpdate = millis();

    // Sequence-gap detection for ETX calculation
    if (!link->seqInitialized) {
        // First packet from this source - initialize sequence tracking
        link->lastSeqNum = seqNum;
        link->seqInitialized = true;
        updateETX(address, true);  // First packet is always a success
        Serial.printf("Link %04X: First packet (seq=%lu), initializing ETX tracking\n",
                     address, seqNum);
    } else {
        // Check for sequence gaps
        uint32_t expectedSeq = link->lastSeqNum + 1;

        if (seqNum == expectedSeq) {
            // No gap - packet received in order
            updateETX(address, true);
            link->lastSeqNum = seqNum;
        } else if (seqNum > expectedSeq) {
            // Gap detected! Packets were lost
            uint32_t gap = seqNum - expectedSeq;

            // Record failures for each missing packet
            for (uint32_t i = 0; i < gap && i < ETX_WINDOW_SIZE; i++) {
                updateETX(address, false);  // Record failure for each lost packet
                link->totalTxFailures++;
            }

            // Record success for current packet
            updateETX(address, true);
            link->lastSeqNum = seqNum;

            Serial.printf("Link %04X: GAP DETECTED! Expected seq=%lu, got seq=%lu, lost %lu packets\n",
                         address, expectedSeq, seqNum, gap);
        } else {
            // seqNum < expectedSeq: Out-of-order or sequence wrapped
            // Could be reordering or sender restarted
            // Just update and treat as success (don't penalize)
            updateETX(address, true);
            link->lastSeqNum = seqNum;
            Serial.printf("Link %04X: Out-of-order packet (expected %lu, got %lu), possibly reordered\n",
                         address, expectedSeq, seqNum);
        }
    }

    Serial.printf("Link %04X: RSSI=%d dBm, SNR=%d dB, ETX=%.2f, Seq=%lu\n",
                 address, link->rssi, link->snr, link->etx, seqNum);
}

void ETXTracker::updateETX(uint16_t address, bool success) {
    LinkMetrics* link = getLinkMetrics(address);
    
    // Add transmission result to sliding window (circular buffer)
    link->txWindow[link->windowIndex] = success;
    link->windowIndex = (link->windowIndex + 1) % ETX_WINDOW_SIZE;
    
    // Track window fill status (for initial packets)
    if (link->windowFilled < ETX_WINDOW_SIZE) {
        link->windowFilled++;
    }
    
    // Update total statistics (for monitoring)
    link->totalTxAttempts++;
    if (success) link->totalTxSuccess++;
    
    // Calculate delivery ratio from sliding window
    uint8_t successCount = 0;
    for (uint8_t i = 0; i < link->windowFilled; i++) {
        if (link->txWindow[i]) successCount++;
    }
    
    float deliveryRatio = (float)successCount / link->windowFilled;
    
    // Calculate instantaneous ETX
    float instantETX;
    if (deliveryRatio > 0.01) {  // Avoid division by very small numbers
        instantETX = 1.0 / deliveryRatio;
    } else {
        instantETX = 100.0;  // Essentially unreachable
    }
    
    // Apply EWMA smoothing (only after window has some data)
    if (link->windowFilled >= 3) {  // Wait for at least 3 samples
        link->etx = ETX_ALPHA * instantETX + (1.0 - ETX_ALPHA) * link->etx;
    } else {
        link->etx = instantETX;  // Bootstrap phase: use instant value
    }
    
    // Clamp ETX to reasonable range [1.0, 10.0]
    if (link->etx < 1.0) link->etx = 1.0;
    if (link->etx > 10.0) link->etx = 10.0;
    
    // Periodic logging (every 10th packet) for production use
    if (link->totalTxAttempts % 10 == 0) {
        Serial.printf("ETX updated for %04X: %.2f (window: %d/%d, instant: %.2f, lifetime: %.1f%%)\n",
                     address, link->etx, successCount, link->windowFilled, instantETX,
                     (float)link->totalTxSuccess / link->totalTxAttempts * 100);
    }
}

} // namespace xmesh
