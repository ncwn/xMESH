#ifndef XMESH_TRICKLE_SCHEDULER_H
#define XMESH_TRICKLE_SCHEDULER_H

#include <cstdint>

namespace xmesh {

/**
 * @brief RFC 6206 Trickle algorithm for adaptive HELLO scheduling
 * 
 * Reduces control overhead by 30-40% in stable networks via redundancy suppression.
 * 
 * Algorithm:
 * 1. Start with interval I = I_min
 * 2. Pick random transmission time t in [I/2, I]
 * 3. Listen for HELLOs, increment counter c
 * 4. At time t: transmit only if c < k (suppression)
 * 5. At interval end: double I (up to I_max), reset c, goto 2
 * 
 * Integration:
 * - Call shouldTransmit() periodically (e.g., every 1s) from main loop
 * - Call onHelloReceived() when HELLO packet arrives
 * - Call onInconsistentHello() on topology change to reset to I_min
 */
class TrickleScheduler {
private:
    uint32_t I_min;             // Minimum interval (ms)
    uint32_t I_max;             // Maximum interval (ms)
    uint32_t I_current;         // Current interval (ms)
    uint8_t k;                  // Redundancy constant
    
    uint32_t intervalStart;     // Start time of current interval
    uint32_t nextTransmit;      // Time of next transmission
    uint8_t consistentHeard;    // Count of consistent messages heard
    
    bool enabled;               // Enable/disable Trickle
    uint32_t transmitCount;     // Total transmissions
    uint32_t suppressCount;     // Suppressed transmissions
    
    enum State {
        IDLE,
        ACTIVE,
        RESET
    } state;

public:
    /**
     * @brief Construct Trickle scheduler
     * @param imin Minimum interval in milliseconds (default 60s)
     * @param imax Maximum interval in milliseconds (default 600s)
     * @param redundancy Redundancy constant k (default 1)
     * @param enable Whether Trickle is enabled (default true)
     */
    TrickleScheduler(uint32_t imin = 60000,
                     uint32_t imax = 600000,
                     uint8_t redundancy = 1,
                     bool enable = true);
    
    /**
     * @brief Start the Trickle timer
     */
    void start();
    
    /**
     * @brief Reset timer to minimum interval (on topology change)
     */
    void reset();
    
    /**
     * @brief Check if should transmit HELLO now
     * 
     * Call this periodically (e.g., every 1s) from main loop.
     * 
     * @return true if transmission should occur
     */
    bool shouldTransmit();
    
    /**
     * @brief Notify that a consistent HELLO was received
     * 
     * Call this when a HELLO packet arrives. Increments suppression counter.
     */
    void onHelloReceived();
    
    /**
     * @brief Notify that an inconsistent HELLO was received (topology change)
     * 
     * Call this on topology change detection. Resets timer to I_min.
     */
    void onInconsistentHello();
    
    /**
     * @brief Get current interval in seconds
     */
    float getCurrentIntervalSec() const;
    
    /**
     * @brief Get transmission count for logging
     */
    uint32_t getTransmitCount() const;
    
    /**
     * @brief Get suppress count for logging
     */
    uint32_t getSuppressCount() const;
    
    /**
     * @brief Check if Trickle is enabled
     */
    bool isEnabled() const;

private:
    /**
     * @brief Double the interval (on stable period)
     */
    void doubleInterval();
    
    /**
     * @brief Check if interval has expired
     */
    bool intervalExpired() const;
};

} // namespace xmesh

#endif // XMESH_TRICKLE_SCHEDULER_H
