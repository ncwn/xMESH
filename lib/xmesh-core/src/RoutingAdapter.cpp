#include "xmesh/RoutingAdapter.h"
#include "services/RoutingTableService.h"

namespace xmesh {

bool RoutingAdapter::findNodeCopy(uint16_t address, RouteNodeCopy& out) {
    auto* list = RoutingTableService::routingTableList;
    list->setInUse();

    bool found = false;
    if (list->moveToStart()) {
        do {
            RouteNode* node = list->getCurrent();
            if (node->networkNode.address == address) {
                out.address = node->networkNode.address;
                out.via = node->via;
                out.metric = node->networkNode.metric;
                out.receivedSNR = node->receivedSNR;
                out.sentSNR = node->sentSNR;
                out.role = node->networkNode.role;
                out.valid = true;
                found = true;
                break;
            }
        } while (list->next());
    }

    list->releaseInUse();
    
    if (!found) {
        out.valid = false;
    }
    return found;
}

} // namespace xmesh
