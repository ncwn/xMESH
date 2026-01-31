/**
 * @file RoutingAdapter.h
 * @brief Thread-safe wrapper for accessing routing table data.
 * 
 * Provides a decoupled interface for querying network node status without
 * direct dependency on LoRaMesher headers in core routing logic.
 */

#ifndef XMESH_ROUTING_ADAPTER_H
#define XMESH_ROUTING_ADAPTER_H

#include <cstdint>

namespace xmesh {

/**
 * @brief Snapshot of a routing table entry
 */
struct RouteNodeCopy {
    uint16_t address;
    uint16_t via;
    uint8_t metric;
    int8_t receivedSNR;
    int8_t sentSNR;
    uint8_t role;
    bool valid;
    
    RouteNodeCopy() : address(0), via(0), metric(0), receivedSNR(0), sentSNR(0), role(0), valid(false) {}
};

/**
 * @brief Adapter for routing table interactions
 */
class RoutingAdapter {
public:
    /**
     * @brief Finds a node in the routing table and returns a copy of its state
     * @param address The target node address
     * @param out Reference to store the node snapshot
     * @return true if node was found and valid, false otherwise
     */
    static bool findNodeCopy(uint16_t address, RouteNodeCopy& out);
};

} // namespace xmesh

#endif // XMESH_ROUTING_ADAPTER_H
