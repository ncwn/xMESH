# Network Manager 
The [Network Manager]() is the responsible to detect, initialize and operate the communication radio modules to
- Connect to the network and the server (if applicable)
- Receive data ready events and send throught he network.

It detects network card drivers on NET ports. 
Drivers are registered in the Internal handler.
Network card drivers implements the can5_netif_t Interface
 
## Network Drivers
Network drivers controls the peripherials through the functions exposed by HAL. 

Register to HAL events and implement the NIC specific protocol to connect and send bytes through the network.

Allows the net manager to install Reception  and Connection events Callbacks. In this way the network manager can operate on an interrupt base an be notified by the driver of relevant events.


```
    // return 0 if detected, and save details in the specified location
    can5_err_t (*detect)(can5_port_idx_t port);  
    
    // send bytes through the interface 
    can5_err_t (*send)(const void* data, size_t len);

    // initialize the module to be ready to connect
    can5_err_t (*init)(can5_port_idx_t port);

    // initialize the module to be ready to connect
    can5_err_t (*uninit)();

    /**
     * @brief Receive len bytes from the card and store data into prxdata buffer
     * @param prxdata pointer to return buffer
     * @param plen    pointer to size variable. It'll be used for number of byte to receive;
     * @param timeout  NOBOCK: if set to 0, do not block, return the bytes available in that moment
        *              MIN: 1000 / configTICK_RATE_HZ
        *              MAX: 0xFFFE
        *              BLOCK:If det to 0xFFFF Block indefinitely until the expected 
        *              number of bytes is received
     * 
     */
    can5_err_t (*recv)(void* prxdata, size_t* plen, uint16_t timeout);

    // set the callback function to execute upon reception of a packet on a specific port
    can5_err_t (*register_recv_cb)(can5_net_rxcb_f*);

    // return true if connection is considered established
    bool       (*is_connected)(void);

    /**
     * @brief Connect to the server
     * @param conncb Callback for connection events
     * @param wait   Time to wait for connection to be acquired
     *               If set to false return immediately. If specified,the callback will be called at connection or internal driver timeout event.
     *               If set to true return when the connection  is aquired or if the internal timeout expires. If specified, the  Callback is called fo all events.
     */
    can5_err_t (*connect)(can5_net_conncb_f* conncb, bool wait);

    // return rssi if available in dbm
    can5_netif_rssi_t (*rssi_get)(void);
    
    // Get driver status
    int32_t  (*status_get)();
```

## Tests
To build the tests, select "Build NET Tests" in the SDK configurator
netif_test_full(); can be used to test the main features of the driver.

## Examples
[can5_netif_template](components\can5_net\can5_netif_template\can5_netif_template.c) can be used as a guideline for writing drivers.