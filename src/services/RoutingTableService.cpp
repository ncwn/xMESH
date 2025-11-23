#include "RoutingTableService.h"

size_t RoutingTableService::routingTableSize() {
    return routingTableList->getLength();
}

RouteNode* RoutingTableService::findNode(uint16_t address) {
    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.address == address) {
                routingTableList->releaseInUse();
                return node;
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
    return nullptr;
}

RouteNode* RoutingTableService::getBestNodeByRole(uint8_t role) {
    // If no custom cost function is registered, fall back to hop-count search
    if (costCallback == nullptr) {
        RouteNode* bestNode = nullptr;

        routingTableList->setInUse();

        if (routingTableList->moveToStart()) {
            do {
                RouteNode* node = routingTableList->getCurrent();

                if ((node->networkNode.role & role) == role &&
                    (bestNode == nullptr || node->networkNode.metric < bestNode->networkNode.metric)) {
                    bestNode = node;
                }

            } while (routingTableList->next());
        }

        routingTableList->releaseInUse();
        return bestNode;
    }

    // Collect gateway candidates without calling the cost callback while the
    // routing-table mutex is held (cost callback also inspects the table).
    struct GatewayCandidate {
        uint16_t address;
        uint16_t via;
        uint8_t metric;
    };

    GatewayCandidate candidates[RTMAXSIZE];
    uint8_t candidateCount = 0;

    routingTableList->setInUse();
    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();
            if ((node->networkNode.role & role) != role) {
                continue;
            }

            if (candidateCount < RTMAXSIZE) {
                candidates[candidateCount].address = node->networkNode.address;
                candidates[candidateCount].via = node->via;
                candidates[candidateCount].metric = node->networkNode.metric;
                candidateCount++;
            }
        } while (routingTableList->next());
    }
    routingTableList->releaseInUse();

    if (candidateCount == 0) {
        return nullptr;
    }

    float bestCost = 0.0f;
    bool bestCostValid = false;
    uint16_t bestAddress = candidates[0].address;

    for (uint8_t i = 0; i < candidateCount; ++i) {
        float candidateCost = costCallback(
            candidates[i].metric,
            candidates[i].via,
            candidates[i].address
        );

        if (!bestCostValid || candidateCost < bestCost) {
            bestCost = candidateCost;
            bestCostValid = true;
            bestAddress = candidates[i].address;
        }
    }

    return findNode(bestAddress);
}

bool RoutingTableService::hasAddressRoutingTable(uint16_t address) {
    RouteNode* node = findNode(address);
    return node != nullptr;
}

uint16_t RoutingTableService::getNextHop(uint16_t dst) {
    RouteNode* node = findNode(dst);

    if (node == nullptr)
        return 0;

    return node->via;
}

uint8_t RoutingTableService::getNumberOfHops(uint16_t address) {
    RouteNode* node = findNode(address);

    if (node == nullptr)
        return 0;

    return node->networkNode.metric;
}

void RoutingTableService::processRoute(RoutePacket* p, int8_t receivedSNR) {
    if ((p->packetSize - sizeof(RoutePacket)) % sizeof(NetworkNode) != 0) {
        ESP_LOGE(LM_TAG, "Invalid route packet size");
        return;
    }

    size_t numNodes = p->getNetworkNodesSize();
    ESP_LOGI(LM_TAG, "Route packet from %X with size %d", p->src, numNodes);

    NetworkNode* receivedNode = new NetworkNode(p->src, 1, p->nodeRole, p->gatewayLoad);
    processRoute(p->src, receivedNode);
    delete receivedNode;

    resetReceiveSNRRoutePacket(p->src, receivedSNR);

    for (size_t i = 0; i < numNodes; i++) {
        NetworkNode* node = &p->networkNodes[i];
        node->metric++;
        processRoute(p->src, node);
    }

    printRoutingTable();

    // Notify callback if registered (for Trickle suppression)
    if (helloCallback != nullptr) {
        helloCallback(p->src);
    }
}

void RoutingTableService::resetReceiveSNRRoutePacket(uint16_t src, int8_t receivedSNR) {
    RouteNode* rNode = findNode(src);
    if (rNode == nullptr)
        return;

    ESP_LOGI(LM_TAG, "Reset Receive SNR from %X: %d", src, receivedSNR);

    rNode->receivedSNR = receivedSNR;
}

