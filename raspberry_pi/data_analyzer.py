#!/usr/bin/env python3
"""
Data Analyzer for xMESH LoRaMesher Experiments
Analyzes CSV data collected from experiments and generates statistics
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
import argparse
import json
from pathlib import Path
from datetime import datetime

class DataAnalyzer:
    def __init__(self, csv_file):
        self.csv_file = csv_file
        self.df = None
        self.results = {}

    def load_data(self):
        """Load CSV data into DataFrame"""
        try:
            self.df = pd.read_csv(self.csv_file)
            print(f"Loaded {len(self.df)} records from {self.csv_file}")

            # Convert timestamp to datetime if needed
            if 'timestamp' in self.df.columns:
                self.df['datetime'] = pd.to_datetime(self.df['timestamp'], unit='ms')

            return True
        except Exception as e:
            print(f"Error loading data: {e}")
            return False

    def calculate_pdr(self):
        """Calculate Packet Delivery Ratio"""
        # Count transmitted packets from sensor nodes (1-2)
        tx_packets = self.df[
            (self.df['event_type'] == 'TX') &
            (self.df['node_id'].isin([1, 2]))
        ]

        # Count received packets at gateway (node 5)
        rx_packets = self.df[
            (self.df['event_type'] == 'RX') &
            (self.df['node_id'] == 5)
        ]

        # Group by sequence number to avoid counting duplicates
        unique_tx = tx_packets['sequence'].nunique() if 'sequence' in tx_packets.columns else len(tx_packets)
        unique_rx = rx_packets['sequence'].nunique() if 'sequence' in rx_packets.columns else len(rx_packets)

        pdr = (unique_rx / unique_tx * 100) if unique_tx > 0 else 0

        self.results['pdr'] = {
            'value': pdr,
            'packets_sent': unique_tx,
            'packets_received': unique_rx,
            'packets_lost': unique_tx - unique_rx
        }

        print(f"PDR: {pdr:.2f}% ({unique_rx}/{unique_tx} packets)")
        return pdr

    def calculate_latency(self):
        """Calculate end-to-end latency"""
        latencies = []

        # For each sequence number, find TX and RX times
        if 'sequence' in self.df.columns:
            sequences = self.df['sequence'].unique()

            for seq in sequences:
                # Find transmission time
                tx_events = self.df[
                    (self.df['event_type'] == 'TX') &
                    (self.df['sequence'] == seq)
                ]

                # Find reception time at gateway
                rx_events = self.df[
                    (self.df['event_type'] == 'RX') &
                    (self.df['node_id'] == 5) &
                    (self.df['sequence'] == seq)
                ]

                if not tx_events.empty and not rx_events.empty:
                    tx_time = tx_events.iloc[0]['timestamp']
                    rx_time = rx_events.iloc[0]['timestamp']
                    latency = rx_time - tx_time
                    if latency > 0:  # Sanity check
                        latencies.append(latency)

        if latencies:
            self.results['latency'] = {
                'mean': np.mean(latencies),
                'median': np.median(latencies),
                'std': np.std(latencies),
                'min': np.min(latencies),
                'max': np.max(latencies),
                'p95': np.percentile(latencies, 95),
                'p99': np.percentile(latencies, 99),
                'samples': len(latencies)
            }

            print(f"Latency: Mean={np.mean(latencies):.2f}ms, "
                  f"Median={np.median(latencies):.2f}ms, "
                  f"P95={np.percentile(latencies, 95):.2f}ms")
        else:
            print("No valid latency measurements found")

        return latencies

    def calculate_overhead(self):
        """Calculate control overhead"""
        # Count control packets (HELLO, ACK)
        control_packets = self.df[
            self.df['event_type'].isin(['HELLO', 'ACK', 'ROUTE'])
        ]

        # Count data packets
        data_packets = self.df[
            self.df['event_type'].isin(['TX', 'RX', 'FWD'])
        ]

        total_packets = len(control_packets) + len(data_packets)
        overhead_ratio = (len(control_packets) / total_packets * 100) if total_packets > 0 else 0

        self.results['overhead'] = {
            'ratio': overhead_ratio,
            'control_packets': len(control_packets),
            'data_packets': len(data_packets),
            'total_packets': total_packets
        }

        # Calculate by type
        overhead_by_type = control_packets['event_type'].value_counts().to_dict()
        self.results['overhead']['by_type'] = overhead_by_type

        print(f"Overhead: {overhead_ratio:.2f}% ({len(control_packets)}/{total_packets} packets)")
        return overhead_ratio

    def analyze_link_quality(self):
        """Analyze RSSI and SNR statistics"""
        # Filter valid RSSI/SNR values
        valid_rssi = self.df[
            (self.df['rssi'] != 0) &
            (self.df['rssi'] > -120) &
            (self.df['rssi'] < 0)
        ]['rssi']

        valid_snr = self.df[
            (self.df['snr'] != 0) &
            (self.df['snr'] > -20) &
            (self.df['snr'] < 30)
        ]['snr']

        self.results['link_quality'] = {
            'rssi': {
                'mean': valid_rssi.mean() if not valid_rssi.empty else 0,
                'std': valid_rssi.std() if not valid_rssi.empty else 0,
                'min': valid_rssi.min() if not valid_rssi.empty else 0,
                'max': valid_rssi.max() if not valid_rssi.empty else 0
            },
            'snr': {
                'mean': valid_snr.mean() if not valid_snr.empty else 0,
                'std': valid_snr.std() if not valid_snr.empty else 0,
                'min': valid_snr.min() if not valid_snr.empty else 0,
                'max': valid_snr.max() if not valid_snr.empty else 0
            }
        }

        print(f"RSSI: Mean={valid_rssi.mean():.1f}dBm, Std={valid_rssi.std():.1f}")
        print(f"SNR: Mean={valid_snr.mean():.1f}dB, Std={valid_snr.std():.1f}")

    def analyze_routing(self):
        """Analyze routing behavior"""
        # Count route updates
        route_updates = self.df[self.df['event_type'] == 'ROUTE']

        # Analyze hop counts
        hop_counts = self.df[self.df['hop_count'] > 0]['hop_count']

        # Analyze route costs
        route_costs = self.df[self.df['cost'] > 0]['cost']

        self.results['routing'] = {
            'route_updates': len(route_updates),
            'avg_hop_count': hop_counts.mean() if not hop_counts.empty else 0,
            'max_hop_count': hop_counts.max() if not hop_counts.empty else 0,
            'avg_cost': route_costs.mean() if not route_costs.empty else 0,
            'route_changes': 0  # TODO: Calculate route changes
        }

        print(f"Routing: {len(route_updates)} updates, "
              f"Avg hops={hop_counts.mean():.1f} if not hop_counts.empty else 0")

    def analyze_duty_cycle(self):
        """Analyze duty cycle usage"""
        # This would require airtime information
        # For now, estimate based on packet counts and sizes

        nodes = self.df['node_id'].unique()
        duty_cycles = {}

        for node in nodes:
            node_packets = self.df[
                (self.df['node_id'] == node) &
                (self.df['event_type'].isin(['TX', 'FWD']))
            ]

            if not node_packets.empty:
                # Estimate airtime (rough calculation)
                # SF7, BW125: ~50ms for 20-byte packet
                avg_packet_size = node_packets['packet_size'].mean() if 'packet_size' in node_packets.columns else 20
                airtime_per_packet = 50  # ms
                total_airtime = len(node_packets) * airtime_per_packet

                # Calculate percentage over experiment duration
                experiment_duration = (self.df['timestamp'].max() - self.df['timestamp'].min())
                duty_cycle = (total_airtime / experiment_duration * 100) if experiment_duration > 0 else 0

                duty_cycles[int(node)] = duty_cycle

        self.results['duty_cycle'] = duty_cycles
        print(f"Duty cycles: {duty_cycles}")

    def plot_timeline(self, output_dir=None):
        """Plot packet timeline"""
        fig, axes = plt.subplots(3, 1, figsize=(12, 8))

        # Plot 1: Packet events over time
        events = self.df.groupby(['timestamp', 'event_type']).size().unstack(fill_value=0)
        events.plot(ax=axes[0], kind='area', stacked=True)
        axes[0].set_title('Packet Events Over Time')
        axes[0].set_xlabel('Time (ms)')
        axes[0].set_ylabel('Packet Count')

        # Plot 2: RSSI over time
        rssi_data = self.df[self.df['rssi'] != 0]
        if not rssi_data.empty:
            axes[1].scatter(rssi_data['timestamp'], rssi_data['rssi'], alpha=0.5, s=10)
            axes[1].set_title('RSSI Over Time')
            axes[1].set_xlabel('Time (ms)')
            axes[1].set_ylabel('RSSI (dBm)')

        # Plot 3: PDR over time (sliding window)
        window_size = 100  # packets
        pdr_timeline = []
        timestamps = []

        for i in range(window_size, len(self.df), 10):
            window = self.df.iloc[i-window_size:i]
            tx = len(window[(window['event_type'] == 'TX') & (window['node_id'].isin([1, 2]))])
            rx = len(window[(window['event_type'] == 'RX') & (window['node_id'] == 5)])
            pdr = (rx / tx * 100) if tx > 0 else 0
            pdr_timeline.append(pdr)
            timestamps.append(window['timestamp'].mean())

        if pdr_timeline:
            axes[2].plot(timestamps, pdr_timeline)
            axes[2].set_title('PDR Over Time (Sliding Window)')
            axes[2].set_xlabel('Time (ms)')
            axes[2].set_ylabel('PDR (%)')

        plt.tight_layout()

        if output_dir:
            plt.savefig(Path(output_dir) / 'timeline.png', dpi=300)
        plt.show()

    def plot_distributions(self, output_dir=None):
        """Plot statistical distributions"""
        fig, axes = plt.subplots(2, 2, figsize=(12, 8))

        # Plot 1: RSSI distribution
        rssi_data = self.df[(self.df['rssi'] != 0) & (self.df['rssi'] > -120)]['rssi']
        if not rssi_data.empty:
            axes[0, 0].hist(rssi_data, bins=30, edgecolor='black')
            axes[0, 0].set_title('RSSI Distribution')
            axes[0, 0].set_xlabel('RSSI (dBm)')
            axes[0, 0].set_ylabel('Frequency')

        # Plot 2: SNR distribution
        snr_data = self.df[(self.df['snr'] != 0)]['snr']
        if not snr_data.empty:
            axes[0, 1].hist(snr_data, bins=30, edgecolor='black')
            axes[0, 1].set_title('SNR Distribution')
            axes[0, 1].set_xlabel('SNR (dB)')
            axes[0, 1].set_ylabel('Frequency')

        # Plot 3: Hop count distribution
        hop_data = self.df[self.df['hop_count'] > 0]['hop_count']
        if not hop_data.empty:
            axes[1, 0].hist(hop_data, bins=range(1, hop_data.max() + 2), edgecolor='black')
            axes[1, 0].set_title('Hop Count Distribution')
            axes[1, 0].set_xlabel('Hop Count')
            axes[1, 0].set_ylabel('Frequency')

        # Plot 4: Packet type distribution
        event_counts = self.df['event_type'].value_counts()
        axes[1, 1].bar(event_counts.index, event_counts.values)
        axes[1, 1].set_title('Packet Type Distribution')
        axes[1, 1].set_xlabel('Event Type')
        axes[1, 1].set_ylabel('Count')
        axes[1, 1].tick_params(axis='x', rotation=45)

        plt.tight_layout()

        if output_dir:
            plt.savefig(Path(output_dir) / 'distributions.png', dpi=300)
        plt.show()

    def generate_report(self, output_file=None):
        """Generate comprehensive analysis report"""
        report = {
            'file': str(self.csv_file),
            'analysis_time': datetime.now().isoformat(),
            'total_records': len(self.df),
            'duration_ms': int(self.df['timestamp'].max() - self.df['timestamp'].min()) if not self.df.empty else 0,
            'nodes': list(self.df['node_id'].unique().astype(int)),
            'results': self.results
        }

        # Print report
        print("\n" + "=" * 60)
        print("ANALYSIS REPORT")
        print("=" * 60)
        print(f"File: {self.csv_file}")
        print(f"Records: {report['total_records']}")
        print(f"Duration: {report['duration_ms'] / 1000:.1f} seconds")
        print(f"Nodes: {report['nodes']}")
        print("-" * 60)

        for metric, values in self.results.items():
            print(f"\n{metric.upper()}:")
            if isinstance(values, dict):
                for key, value in values.items():
                    if isinstance(value, float):
                        print(f"  {key}: {value:.3f}")
                    else:
                        print(f"  {key}: {value}")
            else:
                print(f"  {values}")

        print("=" * 60)

        # Save to JSON if requested
        if output_file:
            with open(output_file, 'w') as f:
                json.dump(report, f, indent=2, default=str)
            print(f"\nReport saved to {output_file}")

        return report

    def compare_protocols(self, other_files):
        """Compare results across multiple protocol runs"""
        comparison = {}

        # Analyze all files
        for file in [self.csv_file] + other_files:
            analyzer = DataAnalyzer(file)
            if analyzer.load_data():
                analyzer.calculate_pdr()
                analyzer.calculate_latency()
                analyzer.calculate_overhead()

                protocol_name = Path(file).stem
                comparison[protocol_name] = analyzer.results

        # Create comparison plots
        # ... (implementation for comparison plots)

        return comparison

def main():
    parser = argparse.ArgumentParser(description='Analyze xMESH experiment data')
    parser.add_argument('csv_file', help='Input CSV file')
    parser.add_argument('-o', '--output-dir', help='Output directory for plots')
    parser.add_argument('-r', '--report', help='Save JSON report to file')
    parser.add_argument('--plot', action='store_true', help='Generate plots')
    parser.add_argument('--compare', nargs='+', help='Compare with other CSV files')

    args = parser.parse_args()

    # Create output directory if specified
    if args.output_dir:
        Path(args.output_dir).mkdir(parents=True, exist_ok=True)

    # Create analyzer
    analyzer = DataAnalyzer(args.csv_file)

    if not analyzer.load_data():
        print("Failed to load data")
        return 1

    # Run analysis
    print("Running analysis...")
    analyzer.calculate_pdr()
    analyzer.calculate_latency()
    analyzer.calculate_overhead()
    analyzer.analyze_link_quality()
    analyzer.analyze_routing()
    analyzer.analyze_duty_cycle()

    # Generate plots if requested
    if args.plot:
        print("\nGenerating plots...")
        analyzer.plot_timeline(args.output_dir)
        analyzer.plot_distributions(args.output_dir)

    # Compare protocols if requested
    if args.compare:
        print("\nComparing protocols...")
        comparison = analyzer.compare_protocols(args.compare)
        # TODO: Generate comparison plots and statistics

    # Generate report
    analyzer.generate_report(args.report)

    return 0

if __name__ == "__main__":
    exit(main())