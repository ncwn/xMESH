# Protocol 3 Validation Suite

This directory contains the complete validation test suite for Protocol 3 (Gateway-Aware Cost Routing) conducted on November 13, 2025.

## Test Suite Contents

### Test A: Baseline Stable Operation
**Directory:** `w5_gateway_indoor_20251113_013301/`
- Duration: 29 minutes
- Validates normal operation, Trickle progression, and W5 load balancing
- All nodes stable from cold start

### Test B: Fault Detection and Recovery
**Directory:** `w5_gateway_indoor_over_I600s_node_detection_20251113_120301/`
- Duration: 35 minutes
- Validates fault detection at Trickle I_max=600s
- Multiple node failure and recovery scenarios

### Test C: Physical Multi-Hop
**Directory:** `w5_gateway_phy_20251113_181954/`
- Duration: 10 minutes
- Attempts multi-hop validation with reduced TX power
- Partial success (routing tables correct, forwarding not achieved)

## Key Files

- `ANALYSIS.md` - Comprehensive academic analysis of all three tests
- Individual test folders contain:
  - `nodeX_YYYYMMDD_HHMMSS.log` - Raw serial capture logs
  - `ANALYSIS.md` - Detailed test-specific analysis

## Quick Results Summary

✅ **Validated:**
- 31-57% overhead reduction
- 360-380s fault detection
- 100% PDR in stable conditions
- Multi-gateway load balancing
- Automatic recovery mechanisms

⚠️ **Requires Further Testing:**
- Physical multi-hop forwarding (need greater node separation)

## Citation

If using this data in research, please reference:
```
Protocol 3 Gateway-Aware Cost Routing Validation Suite
Asian Institute of Technology, Thailand
November 2025
```