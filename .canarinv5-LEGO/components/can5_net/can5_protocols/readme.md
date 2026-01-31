# Strategies
The strategies for sending data over network interfaces are here.

Hazemon server, which receives packet over UDP has different strategy than LoRaWAN.
Currently, the two strategies are:
1. UDP strategy (Hazmon Server compatible)
2. LWAN strategy (LoRaRelay Server compatible) 


Network manager uses can5_netstrat* to send data 'over the wire'. The protocol uses can5_codec to parse the data stored in storage manager and also provides uniform API's for the different netstrats.

can5_codec_* contains the packet maker and parser for the individual strategies.


Future strategies can be added here.


The steps used to add UDP and LWAN strategy are:
1. Add a new can5_strat_xxx.c using can5_netstrat interface.  
2. Add the strategy to `can5_netproto_type_t` in can5_netstrat.h.
3. Export the strategy in can5_netstrat.h as `extern can5_netproto_t netstrat_xxx`.
4. Add codec type xxx to `hazemon_codec_t` in can5_codec.h.
5. Modify `make_tx_packet` and `parse_rx_packet` in can5_codec.c.

The only real requirement is that a new protocol has to inherit the `can5_netproto_t` structure defined in can5_netstrat.h. can5_netmn.c also needs to be modified to include the new strategy.
