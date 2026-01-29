#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

#include <xmesh/TrickleScheduler.h>
#include <xmesh/CostRouter.h>
#include <xmesh/ETXTracker.h>
#include <xmesh/GatewayBalancer.h>

struct XMeshConfig {
    bool isGateway;
    char wifiSsid[33];
    char wifiPassword[65];
};

// Forward declarations
void initWiFi();
void initOTA();
void connectWiFi();
void disconnectWiFi();

static XMeshConfig config;
static bool wifiConnected = false;
static bool otaInitialized = false;
static nvs_handle_t nvsHandle;
static const char* NVS_NAMESPACE = "xmesh_cfg";

static const char* TAG = "MAIN";

constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 30;

xmesh::TrickleScheduler trickle(TRICKLE_I_MIN, TRICKLE_I_MAX, TRICKLE_K, TRICKLE_ENABLED);
xmesh::CostRouter costRouter(W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
xmesh::ETXTracker etxTracker;
xmesh::GatewayBalancer gatewayBalancer;

uint32_t lastMonitorCheck = 0;
constexpr uint32_t MONITOR_INTERVAL_MS = 60000;

struct TestPacket {
    uint32_t counter;
};

TestPacket* testPacket = new TestPacket;
uint32_t packetCounter = 0;

void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        
        LoraMesher& radio = LoraMesher::getInstance();
        while (radio.getReceivedQueueSize() > 0) {
            Serial.printf("[xMESH] Processing received packet\n");
            
            AppPacket<TestPacket>* packet = radio.getNextAppPacket<TestPacket>();
            
            Serial.printf("[xMESH] Received counter=%lu from %04X\n", 
                         packet->payload->counter, packet->src);
            
            trickle.onHelloReceived();
            gatewayBalancer.updateNeighborHealth(packet->src);
            
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

void initNVS() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "NVS initialized and namespace '%s' opened", NVS_NAMESPACE);
    }
}

void loadConfig() {
    uint8_t gatewayVal = 0;
    esp_err_t err = nvs_get_u8(nvsHandle, "is_gateway", &gatewayVal);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Gateway role not found in NVS, using default: false");
        config.isGateway = false;
    } else {
        config.isGateway = (gatewayVal != 0);
    }

    size_t required_size;
    err = nvs_get_str(nvsHandle, "wifi_ssid", NULL, &required_size);
    if (err == ESP_OK && required_size <= sizeof(config.wifiSsid)) {
        nvs_get_str(nvsHandle, "wifi_ssid", config.wifiSsid, &required_size);
    } else {
        memset(config.wifiSsid, 0, sizeof(config.wifiSsid));
    }

    err = nvs_get_str(nvsHandle, "wifi_pass", NULL, &required_size);
    if (err == ESP_OK && required_size <= sizeof(config.wifiPassword)) {
        nvs_get_str(nvsHandle, "wifi_pass", config.wifiPassword, &required_size);
    } else {
        memset(config.wifiPassword, 0, sizeof(config.wifiPassword));
    }

    gatewayBalancer.setIsGateway(config.isGateway);
    ESP_LOGI(TAG, "Loaded Config: Gateway=%s, SSID=%s", 
             config.isGateway ? "YES" : "NO", config.wifiSsid);
}

