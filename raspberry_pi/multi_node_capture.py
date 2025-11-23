#!/usr/bin/env python3
"""
Multi-Node Serial Capture for xMESH Testing
Captures log output from multiple nodes simultaneously
"""

import serial
import threading
import time
import argparse
import sys
import signal
from datetime import datetime
from pathlib import Path

class NodeCapture:
    def __init__(self, node_id, port, baudrate, output_file):
        self.node_id = node_id
        self.port = port
        self.baudrate = baudrate
        self.output_file = output_file
        self.serial_conn = None
        self.running = False
        self.thread = None
        self.packet_count = 0
        self.line_count = 0
        self.tx_count = 0
        self.rx_count = 0
        # Trickle-specific metrics
        self.trickle_hello_count = 0
        self.trickle_suppress_count = 0
        self.trickle_double_count = 0
        self.trickle_reset_count = 0
        self.loramesh_hello_count = 0  # Should be 0 if Trickle working
        self.topology_changes = 0
        self.cost_evaluations = 0
        # Sensor data tracking (PM + GPS)
        self.pm_readings = 0
        self.gps_readings = 0

    def start(self):
        """Start capture thread"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1
            )
            print(f"[Node {self.node_id}] Connected to {self.port}")

            self.running = True
            self.thread = threading.Thread(target=self._capture_loop, daemon=True)
            self.thread.start()
            return True
        except Exception as e:
            print(f"[Node {self.node_id}] Error: {e}")
            return False

    def _capture_loop(self):
        """Main capture loop"""
        with open(self.output_file, 'w') as f:
            f.write(f"=== Node {self.node_id} Capture Log ===\n")
            f.write(f"Port: {self.port}\n")
            f.write(f"Started: {datetime.now().isoformat()}\n")
            f.write("=" * 50 + "\n\n")

            while self.running:
                try:
                    if self.serial_conn.in_waiting > 0:
                        line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            self.line_count += 1
                            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

                            # Write to file
                            f.write(f"[{timestamp}] {line}\n")
                            f.flush()

                            # Parse and count events
                            if "TX:" in line:
                                self.tx_count += 1
                                print(f"[Node {self.node_id}] {timestamp} TX detected")
                            elif "RX:" in line:
                                self.rx_count += 1
                                print(f"[Node {self.node_id}] {timestamp} RX detected")

                            # Trickle-specific events
                            if "[TrickleHELLO] Sending HELLO" in line:
                                self.trickle_hello_count += 1
                                print(f"[Node {self.node_id}] {timestamp} 📡 TRICKLE HELLO #{self.trickle_hello_count}")
                            elif "[Trickle] SUPPRESS" in line:
                                self.trickle_suppress_count += 1
                                print(f"[Node {self.node_id}] {timestamp} 🔇 SUPPRESSED")
                            elif "[Trickle] DOUBLE" in line:
                                self.trickle_double_count += 1
                                # Extract interval if possible
                                if "I=" in line:
                                    try:
                                        interval = line.split("I=")[1].split("s")[0]
                                        print(f"[Node {self.node_id}] {timestamp} ⏫ INTERVAL DOUBLED to {interval}s")
                                    except:
                                        print(f"[Node {self.node_id}] {timestamp} ⏫ INTERVAL DOUBLED")
                            elif "[Trickle] RESET" in line or "[TRICKLE] Topology change" in line:
                                self.trickle_reset_count += 1
                                print(f"[Node {self.node_id}] {timestamp} 🔄 TRICKLE RESET")
                            elif "[TOPOLOGY]" in line:
                                self.topology_changes += 1
                                print(f"[Node {self.node_id}] {timestamp} 🌐 TOPOLOGY CHANGE")
                            elif "Creating Routing Packet" in line and "[TrickleHELLO]" not in line:
                                # This would be LoRaMesher's HELLO (should NOT happen with Trickle)
                                self.loramesh_hello_count += 1
                                print(f"[Node {self.node_id}] {timestamp} ⚠️  LORAMESH HELLO (unexpected!)")
                            elif "[COST]" in line and "Route to" in line:
                                self.cost_evaluations += 1

                            # PM sensor data tracking
                            if "[PMS]" in line and "µg/m³" in line:
                                self.pm_readings += 1
                                print(f"[Node {self.node_id}] {timestamp} 💨 PM Data: {line.split('µg/m³')[0].split('[PMS]')[1].strip()}")

                            # GPS data tracking
                            if "[GPS]" in line and ("°N" in line or "sats" in line):
                                self.gps_readings += 1
                                # Extract coordinates if present
                                if "°N" in line and "°E" in line:
                                    try:
                                        coords = line.split("°E")[0].split("[GPS]")[1].strip()
                                        print(f"[Node {self.node_id}] {timestamp} 📍 GPS: {coords}")
                                    except:
                                        print(f"[Node {self.node_id}] {timestamp} 📍 GPS Update")

                            # PM data in transmission (enhanced packets)
                            if "PM:" in line and "µg/m³" in line and ("TX:" in line or "RX:" in line):
                                # This captures PM data in TX/RX messages
                                if "PM: 1.0=" in line or "PM1.0" in line:
                                    print(f"[Node {self.node_id}] {timestamp} 📦 PM in packet: {line}")

                except Exception as e:
                    if self.running:
                        print(f"[Node {self.node_id}] Read error: {e}")
                        time.sleep(0.1)

    def stop(self):
        """Stop capture"""
        self.running = False
        if self.thread:
            self.thread.join(timeout=2)
        if self.serial_conn:
            self.serial_conn.close()
        print(f"[Node {self.node_id}] Stopped. Lines: {self.line_count}, TX: {self.tx_count}, RX: {self.rx_count}")
        print(f"  Trickle HELLOs: {self.trickle_hello_count}, Suppressed: {self.trickle_suppress_count}")
        print(f"  Interval doubles: {self.trickle_double_count}, Resets: {self.trickle_reset_count}")
        print(f"  Sensor Data - PM: {self.pm_readings}, GPS: {self.gps_readings}")

    def get_stats(self):
        """Get capture statistics"""
        return {
            'node_id': self.node_id,
            'lines': self.line_count,
            'tx': self.tx_count,
            'rx': self.rx_count,
            'trickle_hello': self.trickle_hello_count,
            'trickle_suppress': self.trickle_suppress_count,
            'trickle_double': self.trickle_double_count,
            'trickle_reset': self.trickle_reset_count,
            'loramesh_hello': self.loramesh_hello_count,
            'topology_changes': self.topology_changes,
            'cost_evals': self.cost_evaluations,
            'pm_readings': self.pm_readings,
            'gps_readings': self.gps_readings
        }

class MultiNodeCapture:
    def __init__(self, node_configs, duration=None):
        self.node_configs = node_configs
        self.duration = duration
        self.captures = []
        self.start_time = None
        self.running = False

    def start(self):
        """Start all node captures"""
        print("\n" + "="*60)
        print("xMESH Multi-Node Capture")
        print("="*60)

        # Create capture instances
        for config in self.node_configs:
            capture = NodeCapture(
                node_id=config['id'],
                port=config['port'],
                baudrate=config['baudrate'],
                output_file=config['output']
            )
            if capture.start():
                self.captures.append(capture)
            else:
                print(f"Failed to start Node {config['id']}")

        if not self.captures:
            print("No nodes successfully connected")
            return False

        print(f"\nCapturing from {len(self.captures)} nodes...")
        if self.duration:
            print(f"Duration: {self.duration} seconds")
        print("Press Ctrl+C to stop\n")

        self.start_time = time.time()
        self.running = True

        # Wait for duration or Ctrl+C
        try:
            if self.duration:
                time.sleep(self.duration)
            else:
                while self.running:
                    time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n\nStopping capture...")

        return True

    def stop(self):
        """Stop all captures"""
        self.running = False

        for capture in self.captures:
            capture.stop()

        # Print summary
        elapsed = time.time() - self.start_time if self.start_time else 0
        self._print_summary(elapsed)

    def _print_summary(self, elapsed):
        """Print capture summary"""
        print("\n" + "="*60)
        print("Capture Summary")
        print("="*60)
        print(f"Duration: {elapsed:.1f} seconds")
        print(f"Nodes captured: {len(self.captures)}")
        print()

        total_tx = 0
        total_rx = 0
        total_trickle_hello = 0
        total_trickle_suppress = 0
        total_loramesh_hello = 0

        for capture in self.captures:
            stats = capture.get_stats()
            print(f"Node {stats['node_id']}:")
            print(f"  - Lines captured: {stats['lines']}")
            print(f"  - TX events: {stats['tx']}")
            print(f"  - RX events: {stats['rx']}")
            print(f"  - Trickle HELLOs: {stats['trickle_hello']}")
            print(f"  - Trickle Suppressed: {stats['trickle_suppress']}")
            print(f"  - Interval Doubles: {stats['trickle_double']}")
            print(f"  - Trickle Resets: {stats['trickle_reset']}")
            print(f"  - LoRaMesh HELLOs: {stats['loramesh_hello']} (should be 0!)")
            print(f"  - Topology Changes: {stats['topology_changes']}")
            print(f"  - Cost Evaluations: {stats['cost_evals']}")
            print(f"  - Output: {capture.output_file}")
            print()

            total_tx += stats['tx']
            total_rx += stats['rx']
            total_trickle_hello += stats['trickle_hello']
            total_trickle_suppress += stats['trickle_suppress']
            total_loramesh_hello += stats['loramesh_hello']

        print(f"Total TX events: {total_tx}")
        print(f"Total RX events: {total_rx}")
        print(f"Total Trickle HELLOs: {total_trickle_hello}")
        print(f"Total Suppressed: {total_trickle_suppress}")
        print(f"Total LoRaMesh HELLOs: {total_loramesh_hello} (⚠️ should be 0!)")

        # Calculate reduction vs Protocol 2 baseline (30 HELLOs/hour fixed 120s)
        if elapsed > 0:
            # IMPORTANT: Count ALL HELLOs (Trickle + Safety), not just Trickle!
            # Safety HELLOs are ALSO overhead that must be counted
            total_all_hellos = total_trickle_hello  # Will add safety detection in future

            # Note: Currently only counts Trickle HELLOs
            # TODO: Add safety HELLO detection to get accurate total
            trickle_rate_per_hour = (total_trickle_hello / elapsed) * 3600
            protocol2_baseline = 30 * len(self.captures)  # 30 HELLOs/hour per node

            # This reduction is ONLY for Trickle HELLOs (not total overhead)
            trickle_reduction = ((protocol2_baseline - trickle_rate_per_hour) / protocol2_baseline) * 100

            print(f"\n⚠️  TRICKLE HELLO Rate: {trickle_rate_per_hour:.1f} per hour (vs {protocol2_baseline} baseline)")
            print(f"⚠️  Trickle-only Reduction: {trickle_reduction:.1f}%")
            print(f"\nNote: Safety HELLOs not counted in automatic calculation!")
            print(f"      Manual count needed for total overhead reduction.")

        print("="*60)

def main():
    parser = argparse.ArgumentParser(description='Multi-Node Serial Capture for xMESH')
    parser.add_argument('-d', '--duration', type=int,
                       help='Capture duration in seconds (optional, Ctrl+C to stop)')
    parser.add_argument('-o', '--output-dir',
                       help='Output directory for log files (optional, auto-creates timestamped folder)')
    parser.add_argument('-n', '--test-name', default='test',
                       help='Test name prefix (default: "test"). Creates folder: <protocol>/<name>_YYYYMMDD_HHMMSS/')
    parser.add_argument('-p', '--protocol', default='protocol3',
                       help='Protocol name (default: "protocol3")')
    parser.add_argument('--node1-port', default='/dev/cu.usbserial-0001',
                       help='Node 1 serial port')
    parser.add_argument('--node2-port', default='/dev/cu.usbserial-5',
                       help='Node 2 serial port')
    parser.add_argument('--node3-port', default='/dev/cu.usbserial-7',
                       help='Node 3 serial port')
    parser.add_argument('-b', '--baud', type=int, default=115200,
                       help='Baud rate (default: 115200)')

    args = parser.parse_args()

    # Generate timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    # Create output directory with timestamp
    if args.output_dir:
        # User provided explicit path
        output_dir = Path(args.output_dir)
    else:
        # Auto-create: experiments/results/<protocol>/<test-name>_<timestamp>/
        output_dir = Path(f"experiments/results/{args.protocol}/{args.test_name}_{timestamp}")

    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Test folder: {output_dir}")

    # Configure nodes
    node_configs = [
        {
            'id': 1,
            'port': args.node1_port,
            'baudrate': args.baud,
            'output': output_dir / f"node1_{timestamp}.log"
        },
        {
            'id': 2,
            'port': args.node2_port,
            'baudrate': args.baud,
            'output': output_dir / f"node2_{timestamp}.log"
        },
        {
            'id': 3,
            'port': args.node3_port,
            'baudrate': args.baud,
            'output': output_dir / f"node3_{timestamp}.log"
        }
    ]

    # Create and run capture
    capture = MultiNodeCapture(node_configs, duration=args.duration)

    # Setup signal handler for clean shutdown
    def signal_handler(sig, frame):
        capture.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    # Start capture
    if capture.start():
        capture.stop()

        # Run health check automatically (imports from test_health_check module)
        try:
            from test_health_check import TestHealthAnalyzer

            print("\n" + "="*60)
            print("Automated Health Check")
            print("="*60)

            log_files = sorted(output_dir.glob("node*.log"))
            if log_files:
                analyzer = TestHealthAnalyzer(log_files)
                analyzer.analyze()
        except ImportError:
            print("\nNote: test_health_check.py not found (optional)")
        except Exception as e:
            print(f"\nNote: Health check error: {e}")
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