void RoutingTableService::processRoute(uint16_t via, NetworkNode* node) {
    if (node->address != WiFiService::getLocalAddress()) {

        RouteNode* rNode = findNode(node->address);
        //If nullptr the node is not inside the routing table, then add it
        if (rNode == nullptr) {
            addNodeToRoutingTable(node, via);
            return;
        }

        //Update the metric and restart timeout if needed
        bool shouldUpdateRoute = false;

        // Use cost-based comparison if callback registered (Protocol 3)
        if (costCallback != nullptr) {
            float newCost = costCallback(node->metric, via, node->address);
            float currentCost = costCallback(rNode->networkNode.metric, rNode->via, node->address);

            // Apply hysteresis: new route must be 15% better to switch
            if (newCost < currentCost * 0.85) {
                shouldUpdateRoute = true;
                float improvement = ((currentCost - newCost) / currentCost) * 100.0;
                ESP_LOGI(LM_TAG, "[COST-ROUTING] Better route for %X via %X: cost %.2f < %.2f (-%.1f%%), metric %d→%d",
                        node->address, via, newCost, currentCost, improvement, rNode->networkNode.metric, node->metric);
            }
            else if (newCost < currentCost) {
                // Route is better but doesn't meet hysteresis threshold
                float improvement = ((currentCost - newCost) / currentCost) * 100.0;
                ESP_LOGD(LM_TAG, "[COST-ROUTING] Route for %X via %X is better (cost %.2f vs %.2f, -%.1f%%) but below 15%% hysteresis threshold",
                        node->address, via, newCost, currentCost, improvement);
            }
            else if (node->metric == rNode->networkNode.metric && via == rNode->via) {
                // Same path, just reset timeout
                ESP_LOGV(LM_TAG, "[COST-ROUTING] Refreshing route for %X via %X (cost %.2f, metric %d)",
                        node->address, via, newCost, node->metric);
                resetTimeoutRoutingNode(rNode);
            }
        }
        // Fallback to hop-count comparison (Protocol 1 & 2)
        else {
            if (node->metric < rNode->networkNode.metric) {
                shouldUpdateRoute = true;
                ESP_LOGI(LM_TAG, "Found better route for %X via %X metric %d", node->address, via, node->metric);
            }
            else if (node->metric == rNode->networkNode.metric) {
                resetTimeoutRoutingNode(rNode);
            }
        }

        // Update route if better path found
        if (shouldUpdateRoute) {
            rNode->networkNode.metric = node->metric;
            rNode->via = via;
            resetTimeoutRoutingNode(rNode);
        }

        // Update gateway load metadata when provided (propagates W5 bias data)
        if (node->gatewayLoad != 255 && node->gatewayLoad != rNode->networkNode.gatewayLoad) {
            rNode->networkNode.gatewayLoad = node->gatewayLoad;
        }

        // Update the Role only if the node that sent the packet is the next hop
        if (getNextHop(node->address) == via && node->role != rNode->networkNode.role) {
            ESP_LOGI(LM_TAG, "Updating role of %X to %d", node->address, node->role);
            rNode->networkNode.role = node->role;
        }
    }
}

void RoutingTableService::addNodeToRoutingTable(NetworkNode* node, uint16_t via) {
    if (routingTableList->getLength() >= RTMAXSIZE) {
        ESP_LOGW(LM_TAG, "Routing table max size reached, not adding route and deleting it");
        return;
    }

    // PROTOCOL 3 FIX: Allow higher-hop routes if they have better cost
    // This enables choosing 2-hop good-signal paths over 1-hop weak-signal paths
    if (costCallback != nullptr) {
        // Check if there's an existing route to this destination
        RouteNode* existingRoute = findNode(node->address);

        if (existingRoute != nullptr && node->metric > existingRoute->networkNode.metric) {
            // New route has MORE hops - normally rejected by distance-vector
            // BUT in cost-based routing, check if cost is better
            float newCost = costCallback(node->metric, via, node->address);
            float existingCost = costCallback(existingRoute->networkNode.metric,
                                             existingRoute->via,
                                             node->address);

            // If new route has significantly better cost (>20% improvement), use it!
            if (newCost < existingCost * 0.8) {
                // Replace existing route with better-quality multi-hop route
                ESP_LOGI(LM_TAG, "[COST-ROUTING] Replacing %d-hop route with better %d-hop route for %X: cost %.2f → %.2f (-%.1f%%)",
                        existingRoute->networkNode.metric, node->metric, node->address,
                        existingCost, newCost, ((existingCost - newCost) / existingCost) * 100.0);

                // Update existing route instead of adding new one
                existingRoute->networkNode.metric = node->metric;
                existingRoute->via = via;
                existingRoute->networkNode.gatewayLoad = node->gatewayLoad;
                resetTimeoutRoutingNode(existingRoute);
                return;
            } else {
                // New route has higher hops AND worse/similar cost - reject
                ESP_LOGD(LM_TAG, "[COST-ROUTING] Rejecting %d-hop route (cost %.2f vs existing %d-hop %.2f)",
                        node->metric, newCost, existingRoute->networkNode.metric, existingCost);
                return;
            }
        }
    }
    // Fallback: Original hop-count filtering for Protocol 1 & 2
    else if (calculateMaximumMetricOfRoutingTable() < node->metric) {
        ESP_LOGW(LM_TAG, "Trying to add a route with a metric higher than the maximum of the routing table, not adding route and deleting it");
        return;
    }

    RouteNode* rNode = new RouteNode(*node, via);

    //Reset the timeout of the node
    resetTimeoutRoutingNode(rNode);

    routingTableList->setInUse();

    routingTableList->Append(rNode);

    routingTableList->releaseInUse();

    ESP_LOGI(LM_TAG, "New route added: %X via %X metric %d, role %d", node->address, via, node->metric, node->role);
}

