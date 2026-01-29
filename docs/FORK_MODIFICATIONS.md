# LoRaMesher Fork Modifications

This document tracks all modifications made to the LoRaMesher library fork in `src/`.

## Overview

The xMESH project uses a modified fork of LoRaMesher (6,119 lines) for the physical/MAC layer.
Modifications are minimal to preserve upstream updateability.

**Upstream Repository**: https://github.com/LoRaMesher/LoRaMesher
**Fork Version**: Based on v0.0.11

## Modifications

### 1. ISR Flag Correctness
- **File**: `src/LoraMesher.h:1072`
- **Change**: `volatile bool hasReceivedMessage`
- **Reason**: Flag is set in ISR, requires volatile qualifier for correct behavior

### 2. Route Deletion Declaration
- **File**: `src/LoraMesher.h:321`
- **Change**: Added `deleteRoute()` declaration
- **Reason**: Enables neighbor failure cleanup in xmesh-core

### 3. Route Deletion Implementation
- **File**: `src/LoraMesher.cpp:885`
- **Change**: Added `deleteRoute()` implementation
- **Reason**: Implements the declared route removal functionality

### 4. Thread-Safe Route Removal
- **Files**: `src/services/RoutingTableService.h:111`, `src/services/RoutingTableService.cpp:52`
- **Change**: Added `removeRoute()` method
- **Reason**: Thread-safe route deletion with proper mutex handling

## Re-Applying After Upstream Update

When updating LoRaMesher from upstream:

1. Apply volatile qualifier to `hasReceivedMessage` in LoraMesher.h (~line 1072)
2. Add `deleteRoute()` declaration in LoraMesher.h (~line 321)
3. Add `deleteRoute()` implementation in LoraMesher.cpp (~line 885)
4. Add `removeRoute()` to RoutingTableService (header and cpp)

## Why Fork?

These modifications enable:
- **Correct ISR behavior** - Prevents compiler optimization issues
- **Neighbor cleanup** - xmesh-core's GatewayBalancer needs to remove failed routes
- **Thread safety** - FreeRTOS-compatible route management

## Build Integration

The fork is included via `platformio.ini`:
```ini
lib_deps =
    ...
    file://../../
```

This includes the root `src/` directory as a library dependency.
