#!/usr/bin/env python3
"""
xMESH Gateway-Only Data Collection Tool
Simplified version for single-cable setup

Captures monitoring data from gateway node:
- Packet reception (PDR calculation)
- Monitoring stats (duty-cycle, memory, queue)
- Protocol-specific metrics

This is sufficient for:
- Scalability analysis (Week 2)
- Hardware validation (Week 1)
- Thesis Results chapter (Week 4)
"""

import serial
import argparse
import csv
import sys
import re
import time
from datetime import datetime
from pathlib import Path


class GatewayMonitor:
    """Monitors gateway node serial output"""
    
    def __init__(self, port, protocol, run_number, output_dir):
        self.port = port
        self.protocol = protocol
        self.run_number = run_number
        self.output_dir = Path(output_dir)
        self.serial = None
        self.csv_writer = None
        self.csv_file = None
        
        # Statistics
        self.stats = {
            'packets_received': 0,
            'unique_sources': set(),
            'monitoring_updates': 0,
            'duty_cycle_max': 0.0,
            'memory_min_kb': 9999,
            'start_time': None,
            'packet_timestamps': []  # For PDR calculation
        }
        
        # Regex patterns
        self.patterns = {
            'monitoring_header': re.compile(r'==== Network Monitoring Stats ===='),
            'channel': re.compile(r'Channel: ([\d.]+)% duty-cycle, (\d+) TX, (\d+) violations'),
            'memory': re.compile(r'Memory: (\d+)/(\d+) KB free, Min: (\d+) KB, Peak: (\d+) KB'),
            'queue': re.compile(r'Queue: (\d+) enqueued, (\d+) dropped \(([\d.]+)%\), max depth: (\d+)'),
            'rx_packet': re.compile(r'RX: Seq=(\d+) From=([A-F0-9]+) Hops=(\d+)'),
            'gateway_packet': re.compile(r'GATEWAY: Packet (\d+) from ([A-F0-9]+) received \(hops=(\d+)'),
            'routing_table_size': re.compile(r'Routing table: (\d+) entries'),
        }
    
    def connect(self):
        """Connect to serial port and open CSV file"""
        try:
            self.serial = serial.Serial(self.port, 115200, timeout=1)
            time.sleep(2)
            
            # Create output directory
            protocol_dir = self.output_dir / self.protocol
            protocol_dir.mkdir(parents=True, exist_ok=True)
            
            # Open CSV file
            output_file = protocol_dir / f"run{self.run_number}_gateway.csv"
            self.csv_file = open(output_file, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)
            
            # Write header
            self.csv_writer.writerow([
                'timestamp',
                'event_type',  # 'monitoring' or 'packet'
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
                'packet_seq',
                'packet_source',
                'packet_hops',
                'routing_table_entries',
                'raw_line'
            ])
            
            print(f"✓ Connected to gateway: {self.port}")
            print(f"✓ Output file: {output_file}")
            return True
            
        except Exception as e:
            print(f"❌ Error connecting: {e}")
            return False
    
    def collect_data(self, duration_min):
        """Collect data for specified duration"""
        print(f"\n=== Gateway Data Collection ===")
        print(f"Protocol: {self.protocol}")
        print(f"Run: {self.run_number}")
        print(f"Duration: {duration_min} minutes\n")
        
        self.stats['start_time'] = datetime.now()
        start_time = time.time()
        duration_sec = duration_min * 60
        
        current_monitoring = {}
        
        print(f"Collection started at {self.stats['start_time'].strftime('%H:%M:%S')}")
        print("Waiting for data...\n")
        
        while time.time() - start_time < duration_sec:
            try:
                if self.serial.in_waiting:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    
                    if not line:
                        continue
                    
                    timestamp = datetime.now().isoformat()
                    
                    # Check for monitoring header
                    if self.patterns['monitoring_header'].search(line):
                        current_monitoring = {'timestamp': timestamp, 'event_type': 'monitoring'}
                    
                    # Parse channel stats
                    match = self.patterns['channel'].search(line)
                    if match:
                        duty_cycle = float(match.group(1))
                        tx_count = int(match.group(2))
                        violations = int(match.group(3))
                        current_monitoring.update({
                            'duty_cycle_pct': duty_cycle,
                            'tx_count': tx_count,
                            'violations': violations
                        })
                        if duty_cycle > self.stats['duty_cycle_max']:
                            self.stats['duty_cycle_max'] = duty_cycle
                    
                    # Parse memory
                    match = self.patterns['memory'].search(line)
                    if match:
                        mem_free = int(match.group(1))
                        mem_total = int(match.group(2))
                        mem_min = int(match.group(3))
                        mem_peak = int(match.group(4))
                        current_monitoring.update({
                            'memory_free_kb': mem_free,
                            'memory_total_kb': mem_total,
                            'memory_min_kb': mem_min,
                            'memory_peak_kb': mem_peak
                        })
                        if mem_min < self.stats['memory_min_kb']:
                            self.stats['memory_min_kb'] = mem_min
                    
                    # Parse queue
                    match = self.patterns['queue'].search(line)
                    if match:
                        enqueued = int(match.group(1))
                        dropped = int(match.group(2))
                        drop_rate = float(match.group(3))
                        max_depth = int(match.group(4))
                        current_monitoring.update({
                            'queue_enqueued': enqueued,
                            'queue_dropped': dropped,
                            'queue_drop_rate_pct': drop_rate,
                            'queue_max_depth': max_depth
                        })
                    
                    # Parse routing table size
                    match = self.patterns['routing_table_size'].search(line)
                    if match:
                        rt_size = int(match.group(1))
                        current_monitoring['routing_table_entries'] = rt_size
                    
                    # Write complete monitoring data
                    if 'duty_cycle_pct' in current_monitoring and 'memory_free_kb' in current_monitoring:
                        self._write_row(current_monitoring, line)
                        self.stats['monitoring_updates'] += 1
                        current_monitoring = {}
                    
                    # Parse received packets
                    match = self.patterns['gateway_packet'].search(line)
                    if not match:
                        match = self.patterns['rx_packet'].search(line)
                    
                    if match:
                        seq = int(match.group(1))
                        source = match.group(2)
                        hops = int(match.group(3))
                        
                        packet_data = {
                            'timestamp': timestamp,
                            'event_type': 'packet',
                            'packet_seq': seq,
                            'packet_source': source,
                            'packet_hops': hops
                        }
                        self._write_row(packet_data, line)
                        
                        self.stats['packets_received'] += 1
                        self.stats['unique_sources'].add(source)
                        self.stats['packet_timestamps'].append(time.time())
                
                # Progress update every 5 minutes
                elapsed_sec = int(time.time() - start_time)
                elapsed_min = elapsed_sec // 60
                if elapsed_sec % 300 == 0 and elapsed_sec > 0:  # Every 5 minutes
                    progress_pct = int((elapsed_sec / duration_sec) * 100)
                    print(f"Progress: {elapsed_min}/{duration_min} min ({progress_pct}%) - "
                          f"Packets: {self.stats['packets_received']}, "
                          f"Monitoring updates: {self.stats['monitoring_updates']}")
                
            except Exception as e:
                print(f"⚠️  Error reading data: {e}")
                time.sleep(0.1)
        
        print(f"\n✓ Collection complete!")
        return True
    
    def _write_row(self, data, raw_line):
        """Write data row to CSV"""
        self.csv_writer.writerow([
            data.get('timestamp', ''),
            data.get('event_type', ''),
            data.get('duty_cycle_pct', ''),
            data.get('tx_count', ''),
            data.get('violations', ''),
            data.get('memory_free_kb', ''),
            data.get('memory_total_kb', ''),
            data.get('memory_min_kb', ''),
            data.get('memory_peak_kb', ''),
            data.get('queue_enqueued', ''),
            data.get('queue_dropped', ''),
            data.get('queue_drop_rate_pct', ''),
            data.get('queue_max_depth', ''),
            data.get('packet_seq', ''),
            data.get('packet_source', ''),
            data.get('packet_hops', ''),
            data.get('routing_table_entries', ''),
            raw_line
        ])
        self.csv_file.flush()
    
    def print_summary(self):
        """Print collection summary"""
        duration = (datetime.now() - self.stats['start_time']).total_seconds() / 60
        
        print("\n=== Collection Summary ===")
        print(f"Duration: {duration:.1f} minutes")
        print(f"Packets received: {self.stats['packets_received']}")
        print(f"Unique sources: {len(self.stats['unique_sources'])} ({', '.join(self.stats['unique_sources'])})")
        print(f"Monitoring updates: {self.stats['monitoring_updates']}")
        print(f"Max duty-cycle: {self.stats['duty_cycle_max']:.3f}%")
        print(f"Min free memory: {self.stats['memory_min_kb']} KB")
        
        # Calculate approximate PDR
        if len(self.stats['packet_timestamps']) > 1:
            test_duration_sec = self.stats['packet_timestamps'][-1] - self.stats['packet_timestamps'][0]
            expected_packets = int(test_duration_sec / 60)  # 1 packet per minute
            if expected_packets > 0:
                pdr = (self.stats['packets_received'] / expected_packets) * 100
                print(f"Approximate PDR: {pdr:.1f}% ({self.stats['packets_received']}/{expected_packets} packets)")
    
    def close(self):
        """Close connections"""
        if self.csv_file:
            self.csv_file.close()
        if self.serial:
            self.serial.close()


