/**
 * @file FrameCounter.h
 * @brief Replay protection using monotonic frame counters (like LoRaWAN FCnt)
 * 
 * Provides per-device frame counters to prevent replay attacks:
 * - Outgoing: monotonic counter incremented on each transmission
 * - Incoming: tracks last seen counter per source, rejects old/duplicate
 * - Persistent storage via NVS for counter continuity across reboots
 */

#ifndef XMESH_SECURITY_FRAME_COUNTER_H
#define XMESH_SECURITY_FRAME_COUNTER_H

#include <cstdint>

namespace xmesh {
namespace security {

// Maximum tracked peers for incoming frame counters
constexpr uint8_t MAX_TRACKED_PEERS = 32;

// Counter window size for out-of-order tolerance (0 = strict sequential)
constexpr uint16_t COUNTER_WINDOW_SIZE = 16;

// NVS namespace for persistent counter storage
constexpr const char* NVS_SECURITY_NAMESPACE = "xmesh_sec";

/**
 * @brief Per-peer frame counter tracking entry
 */
struct PeerCounter {
    uint16_t address;           // Peer node address
    uint32_t lastCounter;       // Last seen counter value
    uint32_t windowBitmap;      // Bitmap for window-based replay detection
    uint32_t lastSeenMs;        // Last activity timestamp
    
    PeerCounter() : address(0), lastCounter(0), windowBitmap(0), lastSeenMs(0) {}
};

/**
 * @brief Frame counter manager for replay protection
 * 
 * Usage:
 * 1. Call begin() at startup to initialize from NVS
 * 2. Call getNextOutgoing() before each transmission
 * 3. Call validateIncoming() on received packets
 * 4. Periodically call persist() to save state
 */
class FrameCounter {
public:
    FrameCounter();
    ~FrameCounter();
    
    /**
     * @brief Initialize frame counter from NVS storage
     * @return true if initialized successfully
     */
    bool begin();
    
    /**
     * @brief Get next outgoing frame counter and increment
     * @return Current counter value (post-increment)
     */
    uint32_t getNextOutgoing();
    
    /**
     * @brief Get current outgoing counter without incrementing
     * @return Current counter value
     */
    uint32_t getCurrentOutgoing() const { return outgoingCounter_; }
    
    /**
     * @brief Validate incoming frame counter from a peer
     * 
     * Implements window-based replay detection:
     * - Counters > lastSeen + WINDOW_SIZE: accept, update lastSeen
     * - Counters within window: check bitmap, accept if not seen
     * - Counters < lastSeen - WINDOW_SIZE: reject (too old)
     * 
     * @param srcAddress Source node address
     * @param counter Frame counter from packet
     * @return true if counter is valid (not replay), false if replay detected
     */
    bool validateIncoming(uint16_t srcAddress, uint32_t counter);
    
    /**
     * @brief Persist current counters to NVS
     * @return true if persisted successfully
     */
    bool persist();
    
    /**
     * @brief Get statistics for diagnostics
     */
    uint32_t getReplayRejectCount() const { return replayRejectCount_; }
#if defined(UNIT_TEST) || defined(NATIVE_BUILD)
    uint8_t getTrackedPeerCount() const;
#endif
    
    /**
     * @brief Clean up stale peer entries (not seen for timeout period)
     * @param timeoutMs Timeout in milliseconds (default 1 hour)
     * @return Number of peers cleaned up
     */
    uint8_t cleanupStalePeers(uint32_t timeoutMs = 3600000);

private:
    uint32_t outgoingCounter_;
    PeerCounter peers_[MAX_TRACKED_PEERS];
    uint32_t replayRejectCount_;
    bool initialized_;
    
    // Mutex for thread safety
    void* mutex_;
    
    PeerCounter* findOrCreatePeer(uint16_t address);
    
    /**
     * @brief Load counter state from NVS
     */
    bool loadFromNVS();
    
    /**
     * @brief Save counter state to NVS
     */
    bool saveToNVS();
};

} // namespace security
} // namespace xmesh

#endif // XMESH_SECURITY_FRAME_COUNTER_H
