#!/usr/bin/env python3
"""
xMESH Data Collection Tool
Week 6-7: Performance Evaluation

Captures serial output from gateway node and extracts:
- Packet RX events (sequence, source, hops, RSSI, SNR)
- Routing table updates
- Link quality metrics
- Heartbeat status

Outputs:
- CSV file with timestamped events
- Real-time console monitoring
- Test run statistics summary
"""

import serial
import argparse
import csv
import sys
import re
from datetime import datetime
from pathlib import Path


class xMeshDataCollector:
    """Collects and logs data from xMESH gateway node"""
    
    def __init__(self, port, baudrate=115200, output_file=None):
        self.port = port
        self.baudrate = baudrate
        self.output_file = output_file
        self.serial = None
        self.csv_writer = None
        self.csv_file = None
        
        # Statistics
        self.stats = {
            'packets_received': 0,
            'route_changes': 0,
            'heartbeats': 0,
            'start_time': None,
            'last_seq': {}  # Track last sequence number per source
        }
        
        # Regular expressions for parsing
        self.patterns = {
            'rx': re.compile(r'RX: Seq=(\d+) From=([A-F0-9]+) Hops=(\d+) Value=([\d.]+)'),
            'link_quality': re.compile(r'Link quality: SNR=(-?\d+) dB, Est\.RSSI=(-?\d+) dBm'),
            'link_metrics': re.compile(r'([A-F0-9]+)\s+\|\s*(-?\d+)\s+\|\s*(-?\d+)\s+\|\s*([\d.]+)'),
            'heartbeat': re.compile(r'\[(\d+)\] Heartbeat - Node ([A-F0-9]+) \(([GSR])\) - Uptime: (\d+) sec'),
            'routing_entry': re.compile(r'([A-F0-9]+)\s+\|\s+([A-F0-9]+)\s+\|\s+(\d+)\s+\|\s+\d+\s+\|\s+([\d.]+)'),
            'gateway_rx': re.compile(r'GATEWAY: Packet (\d+) from ([A-F0-9]+) received \(hops=(\d+), value=([\d.]+)\)')
        }
    
    def open_serial(self):
        """Open serial connection to gateway"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0
            )
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to open serial port: {e}")
            return False
    
    def open_csv(self):
        """Open CSV file for writing"""
        if not self.output_file:
            return False
        
        try:
            # Create output directory if needed
            output_path = Path(self.output_file)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            
            self.csv_file = open(self.output_file, 'w', newline='')
            self.csv_writer = csv.writer(self.csv_file)
            
            # Write header
            self.csv_writer.writerow([
                'timestamp',
                'event_type',
                'seq_num',
                'src_addr',
                'dst_addr',
                'hops',
                'value',
                'rssi',
                'snr',
                'cost',
                'raw_line'
            ])
            
            print(f"✓ Logging to {self.output_file}")
            return True
        except IOError as e:
            print(f"✗ Failed to open output file: {e}")
            return False
    
    def parse_rx_packet(self, line, timestamp):
        """Parse RX packet line"""
        match = self.patterns['rx'].search(line)
        if match:
            seq, src, hops, value = match.groups()
            
            # Update statistics
            self.stats['packets_received'] += 1
            self.stats['last_seq'][src] = int(seq)
            
            event = {
                'timestamp': timestamp,
                'event_type': 'RX',
                'seq_num': seq,
                'src_addr': src,
                'dst_addr': 'GATEWAY',
                'hops': hops,
                'value': value,
                'rssi': '',
                'snr': '',
                'cost': '',
                'raw_line': line.strip()
            }
            
            return event
        return None
    
    def parse_link_quality(self, line, timestamp):
        """Parse link quality line"""
        match = self.patterns['link_quality'].search(line)
        if match:
            snr, rssi = match.groups()
            
            event = {
                'timestamp': timestamp,
                'event_type': 'LINK_QUALITY',
                'seq_num': '',
                'src_addr': '',
                'dst_addr': '',
                'hops': '',
                'value': '',
                'rssi': rssi,
                'snr': snr,
                'cost': '',
                'raw_line': line.strip()
            }
            
            return event
        return None
    
    def parse_link_metrics(self, line, timestamp):
        """Parse link metrics table entry"""
        match = self.patterns['link_metrics'].search(line)
        if match:
            addr, rssi, snr, etx = match.groups()
            
            event = {
                'timestamp': timestamp,
                'event_type': 'LINK_METRIC',
                'seq_num': '',
                'src_addr': addr,
                'dst_addr': '',
                'hops': '',
                'value': etx,  # Store ETX in value field
                'rssi': rssi,
                'snr': snr,
                'cost': '',
                'raw_line': line.strip()
            }
            
            return event
        return None
    
    def parse_heartbeat(self, line, timestamp):
        """Parse heartbeat line"""
        match = self.patterns['heartbeat'].search(line)
        if match:
            uptime, node_id, role, uptime_sec = match.groups()
            
            self.stats['heartbeats'] += 1
            
            event = {
                'timestamp': timestamp,
                'event_type': 'HEARTBEAT',
                'seq_num': uptime,
                'src_addr': node_id,
                'dst_addr': '',
                'hops': '',
                'value': uptime_sec,
                'rssi': '',
                'snr': '',
                'cost': '',
                'raw_line': line.strip()
            }
            
            return event
        return None
    
    def parse_routing_entry(self, line, timestamp):
        """Parse routing table entry with cost"""
        match = self.patterns['routing_entry'].search(line)
        if match:
            addr, via, hops, cost = match.groups()
            
            event = {
                'timestamp': timestamp,
                'event_type': 'ROUTE_ENTRY',
                'seq_num': '',
                'src_addr': addr,
                'dst_addr': via,
                'hops': hops,
                'value': '',
                'rssi': '',
                'snr': '',
                'cost': cost,
                'raw_line': line.strip()
            }
            
            return event
        return None
    
    def parse_line(self, line):
        """Parse a line of serial output"""
        timestamp = datetime.now().isoformat()
        
        # Try each pattern
        event = None
        event = self.parse_rx_packet(line, timestamp)
        if event:
            return event
        
        event = self.parse_link_quality(line, timestamp)
        if event:
            return event
        
        event = self.parse_link_metrics(line, timestamp)
        if event:
            return event
        
        event = self.parse_heartbeat(line, timestamp)
        if event:
            return event
        
        event = self.parse_routing_entry(line, timestamp)
        if event:
            return event
        
        return None
    
    def write_event(self, event):
        """Write event to CSV"""
        if self.csv_writer:
            self.csv_writer.writerow([
                event['timestamp'],
                event['event_type'],
                event['seq_num'],
                event['src_addr'],
                event['dst_addr'],
                event['hops'],
                event['value'],
                event['rssi'],
                event['snr'],
                event['cost'],
                event['raw_line']
            ])
            self.csv_file.flush()  # Ensure data is written immediately
    
    def print_event(self, event):
        """Print event to console"""
        if event['event_type'] == 'RX':
            print(f"[{event['timestamp']}] 📦 RX: Seq={event['seq_num']} From={event['src_addr']} Hops={event['hops']} Value={event['value']}")
        elif event['event_type'] == 'HEARTBEAT':
            print(f"[{event['timestamp']}] 💓 Heartbeat: Node {event['src_addr']} Uptime={event['value']}s")
        elif event['event_type'] == 'LINK_QUALITY':
            print(f"[{event['timestamp']}] 📡 Link Quality: RSSI={event['rssi']} dBm, SNR={event['snr']} dB")
        elif event['event_type'] == 'LINK_METRIC':
            print(f"[{event['timestamp']}] 📊 Link Metric: {event['src_addr']} RSSI={event['rssi']} SNR={event['snr']} ETX={event['value']}")
        elif event['event_type'] == 'ROUTE_ENTRY':
            print(f"[{event['timestamp']}] 🛤️  Route: {event['src_addr']} via {event['dst_addr']} Hops={event['hops']} Cost={event['cost']}")
    
    def print_stats(self):
        """Print current statistics"""
        if not self.stats['start_time']:
            return
        
        elapsed = (datetime.now() - self.stats['start_time']).total_seconds()
        
        print("\n" + "="*60)
        print("📊 TEST RUN STATISTICS")
        print("="*60)
        print(f"Duration: {elapsed:.1f} seconds ({elapsed/60:.1f} minutes)")
        print(f"Packets Received: {self.stats['packets_received']}")
        print(f"Heartbeats: {self.stats['heartbeats']}")
        print(f"Route Changes: {self.stats['route_changes']}")
        
        if self.stats['packets_received'] > 0:
            pdr = self.stats['packets_received'] / max(self.stats['last_seq'].values()) * 100 if self.stats['last_seq'] else 0
            print(f"Estimated PDR: {pdr:.1f}%")
        
        print("\nLast Sequence Numbers:")
        for src, seq in self.stats['last_seq'].items():
            print(f"  {src}: {seq}")
        
        print("="*60 + "\n")
    
    def run(self):
        """Main collection loop"""
        if not self.open_serial():
            return False
        
        self.open_csv()
        
        print("\n" + "="*60)
        print("🚀 xMESH Data Collection Started")
        print("="*60)
        print(f"Port: {self.port}")
        print(f"Baudrate: {self.baudrate}")
        print(f"Output: {self.output_file or 'Console only'}")
        print("\nPress Ctrl+C to stop")
        print("="*60 + "\n")
        
        self.stats['start_time'] = datetime.now()
        
        try:
            while True:
                # Read line from serial
                if self.serial.in_waiting > 0:
                    line = self.serial.readline().decode('utf-8', errors='ignore')
                    
                    # Parse the line
                    event = self.parse_line(line)
                    
                    if event:
                        self.write_event(event)
                        self.print_event(event)
                
        except KeyboardInterrupt:
            print("\n\n🛑 Stopping data collection...")
            self.print_stats()
        
        finally:
            if self.serial:
                self.serial.close()
            if self.csv_file:
                self.csv_file.close()
            
            print("\n✓ Data collection complete")
            return True


def main():
    parser = argparse.ArgumentParser(
        description='xMESH Data Collection Tool - Capture and log mesh network data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Log to CSV file
  python data_collection.py --port /dev/cu.usbserial-0001 --output results/flooding/run1.csv
  
  # Console monitoring only
  python data_collection.py --port /dev/cu.usbserial-0001
  
  # Custom baudrate
  python data_collection.py --port COM3 --baudrate 9600 --output test.csv
        """
    )
    
    parser.add_argument(
        '--port',
        type=str,
        required=True,
        help='Serial port (e.g., /dev/cu.usbserial-0001 or COM3)'
    )
    
    parser.add_argument(
        '--baudrate',
        type=int,
        default=115200,
        help='Serial baudrate (default: 115200)'
    )
    
    parser.add_argument(
        '--output',
        type=str,
        default=None,
        help='Output CSV file path (optional, for logging)'
    )
    
    args = parser.parse_args()
    
    collector = xMeshDataCollector(
        port=args.port,
        baudrate=args.baudrate,
        output_file=args.output
    )
    
    success = collector.run()
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