NetworkNode* RoutingTableService::getAllNetworkNodes() {
    routingTableList->setInUse();

    int routingSize = routingTableSize();

    // If the routing table is empty return nullptr
    if (routingSize == 0) {
        routingTableList->releaseInUse();
        return nullptr;
    }

    NetworkNode* payload = new NetworkNode[routingSize];

    if (routingTableList->moveToStart()) {
        for (int i = 0; i < routingSize; i++) {
            RouteNode* currentNode = routingTableList->getCurrent();
            payload[i] = currentNode->networkNode;

            if (!routingTableList->next())
                break;
        }
    }

    routingTableList->releaseInUse();

    return payload;
}

void RoutingTableService::resetTimeoutRoutingNode(RouteNode* node) {
    node->timeout = millis() + DEFAULT_TIMEOUT * 1000;
}

void RoutingTableService::aMessageHasBeenReceivedBy(uint16_t address) {
    RouteNode* rNode = findNode(address);
    if (rNode == nullptr)
        return;

    resetTimeoutRoutingNode(rNode);
}

void RoutingTableService::printRoutingTable() {
    ESP_LOGI(LM_TAG, "Current routing table:");

    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        size_t position = 0;

        do {
            RouteNode* node = routingTableList->getCurrent();

            ESP_LOGI(LM_TAG, "%d - %X via %X metric %d Role %d", position,
                node->networkNode.address,
                node->via,
                node->networkNode.metric,
                node->networkNode.role);

            position++;
        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();
}

void RoutingTableService::manageTimeoutRoutingTable() {
    ESP_LOGI(LM_TAG, "Checking routes timeout");

    routingTableList->setInUse();

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->timeout < millis()) {
                ESP_LOGW(LM_TAG, "Route timeout %X via %X", node->networkNode.address, node->via);

                delete node;
                routingTableList->DeleteCurrent();
            }

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();

    printRoutingTable();
}

uint8_t RoutingTableService::calculateMaximumMetricOfRoutingTable() {
    routingTableList->setInUse();

    uint8_t maximumMetricOfRoutingTable = 0;

    if (routingTableList->moveToStart()) {
        do {
            RouteNode* node = routingTableList->getCurrent();

            if (node->networkNode.metric > maximumMetricOfRoutingTable)
                maximumMetricOfRoutingTable = node->networkNode.metric;

        } while (routingTableList->next());
    }

    routingTableList->releaseInUse();

    return maximumMetricOfRoutingTable + 1;
}

LM_LinkedList<RouteNode>* RoutingTableService::routingTableList = new LM_LinkedList<RouteNode>();
CostCalculationCallback RoutingTableService::costCallback = nullptr;
HelloReceivedCallback RoutingTableService::helloCallback = nullptr;

void RoutingTableService::setCostCalculationCallback(CostCalculationCallback callback) {
    costCallback = callback;
    if (callback != nullptr) {
        ESP_LOGI(LM_TAG, "Cost-based routing enabled");
    }
}

void RoutingTableService::setHelloReceivedCallback(HelloReceivedCallback callback) {
    helloCallback = callback;
    if (callback != nullptr) {
        ESP_LOGI(LM_TAG, "HELLO reception callback enabled (Trickle suppression)");
    }
}
