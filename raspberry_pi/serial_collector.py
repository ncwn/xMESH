#!/usr/bin/env python3
"""
Serial Data Collector for xMESH LoRaMesher Experiments
Collects CSV data from gateway node via USB serial connection
"""

import serial
import csv
import sqlite3
import argparse
import sys
import time
import signal
import threading
from datetime import datetime
from pathlib import Path
import paho.mqtt.client as mqtt
import json

class SerialCollector:
    def __init__(self, port, baudrate, output_file, database_file=None, mqtt_config=None):
        self.port = port
        self.baudrate = baudrate
        self.output_file = output_file
        self.database_file = database_file
        self.mqtt_config = mqtt_config

        self.serial_conn = None
        self.csv_writer = None
        self.csv_file = None
        self.db_conn = None
        self.mqtt_client = None

        self.running = False
        self.packet_count = 0
        self.start_time = None
        self.last_packet_time = None

        # Statistics
        self.stats = {
            'packets_received': 0,
            'packets_logged': 0,
            'errors': 0,
            'uptime': 0
        }

    def setup(self):
        """Initialize all connections"""
        try:
            # Setup serial connection
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            print(f"Connected to {self.port} at {self.baudrate} baud")

            # Setup CSV file
            self.csv_file = open(self.output_file, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)

            # Setup SQLite database if specified
            if self.database_file:
                self.setup_database()

            # Setup MQTT if configured
            if self.mqtt_config:
                self.setup_mqtt()

            self.start_time = datetime.now()
            return True

        except serial.SerialException as e:
            print(f"Error opening serial port: {e}")
            return False
        except Exception as e:
            print(f"Setup error: {e}")
            return False

    def setup_database(self):
        """Create SQLite database and tables"""
        self.db_conn = sqlite3.connect(self.database_file)
        cursor = self.db_conn.cursor()

        # Create experiments table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS experiments (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                start_time TIMESTAMP,
                end_time TIMESTAMP,
                protocol TEXT,
                topology TEXT,
                notes TEXT
            )
        ''')

        # Create packets table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS packets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                experiment_id INTEGER,
                timestamp INTEGER,
                node_id INTEGER,
                event_type TEXT,
                src INTEGER,
                dest INTEGER,
                rssi REAL,
                snr REAL,
                etx REAL,
                hop_count INTEGER,
                packet_size INTEGER,
                sequence INTEGER,
                cost REAL,
                next_hop INTEGER,
                gateway INTEGER,
                FOREIGN KEY (experiment_id) REFERENCES experiments(id)
            )
        ''')

        # Create metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                experiment_id INTEGER,
                timestamp TIMESTAMP,
                metric_name TEXT,
                metric_value REAL,
                node_id INTEGER,
                FOREIGN KEY (experiment_id) REFERENCES experiments(id)
            )
        ''')

        self.db_conn.commit()
        print(f"Database initialized: {self.database_file}")

    def setup_mqtt(self):
        """Setup MQTT connection for real-time monitoring"""
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_disconnect = self.on_mqtt_disconnect

        if 'username' in self.mqtt_config and 'password' in self.mqtt_config:
            self.mqtt_client.username_pw_set(
                self.mqtt_config['username'],
                self.mqtt_config['password']
            )

        try:
            self.mqtt_client.connect(
                self.mqtt_config['broker'],
                self.mqtt_config.get('port', 1883),
                60
            )
            self.mqtt_client.loop_start()
            print(f"MQTT connected to {self.mqtt_config['broker']}")
        except Exception as e:
            print(f"MQTT connection failed: {e}")
            self.mqtt_client = None

    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("MQTT connected successfully")
        else:
            print(f"MQTT connection failed with code {rc}")

    def on_mqtt_disconnect(self, client, userdata, rc):
        print("MQTT disconnected")

    def run(self):
        """Main collection loop"""
        self.running = True
        print("Starting data collection...")
        print("Press Ctrl+C to stop")

        # Write CSV header if needed
        header_written = False

        while self.running:
            try:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8').strip()

                    if line:
                        self.stats['packets_received'] += 1
                        self.last_packet_time = datetime.now()

                        # Process the line
                        if line.startswith('timestamp,'):
                            # This is the CSV header
                            if not header_written:
                                self.csv_writer.writerow(line.split(','))
                                header_written = True
                                print("CSV header detected and written")
                        elif ',' in line:
                            # This is CSV data
                            self.process_csv_line(line)
                        else:
                            # This might be debug output, log it
                            print(f"Debug: {line}")

                # Update statistics periodically
                if self.packet_count % 100 == 0 and self.packet_count > 0:
                    self.print_stats()

            except KeyboardInterrupt:
                print("\nStopping data collection...")
                self.running = False
            except serial.SerialException as e:
                print(f"Serial error: {e}")
                self.stats['errors'] += 1
                time.sleep(1)
            except Exception as e:
                print(f"Unexpected error: {e}")
                self.stats['errors'] += 1

    def process_csv_line(self, line):
        """Process a CSV data line"""
        try:
            fields = line.split(',')

            if len(fields) < 14:  # Expected number of fields
                print(f"Incomplete line: {line}")
                return

            # Parse fields
            data = {
                'timestamp': int(fields[0]),
                'node_id': int(fields[1]),
                'event_type': fields[2],
                'src': int(fields[3]),
                'dest': int(fields[4]),
                'rssi': float(fields[5]),
                'snr': float(fields[6]),
                'etx': float(fields[7]),
                'hop_count': int(fields[8]),
                'packet_size': int(fields[9]),
                'sequence': int(fields[10]),
                'cost': float(fields[11]),
                'next_hop': int(fields[12]),
                'gateway': int(fields[13])
            }

            # Write to CSV
            self.csv_writer.writerow(fields)
            self.csv_file.flush()

            # Write to database
            if self.db_conn:
                self.write_to_database(data)

            # Publish to MQTT
            if self.mqtt_client:
                self.publish_to_mqtt(data)

            self.packet_count += 1
            self.stats['packets_logged'] += 1

            # Print important events
            if data['event_type'] in ['TX', 'RX', 'DROP', 'TIMEOUT']:
                print(f"[{data['timestamp']}] Node {data['node_id']}: {data['event_type']} "
                      f"Src:0x{data['src']:04X} Dst:0x{data['dest']:04X} "
                      f"RSSI:{data['rssi']:.1f} SNR:{data['snr']:.1f}")

        except (ValueError, IndexError) as e:
            print(f"Error parsing line: {e}")
            print(f"Line: {line}")
            self.stats['errors'] += 1

    def write_to_database(self, data):
        """Write packet data to SQLite database"""
        try:
            cursor = self.db_conn.cursor()
            cursor.execute('''
                INSERT INTO packets (
                    timestamp, node_id, event_type, src, dest,
                    rssi, snr, etx, hop_count, packet_size,
                    sequence, cost, next_hop, gateway
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                data['timestamp'], data['node_id'], data['event_type'],
                data['src'], data['dest'], data['rssi'], data['snr'],
                data['etx'], data['hop_count'], data['packet_size'],
                data['sequence'], data['cost'], data['next_hop'], data['gateway']
            ))

            if self.packet_count % 10 == 0:  # Commit every 10 packets
                self.db_conn.commit()

        except sqlite3.Error as e:
            print(f"Database error: {e}")
            self.stats['errors'] += 1

    def publish_to_mqtt(self, data):
        """Publish packet data to MQTT for real-time monitoring"""
        try:
            topic = f"xmesh/node/{data['node_id']}/packet"
            payload = json.dumps(data)
            self.mqtt_client.publish(topic, payload, qos=1)

            # Also publish aggregated stats
            if self.packet_count % 10 == 0:
                stats_topic = "xmesh/stats"
                self.mqtt_client.publish(stats_topic, json.dumps(self.stats), qos=1)

        except Exception as e:
            print(f"MQTT publish error: {e}")

    def print_stats(self):
        """Print collection statistics"""
        if self.start_time:
            uptime = datetime.now() - self.start_time
            self.stats['uptime'] = str(uptime).split('.')[0]

        print("\n=== Collection Statistics ===")
        print(f"Uptime: {self.stats['uptime']}")
        print(f"Packets Received: {self.stats['packets_received']}")
        print(f"Packets Logged: {self.stats['packets_logged']}")
        print(f"Errors: {self.stats['errors']}")

        if self.last_packet_time:
            time_since_last = datetime.now() - self.last_packet_time
            print(f"Last Packet: {time_since_last.total_seconds():.1f}s ago")
        print("============================\n")

    def cleanup(self):
        """Clean up connections"""
        print("\nCleaning up...")

        if self.csv_file:
            self.csv_file.close()
            print(f"CSV file saved: {self.output_file}")

        if self.db_conn:
            self.db_conn.commit()
            self.db_conn.close()
            print(f"Database saved: {self.database_file}")

        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()

        if self.serial_conn:
            self.serial_conn.close()

        self.print_stats()
        print("Data collection complete")

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    print("\nReceived interrupt signal")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='Serial Data Collector for xMESH')
    parser.add_argument('-p', '--port', default='/dev/ttyUSB0',
                       help='Serial port (default: /dev/ttyUSB0)')
    parser.add_argument('-b', '--baud', type=int, default=115200,
                       help='Baud rate (default: 115200)')
    parser.add_argument('-o', '--output', required=True,
                       help='Output CSV file path')
    parser.add_argument('-d', '--database',
                       help='SQLite database file (optional)')
    parser.add_argument('-m', '--mqtt-broker',
                       help='MQTT broker address for real-time monitoring')
    parser.add_argument('--mqtt-port', type=int, default=1883,
                       help='MQTT port (default: 1883)')
    parser.add_argument('--mqtt-user',
                       help='MQTT username')
    parser.add_argument('--mqtt-pass',
                       help='MQTT password')
    parser.add_argument('--protocol', default='unknown',
                       help='Protocol being tested (flooding/hopcount/gateway)')
    parser.add_argument('--topology', default='unknown',
                       help='Network topology (linear/diamond/grid)')
    parser.add_argument('--repetition', type=int, default=1,
                       help='Experiment repetition number')

    args = parser.parse_args()

    # Prepare MQTT config if specified
    mqtt_config = None
    if args.mqtt_broker:
        mqtt_config = {
            'broker': args.mqtt_broker,
            'port': args.mqtt_port
        }
        if args.mqtt_user:
            mqtt_config['username'] = args.mqtt_user
        if args.mqtt_pass:
            mqtt_config['password'] = args.mqtt_pass

    # Create output directory if it doesn't exist
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Create collector instance
    collector = SerialCollector(
        port=args.port,
        baudrate=args.baud,
        output_file=args.output,
        database_file=args.database,
        mqtt_config=mqtt_config
    )

    # Setup signal handler
    signal.signal(signal.SIGINT, signal_handler)

    # Start collection
    if collector.setup():
        print(f"Protocol: {args.protocol}")
        print(f"Topology: {args.topology}")
        print(f"Repetition: {args.repetition}")
        print("-" * 40)

        try:
            collector.run()
        except KeyboardInterrupt:
            pass
        finally:
            collector.cleanup()
    else:
        print("Failed to setup collector")
        sys.exit(1)

if __name__ == "__main__":
    main()