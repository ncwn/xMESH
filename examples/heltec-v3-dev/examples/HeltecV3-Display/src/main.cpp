#include <Arduino.h>
#include "LoraMesher.h"
#include "display.h"

// Heltec LoRa 32 V3 Pin Definitions
#define LORA_CS     8   // NSS/CS pin
#define LORA_IRQ    14  // DIO1 pin (interrupt)
#define LORA_RST    12  // Reset pin
#define LORA_IO1    13  // BUSY pin (NOT DIO1 - this goes to GPIO/BUSY parameter in RadioLib)
#define LORA_BUSY   13  // BUSY pin (same as LORA_IO1)

// Custom SPI pins for Heltec V3
#define LORA_MOSI   10
#define LORA_MISO   11
#define LORA_SCK    9

// LED pin for Heltec V3
#define BOARD_LED   35
#define LED_ON      HIGH
#define LED_OFF     LOW

// Vext pin for powering peripherals (OLED, LoRa)
#define Vext        36
#define Vext_ON     LOW   // Active low
#define Vext_OFF    HIGH

LoraMesher& radio = LoraMesher::getInstance();

uint32_t dataCounter = 0;
struct dataPacket {
    uint32_t counter = 0;
};

dataPacket* helloPacket = new dataPacket;

// Custom SPI instance for Heltec V3
SPIClass customSPI(HSPI);

/**
 * @brief LED flash function
 * 
 * @param flashes Number of flashes
 * @param delaymS Delay in milliseconds
 */
void led_Flash(uint16_t flashes, uint16_t delaymS) {
    uint16_t index;
    for (index = 1; index <= flashes; index++) {
        digitalWrite(BOARD_LED, LED_ON);
        delay(delaymS);
        digitalWrite(BOARD_LED, LED_OFF);
        delay(delaymS);
    }
}

/**
 * @brief Print the counter of the packet
 *
 * @param data
 */
void printPacket(dataPacket data) {
    Serial.printf("Hello Counter received nº %d\n", data.counter);
    
    // Update display with RX counter (will be shown on line 3)
    String msg = "RX: #" + String(data.counter);
    Screen.changeLineThree(msg);
}

/**
 * @brief Iterate through the payload of the packet and print the counter of the packet
 *
 * @param packet
 */
void printDataPacket(AppPacket<dataPacket>* packet) {
    Serial.printf("Packet arrived from %X with size %d\n", packet->src, packet->payloadSize);

    //Get the payload to iterate through it
    dataPacket* dPacket = packet->payload;
    size_t payloadLength = packet->getPayloadLength();

    for (size_t i = 0; i < payloadLength; i++) {
        //Print the packet
        printPacket(dPacket[i]);
    }
}

/**
 * @brief Function that process the received packets
 *
 */
void processReceivedPackets(void*) {
    for (;;) {
        /* Wait for the notification of processReceivedPackets and enter blocking */
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);
        led_Flash(1, 100); //one quick LED flash to indicate a packet has arrived

        //Iterate through all the packets inside the Received User Packets Queue
        while (radio.getReceivedQueueSize() > 0) {
            Serial.println("ReceivedUserData_TaskHandle notify received");
            Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());

            //Get the first element inside the Received User Packets Queue
            AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();

            //Print the data packet
            printDataPacket(packet);

            //Delete the packet when used. It is very important to call this function to release the memory of the packet.
            radio.deletePacket(packet);
        }
    }
}

TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create a Receive Messages Task and add it to the LoRaMesher
 *
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
    }

    radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
}

/**
 * @brief Task to update the display periodically
 */
void displayTask(void* parameter) {
    for (;;) {
        Screen.drawDisplay();
        
        // Update routing table size
        Screen.changeSizeRouting(radio.routingTableSize());
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

TaskHandle_t displayTaskHandle = NULL;

/**
 * @brief Create the display update task
 */
void createDisplayTask() {
    int res = xTaskCreate(
        displayTask,
        "Display Task",
        2048,
        (void*) 1,
        1,
        &displayTaskHandle);
    if (res != pdPASS) {
        Serial.printf("Error: Display Task creation gave error: %d\n", res);
    }
}

/**
 * @brief Initialize LoRaMesher for Heltec LoRa 32 V3
 *
 */
void setupLoraMesher() {
    // Initialize custom SPI with Heltec V3 pins
    customSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    
    // Configure LoRaMesher for Heltec V3
    LoraMesher::LoraMesherConfig config;
    config.module = LoraMesher::LoraModules::SX1262_MOD;
    config.loraCs = LORA_CS;
    config.loraIrq = LORA_IRQ;
    config.loraRst = LORA_RST;
    config.loraIo1 = LORA_IO1;
    config.spi = &customSPI;
    
    // LoRa parameters for Thailand/Asia (trying 915 MHz - closest supported frequency)
    // Note: Check your Heltec V3 variant - it may be 868/915 MHz or 923 MHz version
    config.freq = 915.0;      // 915 MHz (US915/AS923 compatible)
    config.bw = 125.0;        // 125 kHz bandwidth
    config.sf = 7;            // Spreading factor 7
    config.cr = 7;            // Coding rate 4/7
    config.syncWord = 0x12;   // Private sync word
    config.power = 14;        // 14 dBm output power
    config.preambleLength = 8;
    
    Serial.println("Initializing LoRaMesher for Heltec LoRa 32 V3...");
    
    //Init the loramesher with a processReceivedPackets function
    radio.begin(config);

    //Create the receive task and add it to the LoRaMesher
    createReceiveMessages();

    //Start LoRaMesher
    radio.start();

    Serial.println("LoRaMesher initialized successfully!");
    Serial.printf("Local address: 0x%X\n", radio.getLocalAddress());
    
    // Update display with local address and clear status
    String localAddr = "0x" + String(radio.getLocalAddress(), HEX);
    Screen.changeLineOne(localAddr);
    Screen.changeLineTwo("Ready");
    Screen.changeLineThree("");
    Screen.changeLineFour("");
    Screen.drawDisplay();
}

void setup() {
    Serial.begin(115200);
    delay(1500); // Give time for serial monitor to connect
    
    Serial.println("\n\n========================================");
    Serial.println("Heltec V3 LoRaMesher with Display");
    Serial.println("========================================\n");

    // Enable Vext to power OLED and LoRa peripherals
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, Vext_ON);
    delay(100); // Wait for power to stabilize
    
    pinMode(BOARD_LED, OUTPUT);
    led_Flash(2, 125); // Two quick LED flashes to indicate program start
    
    // Initialize display first
    Screen.initDisplay();
    delay(1000);
    
    // Setup LoRaMesher
    setupLoraMesher();
    
    // Create display update task
    createDisplayTask();
}

void loop() {
    for (;;) {
        // Flash LED to indicate transmission
        led_Flash(1, 50);
        
        Serial.printf("Sending packet #%d\n", dataCounter);

        helloPacket->counter = dataCounter;
        
        // Update display with TX info on line 2
        String txMsg = "TX: #" + String(dataCounter);
        Screen.changeLineTwo(txMsg);
        
        dataCounter++;

        //Create packet and send it.
        radio.createPacketAndSend(BROADCAST_ADDR, helloPacket, 1);

        //Wait 20 seconds to send the next packet
        vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
}