def main():
    parser = argparse.ArgumentParser(
        description='xMESH Gateway-Only Data Collection (Single Cable Setup)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example:
  python3 gateway_data_collection.py \\
    --port /dev/cu.usbserial-0001 \\
    --protocol flooding \\
    --run-number 1 \\
    --duration 30 \\
    --output-dir experiments/results
"""
    )
    
    parser.add_argument('--port', required=True,
                       help='Serial port for gateway node (D218)')
    parser.add_argument('--protocol', required=True,
                       choices=['flooding', 'hopcount', 'cost_routing'],
                       help='Protocol being tested')
    parser.add_argument('--run-number', type=int, required=True,
                       help='Run number (1, 2, or 3)')
    parser.add_argument('--duration', type=int, default=30,
                       help='Test duration in minutes (default: 30)')
    parser.add_argument('--output-dir', default='experiments/results',
                       help='Output directory for CSV files')
    
    args = parser.parse_args()
    
    # Create monitor
    monitor = GatewayMonitor(
        port=args.port,
        protocol=args.protocol,
        run_number=args.run_number,
        output_dir=args.output_dir
    )
    
    # Connect and collect
    try:
        if not monitor.connect():
            sys.exit(1)
        
        if not monitor.collect_data(args.duration):
            sys.exit(1)
        
        monitor.print_summary()
        monitor.close()
        sys.exit(0)
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Collection interrupted by user")
        monitor.close()
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        monitor.close()
        sys.exit(1)


if __name__ == '__main__':
    main()
