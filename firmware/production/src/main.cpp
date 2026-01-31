#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <WiFi.h>
#include <ota/OTAManager.h>
#include <PubSubClient.h>

#include <cfloat>
#include <xmesh/TrickleScheduler.h>
#include <xmesh/hal/Sensors.h>
#include <xmesh/hal/SensorPacket.h>
#include <xmesh/CostRouter.h>
#include <xmesh/ETXTracker.h>
#include <xmesh/GatewayBalancer.h>
#include <xmesh/hal/Display.h>
#include <xmesh/MobilityDetector.h>
#include <xmesh/security/SecurityManager.h>
#include "DutyCycleBudget.h"

// LoRaMesher routing service for callback registration
#include "services/RoutingTableService.h"
#include <xmesh/RoutingAdapter.h>

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
void applyMobilityParams(xmesh::MobilityState state);
uint32_t estimateAirtimeMs(size_t payloadLen);

static XMeshConfig config;
static bool wifiConnected = false;
static bool otaInitialized = false;
static xmesh::ota::OTAManager otaManager;
static xmesh::hal::Display display;
static xmesh::hal::Sensors sensors;
static HardwareSerial pmsSerial(1);  // UART1 for PMS7003
static HardwareSerial gpsSerial(2);  // UART2 for GPS
static nvs_handle_t nvsHandle;
static const char* NVS_NAMESPACE = "xmesh_cfg";

static const char* TAG = "MAIN";

static bool appMarkedValid = false;

constexpr uint32_t WATCHDOG_TIMEOUT_SEC = 30;

