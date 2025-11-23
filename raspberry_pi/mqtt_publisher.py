#!/usr/bin/env python3
"""
xMESH MQTT Publisher for AIT Hazemon Infrastructure
Bridges LoRa mesh gateway to MQTT broker for real-time monitoring

Usage:
    python3 mqtt_publisher.py --port /dev/ttyUSB0 --config mqtt_config.json
"""

import serial
import json
import argparse
import signal
import sys
from pathlib import Path
import paho.mqtt.client as mqtt
from datetime import datetime
import struct
import re
import ssl
import certifi

class xMESH_MQTT_Publisher:
    def __init__(self, serial_port, baudrate, mqtt_config_file):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.serial_conn = None
        self.mqtt_client = None
        self.running = False

        # Load MQTT configuration
        with open(mqtt_config_file, 'r') as f:
            self.mqtt_config = json.load(f)

        # Statistics
        self.stats = {
            'packets_received': 0,
            'packets_published': 0,
            'mqtt_errors': 0,
            'start_time': None
        }
        self.current_packet = None

    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            print(f"✅ Connected to MQTT broker: {self.mqtt_config['broker']}")
            print(f"   Topics: {', '.join(self.mqtt_config['topics'].values())}")
        else:
            print(f"❌ MQTT connection failed with code {rc}")

    def on_mqtt_disconnect(self, client, userdata, rc):
        """MQTT disconnection callback"""
        if rc != 0:
            print(f"⚠️  Unexpected MQTT disconnection (code {rc})")

    def on_mqtt_publish(self, client, userdata, mid):
        """MQTT publish callback"""
        self.stats['packets_published'] += 1

    def setup_mqtt(self):
        """Initialize MQTT connection to AIT Hazemon"""
        try:
            self.mqtt_client = mqtt.Client(
                client_id=self.mqtt_config['client_id'],
                clean_session=self.mqtt_config.get('clean_session', True)
            )

            # Set callbacks
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
            self.mqtt_client.on_publish = self.on_mqtt_publish

            # TLS configuration
            if self.mqtt_config.get('use_tls', False):
                ca_path = self.mqtt_config.get('ca_cert') or certifi.where()
                self.mqtt_client.tls_set(
                    ca_certs=ca_path,
                    tls_version=ssl.PROTOCOL_TLS_CLIENT
                )
                self.mqtt_client.tls_insecure_set(self.mqtt_config.get('tls_insecure', False))

            # Set credentials
            if self.mqtt_config.get('username') and self.mqtt_config.get('password'):
                if self.mqtt_config['username'] != "YOUR_AIT_USERNAME":
                    self.mqtt_client.username_pw_set(
                        self.mqtt_config['username'],
                        self.mqtt_config['password']
                    )
                else:
                    print("⚠️  WARNING: Update mqtt_config.json with your AIT credentials!")

            # Connect
            self.mqtt_client.connect(
                self.mqtt_config['broker'],
                self.mqtt_config['port'],
                self.mqtt_config.get('keepalive', 60)
            )

            # Start network loop in background
            self.mqtt_client.loop_start()

            return True

        except Exception as e:
            print(f"❌ MQTT setup error: {e}")
            return False

    def setup_serial(self):
        """Initialize serial connection to gateway node"""
        try:
            self.serial_conn = serial.Serial(
                port=self.serial_port,
                baudrate=self.baudrate,
                timeout=1
            )
            print(f"✅ Connected to gateway: {self.serial_port} @ {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"❌ Serial setup error: {e}")
            return False

    def parse_sensor_packet(self, line):
        """
        Parse EnhancedSensorData packet from serial output

        Expected format (from gateway logs):
        RX: Seq=1 From=BB94
          PM: 1.0=12 2.5=15 10=18 µg/m³ (AQI: Good)
          GPS: 13.727800°N, 100.775300°E, alt=25.5m, 7 sats (Excellent)
        """
        packet_to_publish = None

        try:
            # New packet header
            if line.startswith("RX:"):
                seq_match = re.search(r"Seq=(\d+)", line)
                src_match = re.search(r"From=([A-Fa-f0-9]+)", line)

                # Publish previous packet if we have one ready
                if self.current_packet and (
                    self.current_packet.get('pm1_0') is not None or
                    self.current_packet.get('latitude') is not None
                ):
                    packet_to_publish = self.current_packet

                if seq_match and src_match:
                    self.current_packet = {
                        'sequence': int(seq_match.group(1)),
                        'src': src_match.group(1),
                        'timestamp': datetime.now().isoformat()
                    }
                else:
                    self.current_packet = None

            # PM line
            elif line.startswith("PM:") and self.current_packet:
                pm_match = re.search(
                    r"PM:\s*1\.0=([0-9.]+)\s*2\.5=([0-9.]+)\s*10=([0-9.]+)",
                    line
                )
                if pm_match:
                    self.current_packet['pm1_0'] = float(pm_match.group(1))
                    self.current_packet['pm2_5'] = float(pm_match.group(2))
                    self.current_packet['pm10'] = float(pm_match.group(3))

            # GPS line
            elif line.startswith("GPS:") and self.current_packet:
                gps_match = re.search(
                    r"GPS:\s*([0-9.+-]+)[^0-9NS]*([NS]),\s*([0-9.+-]+)[^0-9EW]*([EW]),\s*alt=([0-9.+-]+)m,\s*(\d+)\s*sats",
                    line
                )
                if gps_match:
                    lat_val = float(gps_match.group(1))
                    lat_dir = gps_match.group(2)
                    lon_val = float(gps_match.group(3))
                    lon_dir = gps_match.group(4)
                    alt_val = float(gps_match.group(5))
                    sats_val = int(gps_match.group(6))

                    latitude = lat_val if lat_dir == 'N' else -lat_val
                    longitude = lon_val if lon_dir == 'E' else -lon_val

                    self.current_packet['latitude'] = latitude
                    self.current_packet['longitude'] = longitude
                    self.current_packet['altitude'] = alt_val
                    self.current_packet['satellites'] = sats_val
                    self.current_packet['gps_valid'] = True

                    packet_to_publish = self.current_packet
                    self.current_packet = None

            return packet_to_publish

        except Exception as e:
            return None

    def publish_sensor_data(self, packet_data):
        """Publish parsed sensor data to MQTT topics"""
        if not self.mqtt_client or not packet_data:
            return False

        try:
            # Construct topic with node ID
            topic = self.mqtt_config['topics']['sensor_data'].format(
                node_id=packet_data.get('src', 'unknown')
            )

            # Create JSON payload
            payload = json.dumps({
                'timestamp': packet_data.get('timestamp'),
                'sequence': packet_data.get('sequence'),
                'source': packet_data.get('src'),
                'pm1_0': packet_data.get('pm1_0', 0),
                'pm2_5': packet_data.get('pm2_5', 0),
                'pm10': packet_data.get('pm10', 0),
                'latitude': packet_data.get('latitude', 0.0),
                'longitude': packet_data.get('longitude', 0.0),
                'altitude': packet_data.get('altitude', 0.0),
                'satellites': packet_data.get('satellites', 0),
                'gps_valid': packet_data.get('gps_valid', False)
            })

            # Publish with QoS=1 (at least once delivery)
            result = self.mqtt_client.publish(
                topic,
                payload,
                qos=self.mqtt_config.get('qos', 1)
            )

            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"📤 Published to {topic}: Seq={packet_data.get('sequence')}")
                return True
            else:
                self.stats['mqtt_errors'] += 1
                return False

        except Exception as e:
            print(f"❌ Publish error: {e}")
            self.stats['mqtt_errors'] += 1
            return False

    def run(self):
        """Main data collection and publishing loop"""
        print("\n╔═══════════════════════════════════════════════════════════╗")
        print("║      xMESH MQTT Publisher for AIT Hazemon                ║")
        print("╚═══════════════════════════════════════════════════════════╝\n")

        # Setup connections
        if not self.setup_serial():
            return

        if not self.setup_mqtt():
            print("⚠️  Running without MQTT (serial logging only)")

        self.running = True
        self.stats['start_time'] = datetime.now()

        print("\n📡 Listening for sensor data from gateway...\n")
        print("Press Ctrl+C to stop\n")

        try:
            while self.running:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()

                    # Print to console for monitoring
                    print(line)

                    # Parse and publish sensor packets
                    packet_data = self.parse_sensor_packet(line)
                    if packet_data:
                        self.stats['packets_received'] += 1
                        if self.mqtt_client:
                            self.publish_sensor_data(packet_data)

                    # Print statistics every 60 seconds
                    if self.stats['packets_received'] > 0 and self.stats['packets_received'] % 10 == 0:
                        self.print_stats()

        except KeyboardInterrupt:
            print("\n\n⏹️  Stopping MQTT publisher...")
            self.shutdown()

    def print_stats(self):
        """Print current statistics"""
        elapsed = (datetime.now() - self.stats['start_time']).total_seconds()
        print(f"\n📊 Stats: {self.stats['packets_received']} RX, " +
              f"{self.stats['packets_published']} published, " +
              f"{self.stats['mqtt_errors']} errors, " +
              f"{int(elapsed)}s uptime\n")

    def shutdown(self):
        """Clean shutdown"""
        self.running = False

        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            print("✅ MQTT disconnected")

        if self.serial_conn:
            self.serial_conn.close()
            print("✅ Serial closed")

        self.print_stats()
        print("\n✅ Shutdown complete\n")

def main():
    parser = argparse.ArgumentParser(description='xMESH MQTT Publisher for AIT Hazemon')
    parser.add_argument('--port', required=True, help='Serial port (e.g., /dev/ttyUSB0)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Serial baud rate')
    parser.add_argument('--config', default='mqtt_config.json', help='MQTT config file')

    args = parser.parse_args()

    # Create publisher
    publisher = xMESH_MQTT_Publisher(args.port, args.baudrate, args.config)

    # Setup signal handler for graceful shutdown
    def signal_handler(sig, frame):
        print("\n\n⏹️  Received interrupt signal...")
        publisher.shutdown()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    # Run
    publisher.run()

if __name__ == '__main__':
    main()
