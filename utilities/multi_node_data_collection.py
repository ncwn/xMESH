#!/usr/bin/env python3
"""
xMESH Multi-Node Data Collection Tool
Week 1: Hardware Testing with Monitoring

Captures serial output from ALL 3 nodes simultaneously:
- Sensor (BB94)
- Router (6674)  
- Gateway (D218)

Extracts monitoring data:
- Channel occupancy (duty-cycle %)
- Memory usage (current/min/max heap)
- Queue statistics (enqueued/dropped/depth)
- TX/RX packet counts
- Protocol-specific metrics (routing table, gateway load, etc.)

Outputs:
- 3 CSV files (one per node)
- Real-time console monitoring
- Test run statistics summary
"""

import serial
import argparse
import csv
import sys
import re
import threading
import time
from datetime import datetime
from pathlib import Path


class NodeMonitor:
    """Monitors a single node's serial output"""
    
    def __init__(self, node_id, port, role, output_file):
        self.node_id = node_id
        self.port = port
        self.role = role
        self.output_file = output_file
        self.serial = None
        self.csv_writer = None
        self.csv_file = None
        self.running = False
        self.thread = None
        
        # Statistics
        self.stats = {
            'tx_count': 0,
            'rx_count': 0,
            'monitoring_updates': 0,
            'duty_cycle_max': 0.0,
            'memory_min_kb': 9999,
            'queue_drops': 0,
            'start_time': None
        }
        
        # Regex patterns for monitoring stats
        self.patterns = {
            'monitoring_header': re.compile(r'==== Network Monitoring Stats ===='),
            'channel': re.compile(r'Channel: ([\d.]+)% duty-cycle, (\d+) TX, (\d+) violations'),
            'memory': re.compile(r'Memory: (\d+)/(\d+) KB free, Min: (\d+) KB, Peak: (\d+) KB'),
            'queue': re.compile(r'Queue: (\d+) enqueued, (\d+) dropped \(([\d.]+)%\), max depth: (\d+)'),
            'tx_packet': re.compile(r'TX: Seq=(\d+)'),
            'rx_packet': re.compile(r'RX: Seq=(\d+) From=([A-F0-9]+)'),
            'routing_table_size': re.compile(r'Routing table: (\d+) entries'),
        }
    
    def connect(self):
        """Connect to serial port"""
        try:
            self.serial = serial.Serial(self.port, 115200, timeout=1)
            time.sleep(2)  # Wait for serial to stabilize
            
            # Open CSV file for writing
            self.csv_file = open(self.output_file, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)
            
            # Write CSV header
            self.csv_writer.writerow([
                'timestamp',
                'node_id',
                'role',
                'duty_cycle_pct',
                'tx_count',
                'violations',
                'memory_free_kb',
                'memory_total_kb',
                'memory_min_kb',
                'memory_peak_kb',
                'queue_enqueued',
                'queue_dropped',
                'queue_drop_rate_pct',
                'queue_max_depth',
                'routing_table_entries',
                'raw_line'
            ])
            
            return True
        except Exception as e:
            print(f"❌ Error connecting to {self.node_id}: {e}")
            return False
    
    def start(self):
        """Start monitoring thread"""
        self.running = True
        self.stats['start_time'] = datetime.now()
        self.thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.thread.start()
    
    def stop(self):
        """Stop monitoring thread"""
        self.running = False
        if self.thread:
            self.thread.join(timeout=2)
        if self.csv_file:
            self.csv_file.close()
        if self.serial:
            self.serial.close()
    
    def _monitor_loop(self):
        """Main monitoring loop (runs in thread)"""
        current_data = {}
        
        while self.running:
            try:
                if self.serial.in_waiting:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    
                    if not line:
                        continue
                    
                    # Check for monitoring stats header
                    if self.patterns['monitoring_header'].search(line):
                        # Start of new monitoring block, reset current data
                        current_data = {
                            'timestamp': datetime.now().isoformat(),
                            'node_id': self.node_id,
                            'role': self.role
                        }
                    
                    # Parse channel occupancy
                    match = self.patterns['channel'].search(line)
                    if match:
                        duty_cycle = float(match.group(1))
                        tx_count = int(match.group(2))
                        violations = int(match.group(3))
                        
                        current_data['duty_cycle_pct'] = duty_cycle
                        current_data['tx_count'] = tx_count
                        current_data['violations'] = violations
                        
                        # Update stats
                        if duty_cycle > self.stats['duty_cycle_max']:
                            self.stats['duty_cycle_max'] = duty_cycle
                    
                    # Parse memory
                    match = self.patterns['memory'].search(line)
                    if match:
                        mem_free = int(match.group(1))
                        mem_total = int(match.group(2))
                        mem_min = int(match.group(3))
                        mem_peak = int(match.group(4))
                        
                        current_data['memory_free_kb'] = mem_free
                        current_data['memory_total_kb'] = mem_total
                        current_data['memory_min_kb'] = mem_min
                        current_data['memory_peak_kb'] = mem_peak
                        
                        # Update stats
                        if mem_min < self.stats['memory_min_kb']:
                            self.stats['memory_min_kb'] = mem_min
                    
                    # Parse queue
                    match = self.patterns['queue'].search(line)
                    if match:
                        enqueued = int(match.group(1))
                        dropped = int(match.group(2))
                        drop_rate = float(match.group(3))
                        max_depth = int(match.group(4))
                        
                        current_data['queue_enqueued'] = enqueued
                        current_data['queue_dropped'] = dropped
                        current_data['queue_drop_rate_pct'] = drop_rate
                        current_data['queue_max_depth'] = max_depth
                        
                        # Update stats
                        self.stats['queue_drops'] = dropped
                    
                    # Parse routing table size
                    match = self.patterns['routing_table_size'].search(line)
                    if match:
                        rt_size = int(match.group(1))
                        current_data['routing_table_entries'] = rt_size
                    
                    # Check if we have complete monitoring data to write
                    if 'duty_cycle_pct' in current_data and 'memory_free_kb' in current_data and 'queue_enqueued' in current_data:
                        # Write to CSV
                        self.csv_writer.writerow([
                            current_data.get('timestamp', ''),
                            current_data.get('node_id', ''),
                            current_data.get('role', ''),
                            current_data.get('duty_cycle_pct', ''),
                            current_data.get('tx_count', ''),
                            current_data.get('violations', ''),
                            current_data.get('memory_free_kb', ''),
                            current_data.get('memory_total_kb', ''),
                            current_data.get('memory_min_kb', ''),
                            current_data.get('memory_peak_kb', ''),
                            current_data.get('queue_enqueued', ''),
                            current_data.get('queue_dropped', ''),
                            current_data.get('queue_drop_rate_pct', ''),
                            current_data.get('queue_max_depth', ''),
                            current_data.get('routing_table_entries', ''),
                            line  # Raw line for debugging
                        ])
                        self.csv_file.flush()
                        
                        self.stats['monitoring_updates'] += 1
                        
                        # Clear current data for next block
                        current_data = {}
                    
                    # Count TX/RX packets
                    if self.patterns['tx_packet'].search(line):
                        self.stats['tx_count'] += 1
                    if self.patterns['rx_packet'].search(line):
                        self.stats['rx_count'] += 1
                
            except Exception as e:
                print(f"⚠️  Error reading from {self.node_id}: {e}")
                time.sleep(0.1)