xmesh::TrickleScheduler trickle(TRICKLE_I_MIN, TRICKLE_I_MAX, TRICKLE_K, TRICKLE_ENABLED);
xmesh::CostRouter costRouter(W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
xmesh::ETXTracker etxTracker;
xmesh::GatewayBalancer gatewayBalancer;
xmesh::MobilityDetector mobilityDetector;
DutyCycleBudget dutyCycleBudget;

uint32_t lastMonitorCheck = 0;
uint32_t lastSensorTx = 0;
uint32_t lastSecurityCleanup = 0;
constexpr uint32_t MONITOR_INTERVAL_MS = 60000;
constexpr uint32_t SECURITY_CLEANUP_INTERVAL_MS = 3600000;  // 1 hour

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
char mqttBroker[64] = "";
uint16_t mqttPort = MQTT_PORT_DEFAULT;
bool mqttEnabled = false;
uint32_t lastMqttReconnect = 0;
uint32_t sensorTxCount = 0;

struct TestPacket {
    uint32_t counter;
};

static TestPacket testPacket;
uint32_t packetCounter = 0;

constexpr size_t SECURITY_MAX_PAYLOAD_BYTES = 128;

static bool sendSecurePacket(uint16_t destAddr, const uint8_t* payload, size_t payloadLen, size_t* outLen = nullptr) {
    if (payloadLen == 0) {
        return false;
    }

    LoraMesher& radio = LoraMesher::getInstance();
    auto& security = xmesh::security::SecurityManager::getInstance();
    const uint16_t localAddr = radio.getLocalAddress();

    if (security.getSecurityLevel() >= xmesh::security::SecurityLevel::ENCRYPTED) {
        if (payloadLen > SECURITY_MAX_PAYLOAD_BYTES) {
            ESP_LOGE(TAG, "Secure TX payload too large (%u > %u)",
                     static_cast<unsigned>(payloadLen),
                     static_cast<unsigned>(SECURITY_MAX_PAYLOAD_BYTES));
            return false;
        }

        uint8_t buffer[SECURITY_MAX_PAYLOAD_BYTES];
        memcpy(buffer, payload, payloadLen);
        size_t encLen = payloadLen;
        if (!security.securePayload(buffer, &encLen, sizeof(buffer), localAddr, destAddr)) {
            ESP_LOGW(TAG, "Secure TX failed for %04X", destAddr);
            return false;
        }

        radio.sendPacket(destAddr, buffer, encLen);
        if (outLen != nullptr) {
            *outLen = encLen;
        }
        ESP_LOGD(TAG, "Secure TX to %04X (%u -> %u bytes)", destAddr,
                 static_cast<unsigned>(payloadLen), static_cast<unsigned>(encLen));
        return true;
    }

    radio.sendPacket(destAddr, payload, payloadLen);
    if (outLen != nullptr) {
        *outLen = payloadLen;
    }
    return true;
}

template <typename T>
static bool sendSecurePacket(uint16_t destAddr, const T* payload, size_t payloadCount, size_t* outLen = nullptr) {
    return sendSecurePacket(destAddr,
                            reinterpret_cast<const uint8_t*>(payload),
                            payloadCount * sizeof(T),
                            outLen);
}

const char* getNodeModeName(xmesh::hal::NodeMode mode) {
    switch (mode) {
        case xmesh::hal::NodeMode::SENSOR: return "SENSOR";
        case xmesh::hal::NodeMode::RELAY: return "RELAY";
        case xmesh::hal::NodeMode::GATEWAY: return "GATEWAY";
        default: return "UNKNOWN";
    }
}

// CostCalculationCallback signature: (hops, via, destAddr) -> cost
float costCalculationCallback(uint8_t hops, uint16_t via, uint16_t destAddr) {
    // Reject routes through failed neighbors
    if (gatewayBalancer.isNeighborFailed(via)) {
        ESP_LOGD(TAG, "Rejecting route via %04X - neighbor failed", via);
        return FLT_MAX;
    }
    
    int8_t snr = 0;
    int16_t rssi = -100;
    float etx = ETX_DEFAULT;
    uint8_t encodedGatewayLoad = gatewayBalancer.peekLocalGatewayLoad();
    float gatewayBias = gatewayBalancer.getGatewayBias(via, encodedGatewayLoad);
    
    xmesh::RouteNodeCopy nodeCopy;
    if (xmesh::RoutingAdapter::findNodeCopy(via, nodeCopy)) {
        snr = nodeCopy.receivedSNR;
        rssi = static_cast<int16_t>(snr * 4 - 110);
    }
    
    xmesh::LinkMetrics* metrics = etxTracker.getLinkMetrics(via);
    if (metrics != nullptr) {
        etx = metrics->etx;
        if (metrics->rssi != 0) {
            rssi = metrics->rssi;
        }
        if (metrics->snr != 0) {
            snr = metrics->snr;
        }
    }
    
    float cost = costRouter.calculateCost(hops, via, destAddr, rssi, snr, etx, gatewayBias);
    
    ESP_LOGD(TAG, "Cost for %04X via %04X: %.2f (hops=%d, rssi=%d, snr=%d, etx=%.2f)", 
             destAddr, via, cost, hops, rssi, snr, etx);
    
    return cost;
}

void helloReceivedCallback(uint16_t srcAddr) {
    // Check if this is a NEW neighbor (topology change = inconsistency)
    xmesh::RouteNodeCopy nodeCopy;
    bool wasKnown = xmesh::RoutingAdapter::findNodeCopy(srcAddr, nodeCopy);
    
    if (!wasKnown) {
        // New neighbor detected - this is an inconsistency per RFC 6206
        ESP_LOGD(TAG, "New neighbor %04X detected - Trickle reset", srcAddr);
        trickle.onInconsistentHello();
    }
    
    trickle.onHelloReceived();
    
    // Re-fetch after processing (node may now be in table)
    if (xmesh::RoutingAdapter::findNodeCopy(srcAddr, nodeCopy) && nodeCopy.metric == 1) {
        int8_t snr = nodeCopy.receivedSNR;
        int16_t rssi = static_cast<int16_t>(snr * 4 - 110);
        etxTracker.updateLinkMetrics(srcAddr, rssi, snr, packetCounter);
    }
    
    if (mobilityDetector.isEnabled() && nodeCopy.valid) {
        mobilityDetector.feedSNR(srcAddr, nodeCopy.receivedSNR);
    }
    
    gatewayBalancer.updateNeighborHealth(srcAddr);
}

void updateDisplay() {
    display.clear();
    display.setCursor(0, 0);
    display.setTextSize(1);
    
    LoraMesher& radio = LoraMesher::getInstance();
    
    display.print("xMESH ");
    display.println(config.isGateway ? "[GW]" : "[NODE]");
    
    display.print("Addr: ");
    char addrBuf[8];
    snprintf(addrBuf, sizeof(addrBuf), "%04X", radio.getLocalAddress());
    display.println(addrBuf);
    
    display.print("Neighbors: ");
    display.println(gatewayBalancer.getNeighborCount());
    
    display.print("Routes: ");
    display.println(radio.routingTableSize());
    
    if (wifiConnected) {
        display.print("IP: ");
        display.println(WiFi.localIP().toString());
    }
    
    auto nodeMode = sensors.getNodeMode();
    display.print("Mode: ");
    display.println(getNodeModeName(nodeMode));
    
    if (sensors.isPMSDetected()) {
        auto aq = sensors.readAirQuality();
        if (aq.valid) {
            display.print("PM2.5: ");
            display.print(aq.pm2_5);
            display.println(" ug/m3");
        }
    }
    
    display.display();
}

void publishSensorToMQTT(uint16_t srcAddr, const xmesh::hal::SensorPacket* sp) {
    if (!mqttEnabled || !mqttClient.connected()) return;
    
    LoraMesher& radio = LoraMesher::getInstance();
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/%04X/%04X", MQTT_TOPIC_PREFIX, radio.getLocalAddress(), srcAddr);
    
    char json[256];
    snprintf(json, sizeof(json),
        "{\"version\":%d,\"node\":\"%04X\",\"gateway\":\"%04X\",\"timestamp\":%lu,"
        "\"pm\":{\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u,\"valid\":%s},"
        "\"gps\":{\"lat\":%.7f,\"lon\":%.7f,\"alt\":%d,\"sats\":%u,\"valid\":%s}}",
        sp->version, srcAddr, radio.getLocalAddress(), sp->timestamp,
        sp->pm1_0, sp->pm2_5, sp->pm10, (sp->flags & xmesh::hal::FLAG_PMS_VALID) ? "true" : "false",
        sp->latitude / 1e7, sp->longitude / 1e7, sp->altitude, sp->satellites,
        (sp->flags & xmesh::hal::FLAG_GPS_VALID) ? "true" : "false");
    
    mqttClient.publish(topic, json);
    ESP_LOGI(TAG, "MQTT published to %s", topic);
}

void mqttReconnect() {
    if (strlen(mqttBroker) == 0) return;
    if (millis() - lastMqttReconnect < MQTT_RECONNECT_INTERVAL_MS) return;
    lastMqttReconnect = millis();
    
    ESP_LOGI(TAG, "MQTT connecting to %s:%d", mqttBroker, mqttPort);
    
    LoraMesher& radio = LoraMesher::getInstance();
    char clientId[16];
    snprintf(clientId, sizeof(clientId), "xmesh-%04X", radio.getLocalAddress());
    
    if (mqttClient.connect(clientId)) {
        ESP_LOGI(TAG, "MQTT connected");
        mqttEnabled = true;
    } else {
        ESP_LOGW(TAG, "MQTT connection failed, rc=%d", mqttClient.state());
    }
}

void processReceivedPackets(void*) {
    for (;;) {
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        
        LoraMesher& radio = LoraMesher::getInstance();
        while (radio.getReceivedQueueSize() > 0) {
            AppPacket<uint8_t>* rawPacket = radio.getNextAppPacket<uint8_t>();
            auto& security = xmesh::security::SecurityManager::getInstance();

            if (security.getSecurityLevel() >= xmesh::security::SecurityLevel::ENCRYPTED) {
                size_t decLen = rawPacket->payloadSize;
                if (!security.verifyAndDecrypt(rawPacket->payload, &decLen, rawPacket->src, radio.getLocalAddress())) {
                    ESP_LOGW(TAG, "Security reject from %04X (len=%u)", rawPacket->src, rawPacket->payloadSize);
                    radio.deletePacket(rawPacket);
                    continue;
                }
                rawPacket->payloadSize = static_cast<uint8_t>(decLen);
                ESP_LOGD(TAG, "Security decrypt ok from %04X (len=%u)", rawPacket->src,
                         static_cast<unsigned>(decLen));
            }
            
            if (rawPacket->payloadSize == sizeof(xmesh::hal::SensorPacket)) {
                xmesh::hal::SensorPacket* sp = reinterpret_cast<xmesh::hal::SensorPacket*>(rawPacket->payload);
                if (sp->version == xmesh::hal::SENSOR_PACKET_VERSION) {
                    ESP_LOGI(TAG, "SensorPacket from %04X: PM2.5=%d, lat=%ld, lon=%ld",
                             rawPacket->src, sp->pm2_5, sp->latitude, sp->longitude);
                    Serial.printf("[RX] SensorPacket from %04X: PM2.5=%d\n", rawPacket->src, sp->pm2_5);
                    
                    if (config.isGateway && ENABLE_MQTT_FORWARD) {
                        publishSensorToMQTT(rawPacket->src, sp);
                        gatewayBalancer.recordGatewayLoadSample();
                        Serial.printf("[MQTT] Published sensor data from %04X\n", rawPacket->src);
                    }
                }
            } else {
                TestPacket* tp = reinterpret_cast<TestPacket*>(rawPacket->payload);
                ESP_LOGD(TAG, "TestPacket from %04X: counter=%lu", rawPacket->src, tp->counter);
            }
            
            trickle.onHelloReceived();
            gatewayBalancer.updateNeighborHealth(rawPacket->src);
            
            radio.deletePacket(rawPacket);
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
    
    uint8_t mobilityVal = 0;
    err = nvs_get_u8(nvsHandle, "mobility_en", &mobilityVal);
    if (err == ESP_OK && mobilityVal != 0) {
        mobilityDetector.enable();
        ESP_LOGI(TAG, "Mobility detection: enabled (from NVS)");
    }

    size_t mqtt_len = sizeof(mqttBroker);
    if (nvs_get_str(nvsHandle, "mqtt_broker", mqttBroker, &mqtt_len) == ESP_OK) {
        nvs_get_u16(nvsHandle, "mqtt_port", &mqttPort);
        ESP_LOGI(TAG, "Restored MQTT broker: %s:%d", mqttBroker, mqttPort);
    }
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

void saveMobilityEnabled(bool enabled) {
    esp_err_t err = nvs_set_u8(nvsHandle, "mobility_en", enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(nvsHandle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save mobility state: %s", esp_err_to_name(err));
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
    else if (command == "ota status") {
        ESP_LOGI(TAG, "OTA status requested");
        Serial.println("==== OTA Status ====");
        Serial.printf("State: %d\n", (int)otaManager.getState());
        Serial.printf("Current Version: %s\n", otaManager.getCurrentVersion());
        Serial.printf("Available: %s\n", otaManager.getAvailableVersion());
        Serial.printf("Update Available: %s\n", otaManager.isUpdateAvailable() ? "YES" : "NO");
        Serial.printf("Progress: %d%%\n", otaManager.getProgress());
        Serial.printf("Last Error: %d\n", (int)otaManager.getLastError());
        Serial.printf("Rollback Detected: %s\n", otaManager.getRollbackReason() ? "YES" : "NO");
        Serial.println("====================");
    }
    else if (command == "ota check") {
        if (!wifiConnected) {
            ESP_LOGW(TAG, "OTA check requested without WiFi");
            Serial.println("[OTA] WiFi not connected");
        } else {
            if (!otaInitialized) {
                initOTA();
            }
            ESP_LOGI(TAG, "Checking for updates (configured URL)");
            Serial.println("[OTA] Checking for updates...");
            if (otaManager.checkForUpdates()) {
                Serial.printf("[OTA] Update available: %s\n", otaManager.getAvailableVersion());
            } else {
                Serial.println("[OTA] No update available or check failed");
            }
        }
    }
    else if (command.startsWith("ota check ")) {
        String url = command.substring(10);
        url.trim();
        if (url.length() == 0) {
            Serial.println("[OTA] Usage: ota check <url>");
        } else if (!wifiConnected) {
            ESP_LOGW(TAG, "OTA check URL requested without WiFi");
            Serial.println("[OTA] WiFi not connected");
        } else {
            if (!otaInitialized) {
                initOTA();
            }
            otaManager.setVersionCheckUrl(url.c_str());
            ESP_LOGI(TAG, "Checking for updates: %s", url.c_str());
            Serial.println("[OTA] Checking for updates...");
            if (otaManager.checkForUpdates(url.c_str())) {
                Serial.printf("[OTA] Update available: %s\n", otaManager.getAvailableVersion());
            } else {
                Serial.println("[OTA] No update available or check failed");
            }
        }
    }
    else if (command.startsWith("ota url ")) {
        String url = command.substring(8);
        url.trim();
        if (url.length() == 0) {
            Serial.println("[OTA] Usage: ota url <url>");
        } else {
            otaManager.setFirmwareUrl(url.c_str());
            ESP_LOGI(TAG, "Firmware URL set: %s", url.c_str());
            Serial.printf("[OTA] Firmware URL set: %s\n", url.c_str());
        }
    }
    else if (command == "ota update") {
        if (!wifiConnected) {
            ESP_LOGW(TAG, "OTA update requested without WiFi");
            Serial.println("[OTA] WiFi not connected");
        } else if (otaManager.getState() != xmesh::ota::OTAState::IDLE) {
            Serial.println("[OTA] Update already in progress");
        } else {
            if (!otaInitialized) {
                initOTA();
            }
            ESP_LOGI(TAG, "Starting HTTP OTA update (configured URL)");
            Serial.println("[OTA] Starting HTTP update...");
            if (!otaManager.startHttpUpdate(nullptr)) {
                Serial.println("[OTA] Update failed to start");
            }
        }
    }
    else if (command.startsWith("ota update ")) {
        String url = command.substring(11);
        url.trim();
        if (url.length() == 0) {
            Serial.println("[OTA] Usage: ota update <url>");
        } else if (!wifiConnected) {
            ESP_LOGW(TAG, "OTA update URL requested without WiFi");
            Serial.println("[OTA] WiFi not connected");
        } else if (otaManager.getState() != xmesh::ota::OTAState::IDLE) {
            Serial.println("[OTA] Update already in progress");
        } else {
            if (!otaInitialized) {
                initOTA();
            }
            otaManager.setFirmwareUrl(url.c_str());
            ESP_LOGI(TAG, "Starting HTTP OTA update: %s", url.c_str());
            Serial.println("[OTA] Starting HTTP update...");
            if (!otaManager.startHttpUpdate(url.c_str())) {
                Serial.println("[OTA] Update failed to start");
            }
        }
    }
    else if (command == "ota abort") {
        if (otaManager.getState() == xmesh::ota::OTAState::IDLE) {
            Serial.println("[OTA] No update in progress");
        } else {
            ESP_LOGI(TAG, "Aborting OTA update");
            otaManager.abort();
            Serial.println("[OTA] Update aborted");
        }
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
        auto& security = xmesh::security::SecurityManager::getInstance();
        Serial.printf("Security: Level=%d, Mode=%d, Devices=%d\n", 
                     (int)security.getSecurityLevel(), (int)security.getAuthMode(), 
                     security.getAuthorizedDeviceCount());
        Serial.printf("Neighbors: %d\n", gatewayBalancer.getNeighborCount());
        Serial.printf("Routing Table: %d entries\n", radio.routingTableSize());
        Serial.printf("Trickle TX: %lu, Suppressed: %lu\n", 
                     trickle.getTransmitCount(), trickle.getSuppressCount());
        uint8_t gatewayLoad = gatewayBalancer.peekLocalGatewayLoad();
        if (gatewayLoad == 255) {
            Serial.printf("Gateway Load: unknown\n");
        } else {
            Serial.printf("Gateway Load: %.1f ppm (encoded=%u)\n",
                          xmesh::GatewayBalancer::decodeGatewayLoad(gatewayLoad), gatewayLoad);
        }
        if (mobilityDetector.isEnabled()) {
            Serial.printf("Mobility: %s (variance: %.2f dB²)\n", 
                         mobilityDetector.getStateName(), mobilityDetector.getAggregateVariance());
            Serial.printf("Duty Cycle: %.1f%%\n", dutyCycleBudget.getUsagePercent());
        }
        Serial.printf("Free Heap: %lu bytes\n", esp_get_free_heap_size());
        Serial.println("======================");
    }
    else if (command == "reset trickle") {
        trickle.reset();
        Serial.println("[CMD] Trickle timer reset to I_min");
    }
    else if (command.startsWith("send ")) {
        String args = command.substring(5);
        uint16_t destAddr = strtoul(args.c_str(), NULL, 16);
        if (destAddr == 0) {
            Serial.println("[CMD] Usage: send XXXX (hex address)");
        } else {
            LoraMesher& radio = LoraMesher::getInstance();
            testPacket.counter = packetCounter++;
            size_t sentLen = 0;
            if (sendSecurePacket(destAddr, &testPacket, 1, &sentLen)) {
                dutyCycleBudget.recordAirtime(estimateAirtimeMs(sentLen));
                Serial.printf("[CMD] Sent packet #%lu to %04X\n", testPacket.counter, destAddr);
            } else {
                Serial.println("[CMD] Send failed (security)");
            }
        }
    }
    else if (command == "routes") {
        LoraMesher& radio = LoraMesher::getInstance();
        Serial.println("==== Routing Table ====");
        Serial.printf("Total entries: %d\n", radio.routingTableSize());
        RoutingTableService::printRoutingTable();
        Serial.println("=======================");
    }
    else if (command == "neighbors") {
        Serial.println("==== Neighbors ====");
        Serial.printf("Count: %d\n", gatewayBalancer.getNeighborCount());
        Serial.printf("ETX Tracked Links: %d\n", etxTracker.getNumTrackedLinks());
        Serial.println("===================");
    }
    else if (command == "mobility on") {
        mobilityDetector.enable();
        saveMobilityEnabled(true);
        Serial.println("[CMD] Mobility detection: ON");
    }
    else if (command == "mobility off") {
        mobilityDetector.disable();
        saveMobilityEnabled(false);
        Serial.println("[CMD] Mobility detection: OFF");
    }
    else if (command == "emergency") {
        if (!mobilityDetector.isEnabled()) {
            Serial.println("[CMD] Enable mobility first: mobility on");
        } else {
            mobilityDetector.triggerEmergency();
            applyMobilityParams(xmesh::MobilityState::EMERGENCY);
            Serial.println("[CMD] EMERGENCY state triggered");
        }
    }
    else if (command.startsWith("mobility simulate ")) {
        String state = command.substring(18);
        if (!mobilityDetector.isEnabled()) {
            Serial.println("[CMD] Enable mobility first: mobility on");
        } else if (state == "static") {
            // Force static by feeding consistent SNR
            for (int i = 0; i < 5; i++) mobilityDetector.feedSNR(0x0001, 10);
            Serial.println("[CMD] Simulating STATIC state");
        } else if (state == "mobile") {
            // Force mobile by feeding varying SNR
            for (int i = 0; i < 10; i++) mobilityDetector.feedSNR(0x0001, (i % 2) ? -5 : 15);
            Serial.println("[CMD] Simulating MOBILE state");
        } else if (state == "emergency") {
            mobilityDetector.triggerEmergency();
            applyMobilityParams(xmesh::MobilityState::EMERGENCY);
            Serial.println("[CMD] Simulating EMERGENCY state");
        } else {
            Serial.println("[CMD] Usage: mobility simulate <static|mobile|emergency>");
        }
    }
    else if (command == "dutycycle") {
        Serial.println("==== Duty Cycle ====");
        Serial.printf("Usage: %.1f%% of 1%% limit\n", dutyCycleBudget.getUsagePercent());
        Serial.printf("Remaining: %lu ms (of 36000ms/hour)\n", dutyCycleBudget.getRemainingBudgetMs());
        Serial.printf("Exhausted: %s\n", dutyCycleBudget.isExhausted() ? "YES" : "NO");
        Serial.println("====================");
    }
    // Security commands
    else if (command.startsWith("security ")) {
        auto& security = xmesh::security::SecurityManager::getInstance();
        String sub = command.substring(9);
        if (sub == "status") {
            Serial.println("==== Security Status ====");
            Serial.printf("Level: %d\n", (int)security.getSecurityLevel());
            Serial.printf("Frame Counter: %lu\n", security.getFrameCounterOut());
            Serial.printf("Auth Mode: %d\n", (int)security.getAuthMode());
            Serial.printf("Devices: %d\n", security.getAuthorizedDeviceCount());
            Serial.printf("Replay Rejects: %lu\n", security.getReplayRejectCount());
            Serial.println("=========================");
        }
        else if (sub.startsWith("level ")) {
            int level = sub.substring(6).toInt();
            security.setSecurityLevel(static_cast<xmesh::security::SecurityLevel>(level));
            security.persist();
            Serial.printf("[CMD] Security level set to: %d (persisted)\n", level);
        }
        else if (sub.startsWith("key ")) {
            String password = sub.substring(4);
            security.setEncryptionKeyFromPassword(password.c_str());
            security.persist();
            Serial.println("[CMD] Security key derived from password (persisted)");
        }
        else if (sub == "devices") {
            Serial.printf("[CMD] Authorized devices: %d\n", security.getAuthorizedDeviceCount());
        }
        else if (sub.startsWith("add ")) {
            uint16_t addr = strtoul(sub.substring(4).c_str(), NULL, 16);
            if (security.addAuthorizedDevice(addr)) {
                security.persist();
                Serial.printf("[CMD] Device %04X added (persisted)\n", addr);
            } else {
                Serial.println("[CMD] Failed to add device");
            }
        }
        else if (sub.startsWith("remove ")) {
            uint16_t addr = strtoul(sub.substring(7).c_str(), NULL, 16);
            if (security.removeAuthorizedDevice(addr)) {
                security.persist();
                Serial.printf("[CMD] Device %04X removed (persisted)\n", addr);
            } else {
                Serial.println("[CMD] Failed to remove device");
            }
        }
        else if (sub.startsWith("mode ")) {
            int mode = sub.substring(5).toInt();
            security.setAuthMode(static_cast<xmesh::security::AuthMode>(mode));
            security.persist();
            Serial.printf("[CMD] Auth mode set to: %d (persisted)\n", mode);
        }
    }
    // Sensor commands
    else if (command == "sensors status") {
        Serial.println("==== Sensor Status ====");
        Serial.printf("Node Mode: %s\n", getNodeModeName(sensors.getNodeMode()));
        Serial.printf("PMS Detected: %s\n", sensors.isPMSDetected() ? "YES" : "NO");
        Serial.printf("GPS Detected: %s\n", sensors.isGPSDetected() ? "YES" : "NO");
        if (sensors.isPMSDetected()) {
            Serial.printf("PMS State: %s\n", 
                sensors.getPMSState() == xmesh::hal::PMSState::OFF ? "OFF" :
                sensors.getPMSState() == xmesh::hal::PMSState::WARMING ? "WARMING" : "READY");
        }
        Serial.printf("Sensor TX Count: %lu\n", sensorTxCount);
        Serial.println("=======================");
    }
    else if (command == "sensors detect") {
        Serial.println("[SENSORS] Re-running detection...");
        sensors.detectPMS(PMS_DETECT_TIMEOUT_MS);
        sensors.detectGPS(GPS_DETECT_TIMEOUT_MS);
        Serial.printf("[SENSORS] PMS: %s, GPS: %s\n",
            sensors.isPMSDetected() ? "detected" : "not found",
            sensors.isGPSDetected() ? "detected" : "not found");
        Serial.printf("[SENSORS] Node mode: %s\n", getNodeModeName(sensors.getNodeMode()));
    }
    else if (command == "sensors read") {
        if (!sensors.isPMSDetected()) {
            Serial.println("[SENSORS] No PMS sensor detected");
        } else if (sensors.getPMSState() != xmesh::hal::PMSState::READY) {
            Serial.println("[SENSORS] PMS not ready - use 'sensors power on' and wait 30s");
        } else {
            auto aq = sensors.readAirQuality();
            Serial.printf("[SENSORS] PM1.0=%d, PM2.5=%d, PM10=%d\n", aq.pm1_0, aq.pm2_5, aq.pm10);
        }
        if (sensors.isGPSDetected()) {
            auto gps = sensors.readGPS();
            Serial.printf("[SENSORS] GPS: lat=%ld, lon=%ld, alt=%d, sats=%d\n",
                gps.latitude, gps.longitude, gps.altitude, gps.satellites);
        }
    }
    else if (command == "sensors send") {
        if (sensors.getNodeMode() == xmesh::hal::NodeMode::RELAY) {
            Serial.println("[SENSORS] No sensors - cannot send");
        } else if (sensors.getPMSState() != xmesh::hal::PMSState::READY) {
            Serial.println("[SENSORS] PMS not ready - wait for warmup");
        } else {
            LoraMesher& radio = LoraMesher::getInstance();
            auto aq = sensors.readAirQuality();
            auto gps = sensors.readGPS();
            xmesh::hal::SensorPacket sp = {};
            sp.version = xmesh::hal::SENSOR_PACKET_VERSION;
            sp.flags = xmesh::hal::FLAG_PMS_VALID;
            if (gps.satellites > 0) sp.flags |= xmesh::hal::FLAG_GPS_VALID;
            sp.pm1_0 = aq.pm1_0;
            sp.pm2_5 = aq.pm2_5;
            sp.pm10 = aq.pm10;
            sp.latitude = gps.latitude;
            sp.longitude = gps.longitude;
            sp.altitude = gps.altitude;
            sp.satellites = gps.satellites;
            sp.timestamp = millis() / 1000;
            size_t sentLen = 0;
            if (sendSecurePacket(BROADCAST_ADDR, reinterpret_cast<const uint8_t*>(&sp), sizeof(sp), &sentLen)) {
                dutyCycleBudget.recordAirtime(estimateAirtimeMs(sentLen));
                sensorTxCount++;
                Serial.printf("[SENSORS] Force TX #%lu: PM2.5=%d\n", sensorTxCount, sp.pm2_5);
            } else {
                Serial.println("[SENSORS] Send failed (security)");
            }
        }
    }
    else if (command == "sensors power on") {
        if (!sensors.isPMSDetected()) {
            Serial.println("[SENSORS] No PMS sensor detected");
        } else {
            sensors.setPMSPower(true);
            Serial.println("[SENSORS] PMS power ON - warming up (30s)");
        }
    }
    else if (command == "sensors power off") {
        if (!sensors.isPMSDetected()) {
            Serial.println("[SENSORS] No PMS sensor detected");
        } else {
            sensors.setPMSPower(false);
            Serial.println("[SENSORS] PMS power OFF");
        }
    }
    // MQTT commands
    else if (command.startsWith("mqtt ") && command.length() > 5 && command.substring(5) != "status") {
        String broker = command.substring(5);
        broker.trim();
        strncpy(mqttBroker, broker.c_str(), sizeof(mqttBroker) - 1);
        mqttBroker[sizeof(mqttBroker) - 1] = '\0';
        mqttEnabled = false;
        nvs_set_str(nvsHandle, "mqtt_broker", mqttBroker);
        nvs_set_u16(nvsHandle, "mqtt_port", mqttPort);
        nvs_commit(nvsHandle);
        Serial.printf("[MQTT] Broker set to: %s\n", mqttBroker);
        if (config.isGateway && wifiConnected) {
            mqttClient.setServer(mqttBroker, mqttPort);
            Serial.println("[MQTT] Will connect on next loop");
        }
    }
    else if (command == "mqtt status") {
        Serial.println("==== MQTT Status ====");
        Serial.printf("Broker: %s\n", strlen(mqttBroker) > 0 ? mqttBroker : "(not set)");
        Serial.printf("Port: %d\n", mqttPort);
        Serial.printf("Enabled: %s\n", mqttEnabled ? "YES" : "NO");
        Serial.printf("Connected: %s\n", mqttClient.connected() ? "YES" : "NO");
        Serial.printf("Gateway Mode: %s\n", config.isGateway ? "YES" : "NO");
        Serial.printf("WiFi Connected: %s\n", wifiConnected ? "YES" : "NO");
        Serial.println("=====================");
    }
    else if (command == "help") {
        Serial.println("Available commands:");
        Serial.println("  gateway on/off  - Toggle gateway mode");
        Serial.println("  wifi SSID PASS  - Set WiFi credentials");
        Serial.println("  wifi on/off     - Enable/disable WiFi for OTA");
        Serial.println("  wifi scan       - Scan for WiFi networks");
        Serial.println("  ota status      - Show OTA state and version info");
        Serial.println("  ota check       - Check for updates (configured URL)");
        Serial.println("  ota check <url> - Check for updates from URL");
        Serial.println("  ota url <url>   - Set firmware URL for HTTP OTA");
        Serial.println("  ota update      - Start HTTP update (configured URL)");
        Serial.println("  ota update <url> - Start HTTP update from URL");
        Serial.println("  ota abort       - Abort OTA update in progress");
        Serial.println("  status          - Show node status");
        Serial.println("  routes          - Show routing table");
        Serial.println("  neighbors       - Show neighbor info");
        Serial.println("  send XXXX       - Send test packet to address (hex)");
        Serial.println("  reset trickle   - Reset Trickle timer");
        Serial.println("  mobility on/off - Enable/disable mobility detection");
        Serial.println("  mobility simulate <state> - Simulate static/mobile/emergency");
        Serial.println("  emergency       - Trigger emergency state");
        Serial.println("  dutycycle       - Show duty cycle usage");
        Serial.println("  sensors status  - Show sensor detection and status");
        Serial.println("  sensors detect  - Re-run sensor detection");
        Serial.println("  sensors read    - Force immediate sensor read");
        Serial.println("  sensors send    - Force immediate mesh transmission");
        Serial.println("  sensors power on/off - Manual PMS power control");
        Serial.println("  mqtt <broker>   - Set MQTT broker hostname");
        Serial.println("  mqtt status     - Show MQTT connection status");
        Serial.println("  security status - Show security info");
        Serial.println("  security level N - Set security level (0-3)");
        Serial.println("  security key P - Set key from password");
        Serial.println("  security devices - Show authorized device count");
        Serial.println("  security add X - Add device address (hex)");
        Serial.println("  security remove X - Remove device address (hex)");
        Serial.println("  security mode N - Set auth mode (0-2)");
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
    
    // Create hostname reference (kept for reference as per instructions)
    String hostname = "xmesh-";
    hostname += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    
    // OTAManager handles setup internally (hostname, callbacks, safety)
    otaInitialized = otaManager.begin();
    
    if (otaInitialized) {
        ESP_LOGI(TAG, "OTA service started (Managed) - reference hostname: %s", hostname.c_str());
        Serial.printf("[OTA] Ready - Managed updates active\n");
    } else {
        ESP_LOGE(TAG, "OTA initialization failed");
    }
}

void disconnectWiFi() {
    if (otaInitialized) {
        otaInitialized = false;
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    ESP_LOGI(TAG, "WiFi disconnected");
    Serial.println("[WiFi] Disconnected");
}

void applyMobilityParams(xmesh::MobilityState state) {
    switch (state) {
        case xmesh::MobilityState::STATIC:
            trickle.setIMin(60000);
            trickle.setIMax(600000);
            gatewayBalancer.setWarningThreshold(180000);
            gatewayBalancer.setDetectionThreshold(360000);
            ESP_LOGI(TAG, "Mobility: STATIC (I=60-600s, detect=360s)");
            break;
        case xmesh::MobilityState::MOBILE:
            trickle.setIMin(20000);
            trickle.setIMax(120000);
            gatewayBalancer.setWarningThreshold(90000);
            gatewayBalancer.setDetectionThreshold(180000);
            ESP_LOGI(TAG, "Mobility: MOBILE (I=20-120s, detect=180s)");
            break;
        case xmesh::MobilityState::EMERGENCY:
            trickle.setIMin(10000);
            trickle.setIMax(60000);
            trickle.reset();  // Immediate reset to I_min
            gatewayBalancer.setWarningThreshold(30000);
            gatewayBalancer.setDetectionThreshold(90000);
            ESP_LOGI(TAG, "Mobility: EMERGENCY (I=10-60s, detect=90s)");
            break;
    }
}

uint32_t estimateAirtimeMs(size_t payloadLen) {
    // SF7, BW125kHz, CR4/5 - simplified formula
    // For small packets (4-20 bytes): ~37-50ms
    const float symbolTime = 1.024f;  // 2^7 / 125000 * 1000 ms
    const float preambleTime = 12.5f * symbolTime;  // 8 + 4.25 symbols
    float payloadSymbols = 8.0f + (8.0f * payloadLen + 28.0f) / 28.0f * 5.0f;
    return (uint32_t)(preambleTime + payloadSymbols * symbolTime + 0.5f);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    
    if (display.begin()) {
        ESP_LOGI(TAG, "OLED display initialized");
        display.clear();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.println("xMESH Starting...");
        display.display();
    } else {
        ESP_LOGW(TAG, "OLED display init failed");
    }

    // Initialize sensors
    sensors.setPMSSetPin(PMS_SET_PIN);
    sensors.setWarmupMs(PMS_WARMUP_MS);
    sensors.setReadIntervalMs(SENSOR_READ_INTERVAL_MS);
    
    if (ENABLE_PMS_SENSOR) {
        pmsSerial.begin(PMS_BAUD, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);
        if (sensors.beginAirQuality(&pmsSerial)) {
            ESP_LOGI(TAG, "PMS7003 sensor initialized");
            sensors.detectPMS(PMS_DETECT_TIMEOUT_MS);
        }
    }

    if (ENABLE_GPS_SENSOR) {
        gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        if (sensors.beginGPS(&gpsSerial)) {
            ESP_LOGI(TAG, "GPS sensor initialized");
            sensors.detectGPS(GPS_DETECT_TIMEOUT_MS);
        }
    }
    
    ESP_LOGI(TAG, "Node mode: %s", getNodeModeName(sensors.getNodeMode()));
    
    if (sensors.isPMSDetected()) {
        sensors.setPMSPower(false);
    }

    initNVS();
    loadConfig();

    // Initialize security (default: NONE for backward compatibility)
    auto& security = xmesh::security::SecurityManager::getInstance();
    security.begin(xmesh::security::SecurityLevel::NONE);
    ESP_LOGI(TAG, "Security initialized (level: NONE)");

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
    
    config.freq = LORA_FREQUENCY / 1000000.0f;  // Hz to MHz conversion required by LoraMesher
    config.bw = LORA_BANDWIDTH;
    config.sf = LORA_SPREADING_FACTOR;
    config.cr = LORA_CODING_RATE;
    config.power = LORA_TX_POWER;
    
    radio.begin(config);
    
    Serial.printf("[LoRaMesher] Initialized with address: %04X\n", radio.getLocalAddress());
    
    createReceiveMessagesTask();
    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
    
    radio.start();
    Serial.printf("[LoRaMesher] Radio started\n");
    
    RoutingTableService::setCostCalculationCallback(costCalculationCallback);
    RoutingTableService::setHelloReceivedCallback(helloReceivedCallback);
    Serial.printf("[xMESH] Cost-based routing enabled (W1=%.1f, W2=%.1f, W3=%.1f, W4=%.1f, W5=%.1f)\n",
                  W1_HOP_COUNT, W2_RSSI, W3_SNR, W4_ETX, W5_GATEWAY_BIAS);
    
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

        gatewayBalancer.sampleLocalGatewayLoadForHello();
        
        LoraMesher& radio = LoraMesher::getInstance();
        
        testPacket.counter = packetCounter++;
        size_t sentLen = 0;
        if (sendSecurePacket(BROADCAST_ADDR, &testPacket, 1, &sentLen)) {
            dutyCycleBudget.recordAirtime(estimateAirtimeMs(sentLen));
        } else {
            ESP_LOGW(TAG, "Trickle TX failed (security)");
        }
    }
    
    uint32_t now = millis();
    
    sensors.updatePowerState();
    
    if (sensors.getNodeMode() == xmesh::hal::NodeMode::SENSOR &&
        sensors.getPMSState() == xmesh::hal::PMSState::READY &&
        now - lastSensorTx >= SENSOR_READ_INTERVAL_MS) {
        
        if (!dutyCycleBudget.isExhausted()) {
            LoraMesher& radio = LoraMesher::getInstance();
            
            auto aq = sensors.readAirQuality();
            auto gps = sensors.readGPS();
            
            xmesh::hal::SensorPacket sp = {};
            sp.version = xmesh::hal::SENSOR_PACKET_VERSION;
            sp.flags = 0;
            sp.timestamp = now;
            
            if (aq.valid) {
                sp.flags |= xmesh::hal::FLAG_PMS_VALID;
                sp.pm1_0 = aq.pm1_0;
                sp.pm2_5 = aq.pm2_5;
                sp.pm10 = aq.pm10;
            }
            
            if (gps.valid) {
                sp.flags |= xmesh::hal::FLAG_GPS_VALID | xmesh::hal::FLAG_GPS_FIX;
                sp.latitude = static_cast<int32_t>(gps.latitude * 1e7);
                sp.longitude = static_cast<int32_t>(gps.longitude * 1e7);
                sp.altitude = static_cast<int16_t>(gps.altitude);
                sp.satellites = gps.satellites;
            }
            
            uint16_t destAddr = BROADCAST_ADDR;
            RouteNode* gatewayNode = radio.getClosestGateway();
            if (gatewayNode != nullptr) {
                destAddr = gatewayNode->networkNode.address;
            }
            ESP_LOGD(TAG, "Sending sensor packet to %04X", destAddr);
            size_t sentLen = 0;
            if (sendSecurePacket(destAddr, &sp, 1, &sentLen)) {
                dutyCycleBudget.recordAirtime(estimateAirtimeMs(sentLen));
            } else {
                ESP_LOGW(TAG, "Sensor TX failed (security)");
            }
            
            sensorTxCount++;
            lastSensorTx = now;
            
            ESP_LOGI(TAG, "Sensor TX #%lu: PM2.5=%d, lat=%ld", sensorTxCount, sp.pm2_5, sp.latitude);
            
            sensors.setPMSPower(false);
        }
    }
    
    if (now - lastMonitorCheck >= MONITOR_INTERVAL_MS) {
        lastMonitorCheck = now;
        
        LoraMesher& radio = LoraMesher::getInstance();
        uint8_t failedNeighbors = gatewayBalancer.monitorNeighborHealth();
        if (failedNeighbors > 0) {
            Serial.printf("[GatewayBalancer] Detected %d failed neighbors\n", failedNeighbors);
            
            for (uint8_t i = 0; i < gatewayBalancer.getNeighborCount(); i++) {
                uint16_t addr = gatewayBalancer.getNeighborAddress(i);
                if (addr != 0 && gatewayBalancer.isNeighborFailed(addr)) {
                    radio.deleteRoute(addr);
                    ESP_LOGW(TAG, "Removed route for failed neighbor %04X", addr);
                }
            }
        }
        
        Serial.printf("[Status] Tracked Links: %d, Neighbors: %d, Routing Table Size: %d\n",
                     etxTracker.getNumTrackedLinks(),
                     gatewayBalancer.getNeighborCount(),
                     radio.routingTableSize());

        if (config.isGateway) {
            uint8_t gatewayLoad = gatewayBalancer.peekLocalGatewayLoad();
            if (gatewayLoad == 255) {
                Serial.printf("[GatewayBalancer] Local gateway load: unknown\n");
            } else {
                Serial.printf("[GatewayBalancer] Local gateway load: %.1f ppm (encoded=%u)\n",
                              xmesh::GatewayBalancer::decodeGatewayLoad(gatewayLoad), gatewayLoad);
            }
        }
        
        if (!appMarkedValid && radio.routingTableSize() > 0) {
            // Mark app valid after confirming mesh is operational
            otaManager.markAppValid();
            appMarkedValid = true;
            ESP_LOGI(TAG, "App marked valid - mesh confirmed operational");
        }
        
        if (mobilityDetector.isEnabled() && otaManager.getState() == xmesh::ota::OTAState::IDLE) {
            auto prevState = mobilityDetector.getState();
            mobilityDetector.tick(trickle.isAtMaxInterval());
            auto newState = mobilityDetector.getState();
            if (newState != prevState) {
                applyMobilityParams(newState);
            }
        }
        
        dutyCycleBudget.tick();
        
        updateDisplay();
    }
    
    if (now - lastSecurityCleanup >= SECURITY_CLEANUP_INTERVAL_MS) {
        lastSecurityCleanup = now;
        auto& security = xmesh::security::SecurityManager::getInstance();
        uint8_t cleanedPeers = security.getFrameCounter().cleanupStalePeers();
        security.getKeyManager().cleanupOldKeys();
        if (cleanedPeers > 0) {
            ESP_LOGI(TAG, "Security cleanup: removed %d stale peers", cleanedPeers);
        }
    }
    
    if (otaInitialized) {
        otaManager.process();
    }
    
    if (config.isGateway && wifiConnected && strlen(mqttBroker) > 0) {
        if (!mqttClient.connected()) {
            mqttReconnect();
        }
        mqttClient.loop();
    }

    sensors.update();

    delay(1000);
}