void saveGatewayRole(bool isGateway) {
    esp_err_t err = nvs_set_u8(nvsHandle, "is_gateway", isGateway ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(nvsHandle);
        if (err == ESP_OK) {
            config.isGateway = isGateway;
            gatewayBalancer.setIsGateway(isGateway);
            ESP_LOGI(TAG, "Gateway role saved: %s", isGateway ? "YES" : "NO");
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save gateway role: %s", esp_err_to_name(err));
    }
}

void saveWiFiCredentials(const char* ssid, const char* password) {
    esp_err_t err = nvs_set_str(nvsHandle, "wifi_ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvsHandle, "wifi_pass", password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvsHandle);
        if (err == ESP_OK) {
            strncpy(config.wifiSsid, ssid, sizeof(config.wifiSsid) - 1);
            strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
            ESP_LOGI(TAG, "WiFi credentials saved successfully");
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi credentials: %s", esp_err_to_name(err));
    }
}

void createReceiveMessagesTask() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    
    if (res != pdPASS) {
        Serial.printf("[ERROR] Receive App Task creation failed: %d\n", res);
    }
}

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() == 0) return;
    
    ESP_LOGI(TAG, "Command received: %s", command.c_str());
    
    if (command == "gateway on") {
        saveGatewayRole(true);
        LoraMesher::getInstance().addGatewayRole();
        Serial.println("[CMD] Gateway mode: ON");
    }
    else if (command == "gateway off") {
        saveGatewayRole(false);
        LoraMesher::getInstance().removeGatewayRole();
        Serial.println("[CMD] Gateway mode: OFF");
    }
    else if (command == "wifi on") {
        connectWiFi();
        initOTA();
    }
    else if (command == "wifi off") {
        disconnectWiFi();
    }
    else if (command == "wifi scan") {
        Serial.println("[WiFi] Scanning...");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        int n = WiFi.scanNetworks();
        Serial.printf("[WiFi] Found %d networks:\n", n);
        for (int i = 0; i < n; i++) {
            Serial.printf("  %d: %s (%d dBm) %s\n", 
                i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
        }
    }
    else if (command.startsWith("wifi ")) {
        String args = command.substring(5);
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx > 0) {
            String ssid = args.substring(0, spaceIdx);
            String password = args.substring(spaceIdx + 1);
            saveWiFiCredentials(ssid.c_str(), password.c_str());
            Serial.printf("[CMD] WiFi credentials saved: SSID=%s\n", ssid.c_str());
        } else {
            Serial.println("[CMD] Usage: wifi SSID PASSWORD");
        }
    }
    else if (command == "status") {
        LoraMesher& radio = LoraMesher::getInstance();
        Serial.println("==== xMESH Status ====");
        Serial.printf("Node Address: %04X\n", radio.getLocalAddress());
        Serial.printf("Gateway Mode: %s\n", config.isGateway ? "YES" : "NO");
        Serial.printf("WiFi: %s\n", wifiConnected ? WiFi.localIP().toString().c_str() : "OFF");
        Serial.printf("Neighbors: %d\n", gatewayBalancer.getNeighborCount());
        Serial.printf("Routing Table: %d entries\n", radio.routingTableSize());
        Serial.printf("Trickle TX: %lu, Suppressed: %lu\n", 
                     trickle.getTransmitCount(), trickle.getSuppressCount());
        Serial.printf("Free Heap: %lu bytes\n", esp_get_free_heap_size());
        Serial.println("======================");
    }
    else if (command == "reset trickle") {
        trickle.reset();
        Serial.println("[CMD] Trickle timer reset to I_min");
    }
    else if (command == "help") {
        Serial.println("Available commands:");
        Serial.println("  gateway on/off  - Toggle gateway mode");
        Serial.println("  wifi SSID PASS  - Set WiFi credentials");
        Serial.println("  wifi on/off     - Enable/disable WiFi for OTA");
        Serial.println("  wifi scan       - Scan for WiFi networks");
        Serial.println("  status          - Show node status");
        Serial.println("  reset trickle   - Reset Trickle timer");
        Serial.println("  help            - Show this help");
    }
    else {
        Serial.printf("[CMD] Unknown command: %s\n", command.c_str());
        Serial.println("[CMD] Type 'help' for available commands");
    }
}

void initWiFi() {
    if (!config.isGateway) {
        ESP_LOGI(TAG, "Not a gateway - skipping WiFi initialization");
        return;
    }
    connectWiFi();
}