class MultiNodeCollector:
    """Manages data collection from multiple nodes"""
    
    def __init__(self, protocol, run_number, duration_min, output_dir):
        self.protocol = protocol
        self.run_number = run_number
        self.duration_min = duration_min
        self.output_dir = Path(output_dir)
        self.monitors = []
        
        # Create output directory
        protocol_dir = self.output_dir / protocol
        protocol_dir.mkdir(parents=True, exist_ok=True)
    
    def add_node(self, node_id, port, role):
        """Add a node to monitor"""
        output_file = self.output_dir / self.protocol / f"run{self.run_number}_{node_id}_{role.lower()}.csv"
        monitor = NodeMonitor(node_id, port, role, output_file)
        self.monitors.append(monitor)
        return monitor
    
    def start_collection(self):
        """Start data collection from all nodes"""
        print("\n=== LoRa Mesh Network Data Collection ===")
        print(f"Protocol: {self.protocol}")
        print(f"Run: {self.run_number}")
        print(f"Duration: {self.duration_min} minutes\n")
        
        # Connect all nodes
        print("Connecting to nodes...")
        for monitor in self.monitors:
            if monitor.connect():
                print(f"✓ {monitor.role} ({monitor.node_id}): {monitor.port}")
            else:
                print(f"✗ {monitor.role} ({monitor.node_id}): Failed to connect")
                return False
        
        # Start all monitoring threads
        print(f"\nData collection started at {datetime.now().strftime('%H:%M:%S')}")
        for monitor in self.monitors:
            monitor.start()
        
        # Wait for duration with progress updates
        start_time = time.time()
        duration_sec = self.duration_min * 60
        
        while time.time() - start_time < duration_sec:
            elapsed_sec = int(time.time() - start_time)
            elapsed_min = elapsed_sec // 60
            progress_pct = int((elapsed_sec / duration_sec) * 100)
            progress_bar = '=' * (progress_pct // 5) + '>' + ' ' * (20 - progress_pct // 5)
            
            print(f"\rProgress: [{progress_bar}] {elapsed_min}/{self.duration_min} min ({progress_pct}%)", end='', flush=True)
            time.sleep(10)  # Update every 10 seconds
        
        print("\n\nData collection complete!")
        
        # Stop all monitors
        for monitor in self.monitors:
            monitor.stop()
        
        # Print summary
        self._print_summary()
        
        return True
    
    def _print_summary(self):
        """Print collection summary statistics"""
        print("\n=== Collection Summary ===")
        for monitor in self.monitors:
            print(f"\n{monitor.role} ({monitor.node_id}):")
            print(f"  Monitoring updates: {monitor.stats['monitoring_updates']}")
            print(f"  Max duty-cycle: {monitor.stats['duty_cycle_max']:.3f}%")
            print(f"  Min free memory: {monitor.stats['memory_min_kb']} KB")
            print(f"  Queue drops: {monitor.stats['queue_drops']}")
            print(f"  TX packets: {monitor.stats['tx_count']}")
            print(f"  RX packets: {monitor.stats['rx_count']}")
        
        print("\nFiles saved:")
        for monitor in self.monitors:
            print(f"  - {monitor.output_file}")


def main():
    parser = argparse.ArgumentParser(
        description='xMESH Multi-Node Data Collection Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Flooding protocol, run 1, 30 minutes
  python3 data_collection.py \\
    --protocol flooding \\
    --run-number 1 \\
    --duration 30 \\
    --sensor-port /dev/cu.usbserial-0001 \\
    --router-port /dev/cu.usbserial-0002 \\
    --gateway-port /dev/cu.usbserial-0003 \\
    --output-dir experiments/results
"""
    )
    
    parser.add_argument('--protocol', required=True,
                       choices=['flooding', 'hopcount', 'cost_routing'],
                       help='Protocol being tested')
    parser.add_argument('--run-number', type=int, required=True,
                       help='Run number (1, 2, or 3)')
    parser.add_argument('--duration', type=int, default=30,
                       help='Test duration in minutes (default: 30)')
    parser.add_argument('--sensor-port', required=True,
                       help='Serial port for sensor node (BB94)')
    parser.add_argument('--router-port', required=True,
                       help='Serial port for router node (6674)')
    parser.add_argument('--gateway-port', required=True,
                       help='Serial port for gateway node (D218)')
    parser.add_argument('--output-dir', default='experiments/results',
                       help='Output directory for CSV files')
    
    args = parser.parse_args()
    
    # Create collector
    collector = MultiNodeCollector(
        protocol=args.protocol,
        run_number=args.run_number,
        duration_min=args.duration,
        output_dir=args.output_dir
    )
    
    # Add nodes
    collector.add_node('BB94', args.sensor_port, 'SENSOR')
    collector.add_node('6674', args.router_port, 'ROUTER')
    collector.add_node('D218', args.gateway_port, 'GATEWAY')
    
    # Start collection
    try:
        success = collector.start_collection()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\n⚠️  Collection interrupted by user")
        for monitor in collector.monitors:
            monitor.stop()
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
