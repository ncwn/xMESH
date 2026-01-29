#ifndef XMESH_HAL_SENSOR_PACKET_H
#define XMESH_HAL_SENSOR_PACKET_H

#include <cstdint>

namespace xmesh {
namespace hal {

// Packet version for forward compatibility
constexpr uint8_t SENSOR_PACKET_VERSION = 1;

// Validity flags (bitfield)
constexpr uint8_t FLAG_PMS_VALID = 0x01;   // PM sensor data valid
constexpr uint8_t FLAG_GPS_VALID = 0x02;   // GPS location valid
constexpr uint8_t FLAG_GPS_FIX   = 0x04;   // GPS has recent fix

// Node operating mode
enum class NodeMode : uint8_t {
    RELAY = 0,      // No sensors, pure relay
    SENSOR = 1,     // Has sensors, transmitting
    GATEWAY = 2     // Gateway node
};

// 23-byte sensor data packet for mesh transmission
struct __attribute__((packed)) SensorPacket {
    uint8_t version;        // 1 byte - format version
    uint8_t flags;          // 1 byte - validity flags
    uint16_t pm1_0;         // 2 bytes - PM1.0 ug/m3
    uint16_t pm2_5;         // 2 bytes - PM2.5 ug/m3
    uint16_t pm10;          // 2 bytes - PM10 ug/m3
    int32_t latitude;       // 4 bytes - lat * 1e7
    int32_t longitude;      // 4 bytes - lon * 1e7
    int16_t altitude;       // 2 bytes - meters
    uint8_t satellites;     // 1 byte - sat count
    uint32_t timestamp;     // 4 bytes - uptime ms
};

static_assert(sizeof(SensorPacket) == 23, "SensorPacket must be 23 bytes");

} // namespace hal
} // namespace xmesh

#endif // XMESH_HAL_SENSOR_PACKET_H
