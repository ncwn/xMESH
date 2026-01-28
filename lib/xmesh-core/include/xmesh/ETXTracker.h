#ifndef XMESH_ETX_TRACKER_H
#define XMESH_ETX_TRACKER_H

#include <stdint.h>

namespace xmesh {

// ETX Configuration Constants
constexpr uint8_t ETX_WINDOW_SIZE = 10;
constexpr float ETX_DEFAULT = 1.5f;
constexpr float ETX_ALPHA = 0.3f;

/**
 * @brief Link metrics for a single neighbor.
 * 
 * Tracks RSSI, SNR, and ETX (Expected Transmission Count) using sequence-gap detection.
 */
struct LinkMetrics {
    uint16_t address;           // Neighbor address
    int16_t rssi;               // Last RSSI (dBm) - currently estimated from SNR
    int8_t snr;                 // Last SNR (dB) - from LoRaMesher receivedSNR
    float etx;                  // Expected Transmission Count (sequence-gap based)

    // Sliding window for ETX calculation
    bool txWindow[ETX_WINDOW_SIZE];  // Transmission results (true = success, false = failure)
    uint8_t windowIndex;             // Current position in circular buffer
    uint8_t windowFilled;            // Number of valid entries in window

    // Sequence number tracking for gap detection
    uint32_t lastSeqNum;        // Last received sequence number
    bool seqInitialized;        // Has received first packet from this source

    uint32_t totalTxAttempts;   // Total transmission attempts (for statistics)
    uint32_t totalTxSuccess;    // Total successful transmissions
    uint32_t totalTxFailures;   // Total detected failures (from gaps)
    uint32_t lastUpdate;        // Timestamp of last update
    
    LinkMetrics();
};

/**
 * @brief Zero-overhead Expected Transmission Count (ETX) tracking via sequence-gap detection.
 * 
 * ETXTracker implements a zero-overhead link quality estimation mechanism using sequence
 * number gap detection. Instead of sending probe packets or requiring ACKs, it infers
 * packet loss by monitoring gaps in sequence numbers of received packets.
 * 
 * Key Features:
 * - Sequence-gap detection: No protocol overhead (no ACK packets needed)
 * - Sliding window: Only recent packets affect ETX (time-decayed)
 * - EWMA smoothing: Reduces jitter from single packet losses
 * - Realistic ETX: Reflects actual link quality (can increase when quality degrades)
 * 
 * Algorithm:
 * 1. Detect gaps in sequence numbers by comparing current vs expected
 * 2. Record failure for each missing packet in gap
 * 3. Record success for successfully received packet
 * 4. Calculate delivery ratio from sliding window
 * 5. Compute instantaneous ETX = 1 / delivery_ratio
 * 6. Apply EWMA smoothing: ETX_new = α × ETX_instant + (1-α) × ETX_old
 */
class ETXTracker {
public:
    ETXTracker();

    /**
     * @brief Update link metrics for a neighbor.
     * @param address Neighbor address
     * @param rssi RSSI value (dBm)
     * @param snr SNR value (dB)
     * @param seqNum Sequence number from received packet
     */
    void updateLinkMetrics(uint16_t address, int16_t rssi, int8_t snr, uint32_t seqNum);

    /**
     * @brief Update ETX for a link after transmission attempt.
     * @param address Neighbor address
     * @param success True if transmission succeeded, False if packet loss detected
     */
    void updateETX(uint16_t address, bool success);

    /**
     * @brief Get link metrics for a specific neighbor.
     * @param address Neighbor address
     * @return Pointer to LinkMetrics, or nullptr if not found
     */
    LinkMetrics* getLinkMetrics(uint16_t address);

    /**
     * @brief Get current number of tracked links.
     * @return Number of active tracked links
     */
    uint8_t getNumTrackedLinks() const { return numTrackedLinks; }

private:
    static constexpr uint8_t MAX_TRACKED_LINKS = 10;
    LinkMetrics linkMetrics[MAX_TRACKED_LINKS];
    uint8_t numTrackedLinks;
};

} // namespace xmesh

#endif // XMESH_ETX_TRACKER_H
