#include <unity.h>
#include "../../mocks/MockFreeRTOS.h"

// Arduino stubs for Sensors (not in MockFreeRTOS)
void pinMode(int pin, int mode) {}
void digitalWrite(int pin, int val) {}
int digitalRead(int pin) { return 0; }
void delay(uint32_t ms) { vTaskDelay(ms); }

#define HIGH 0x1
#define LOW  0x0
#define INPUT 0x0
#define OUTPUT 0x1

class HardwareSerial {
public:
    void begin(unsigned long baud) {}
    void end() {}
    int available() { return 0; }
    int read() { return -1; }
};
HardwareSerial Serial1;
HardwareSerial Serial2;

#define private public
#include "../../../lib/xmesh-hal/src/Sensors.cpp"
#undef private

using namespace xmesh::hal;

void setUp(void) {
    mock::reset();
}

void tearDown(void) {}

void test_sensors_get_node_mode_relay_default(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    sensors.pms_detected_ = false;
    sensors.gps_detected_ = false;
    TEST_ASSERT_EQUAL(NodeMode::RELAY, sensors.getNodeMode());
}

void test_sensors_get_node_mode_sensor_when_detected(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    sensors.pms_detected_ = true;
    TEST_ASSERT_EQUAL(NodeMode::SENSOR, sensors.getNodeMode());
}

void test_sensors_update_power_state_transitions(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    sensors.pms_detected_ = true;
    sensors.pms_state_ = PMSState::SLEEPING;
    sensors.last_read_ms_ = 0;
    sensors.read_interval_ms_ = 60000;
    sensors.pms_warmup_ms_ = 30000;
    sensors.pms_set_pin_ = 3; // Default in header is 3
    
    // Transition from SLEEPING to WARMING
    // now(0) - last_read(0) >= 60000 - 30000 -> 0 >= 30000 (False)
    sensors.updatePowerState();
    TEST_ASSERT_EQUAL(PMSState::SLEEPING, sensors.getPMSState());
    
    mock::set_millis(30000);
    // now(30000) - last_read(0) >= 60000 - 30000 -> 30000 >= 30000 (True)
    sensors.updatePowerState();
    TEST_ASSERT_EQUAL(PMSState::WARMING, sensors.getPMSState());
    
    // Transition from WARMING to READY
    // warmup_start_ms was set to 30000 during the SLEEPING -> WARMING transition
    mock::advance_millis(30000); // now is 60000
    // now(60000) - warmup_start_ms(30000) >= 30000 (True)
    sensors.updatePowerState();
    TEST_ASSERT_EQUAL(PMSState::READY, sensors.getPMSState());
}

void test_sensors_is_pms_detected_returns_flag(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    sensors.pms_detected_ = false;
    TEST_ASSERT_FALSE(sensors.isPMSDetected());
    sensors.pms_detected_ = true;
    TEST_ASSERT_TRUE(sensors.isPMSDetected());
}

void test_sensors_set_pms_set_pin_updates_field(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    
    // Default pin is 3
    TEST_ASSERT_EQUAL_UINT8(3, sensors.pms_set_pin_);
    
    // Set to new pin
    sensors.setPMSSetPin(5);
    TEST_ASSERT_EQUAL_UINT8(5, sensors.pms_set_pin_);
}

void test_sensors_set_warmup_ms_updates_field(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    
    // Default warmup is 30000
    TEST_ASSERT_EQUAL_UINT32(30000, sensors.pms_warmup_ms_);
    
    // Set to new warmup time
    sensors.setWarmupMs(20000);
    TEST_ASSERT_EQUAL_UINT32(20000, sensors.pms_warmup_ms_);
}

void test_sensors_set_read_interval_ms_updates_field(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    
    // Default read interval is 60000
    TEST_ASSERT_EQUAL_UINT32(60000, sensors.read_interval_ms_);
    
    // Set to new read interval
    sensors.setReadIntervalMs(120000);
    TEST_ASSERT_EQUAL_UINT32(120000, sensors.read_interval_ms_);
}

void test_sensors_is_gps_detected_returns_flag(void) {
    Sensors sensors;
    sensors.pms_ = nullptr;
    sensors.gps_ = nullptr;
    sensors.pms_serial_ = nullptr;
    sensors.gps_serial_ = nullptr;
    sensors.gps_detected_ = false;
    TEST_ASSERT_FALSE(sensors.isGPSDetected());
    sensors.gps_detected_ = true;
    TEST_ASSERT_TRUE(sensors.isGPSDetected());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_sensors_get_node_mode_relay_default);
    RUN_TEST(test_sensors_get_node_mode_sensor_when_detected);
    RUN_TEST(test_sensors_update_power_state_transitions);
    RUN_TEST(test_sensors_is_pms_detected_returns_flag);
    RUN_TEST(test_sensors_set_pms_set_pin_updates_field);
    RUN_TEST(test_sensors_set_warmup_ms_updates_field);
    RUN_TEST(test_sensors_set_read_interval_ms_updates_field);
    RUN_TEST(test_sensors_is_gps_detected_returns_flag);
    return UNITY_END();
}
