# Modules

The Modules manage a distinct task in the device.
Modules owns an Event Base, declared in the heathed and defined in the source: 

```
// in can5_<tag>.h
ESP_EVENT_DECLARE_BASE(CAN5_EVT_<tag>);

// in can5_<tag>.c
ESP_EVENT_DEFINE_BASE(CAN5_EVT_<tag>);
```

Modules Are responsible for communicating with the peripherials drivers and to run the scheduled operations.

Modules are initialized in the main()

## Hardware Abstraction Layer
Abstract the Canarin ports multiplexing system and allows drivers to operate the peripherials by managing the mux switching.


## Network Manager 
The [Network Manager]() is the responsible to detect, initialize and operate the communication radio modules to
- Connect to the network and the server (if applicable)
- Receive data ready events and send throught he network.

It detects network card drivers on NET ports. 
Drivers are registered in the Internal handler.
Network card drivers implements the can5_netif_t Interface
 
## Sensor Manager

## Local Storage Manager

## Configuration Manager

## Battery Monitor