void connectWiFi() {
    if (strlen(config.wifiSsid) == 0) {
        ESP_LOGW(TAG, "No WiFi SSID configured - use 'wifi SSID PASSWORD' command");
        Serial.println("[WiFi] No SSID configured");
        return;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", config.wifiSsid);
    Serial.printf("[WiFi] Connecting to '%s'...\n", config.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid, config.wifiPassword);
    
    int timeout = 15;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        Serial.print(".");
        timeout--;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        ESP_LOGI(TAG, "WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Connected - IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGW(TAG, "WiFi connection failed after 15s timeout");
        Serial.println("[WiFi] Connection failed");
    }
}

void initOTA() {
    if (!wifiConnected) {
        ESP_LOGI(TAG, "WiFi not connected - skipping OTA initialization");
        return;
    }
    
    // Create hostname from MAC address
    String hostname = "xmesh-";
    hostname += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    
    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.setPassword("xmesh2026");  // OTA password
    
    ArduinoOTA.onStart([]() {
        ESP_LOGI(TAG, "OTA Update starting...");
        Serial.println("[OTA] Update starting...");
    });
    
    ArduinoOTA.onEnd([]() {
        ESP_LOGI(TAG, "OTA Update complete!");
        Serial.println("[OTA] Update complete!");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        ESP_LOGE(TAG, "OTA Error[%u]", error);
        Serial.printf("[OTA] Error[%u]\n", error);
    });
    
    ArduinoOTA.begin();
    otaInitialized = true;
    ESP_LOGI(TAG, "OTA service started - hostname: %s", hostname.c_str());
    Serial.printf("[OTA] Ready - hostname: %s\n", hostname.c_str());
}

void disconnectWiFi() {
    if (otaInitialized) {
        ArduinoOTA.end();
        otaInitialized = false;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    ESP_LOGI(TAG, "WiFi disconnected");
    Serial.println("[WiFi] Disconnected");
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    
    initNVS();
    loadConfig();

    ESP_LOGI(TAG, "Initializing watchdog (timeout: %lu seconds)", WATCHDOG_TIMEOUT_SEC);
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "Watchdog initialized and added to main task");
    
    Serial.printf("\n\n");
    Serial.printf("====================================\n");
    Serial.printf("  xMESH Production Firmware\n");
    Serial.printf("  Modular LoRa Mesh Network\n");
    Serial.printf("====================================\n");
    Serial.printf("Trickle: I_min=%lus, I_max=%lus, k=%d\n", 
                  TRICKLE_I_MIN/1000, TRICKLE_I_MAX/1000, TRICKLE_K);
    Serial.printf("====================================\n\n");
    
    trickle.start();
    Serial.printf("[Trickle] Started with interval: %.1fs\n", 
                  trickle.getCurrentIntervalSec());
    
    LoraMesher& radio = LoraMesher::getInstance();
    
    // Configure LoRaMesher with Heltec WiFi LoRa 32 V3 pins (SX1262)
    LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();
    config.loraCs = 8;      // SS pin
    config.loraRst = 12;    // RST_LoRa  
    config.loraIrq = 14;    // DIO1 (interrupt)
    config.loraIo1 = 13;    // BUSY_LoRa (gpio for SX1262)
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    
    radio.begin(config);
    
    Serial.printf("[LoRaMesher] Initialized with address: %04X\n", radio.getLocalAddress());
    
    createReceiveMessagesTask();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
    
    radio.start();
    Serial.printf("[LoRaMesher] Radio started\n");
    
    // WiFi and OTA for gateway nodes
    initWiFi();
    initOTA();
    
    Serial.printf("\n[xMESH] Initialization complete. Entering main loop...\n\n");
    ESP_LOGI(TAG, "System initialized successfully");
}

void loop() {
    esp_task_wdt_reset();
    processSerialCommands();
    
    if (trickle.shouldTransmit()) {
        Serial.printf("[Trickle] Transmitting (interval: %.1fs, TX: %lu, Suppressed: %lu)\n",
                     trickle.getCurrentIntervalSec(),
                     trickle.getTransmitCount(),
                     trickle.getSuppressCount());
        
        LoraMesher& radio = LoraMesher::getInstance();
        
        testPacket->counter = packetCounter++;
        radio.createPacketAndSend(BROADCAST_ADDR, testPacket, 1);
    }
    
    uint32_t now = millis();
    if (now - lastMonitorCheck >= MONITOR_INTERVAL_MS) {
        lastMonitorCheck = now;
        
        uint8_t failedNeighbors = gatewayBalancer.monitorNeighborHealth();
        if (failedNeighbors > 0) {
            Serial.printf("[GatewayBalancer] Detected %d failed neighbors\n", failedNeighbors);
        }
        
        LoraMesher& radio = LoraMesher::getInstance();
        Serial.printf("[Status] Tracked Links: %d, Neighbors: %d, Routing Table Size: %d\n",
                     etxTracker.getNumTrackedLinks(),
                     gatewayBalancer.getNeighborCount(),
                     radio.routingTableSize());
    }
    
    // Handle OTA updates (non-blocking)
    if (otaInitialized) {
        ArduinoOTA.handle();
    }
    
    delay(1000);
}
