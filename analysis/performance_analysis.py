#!/usr/bin/env python3
"""
xMESH Performance Analysis
Week 6-7: Compare Flooding, Hop-Count, and Cost Routing

Calculates:
- Packet Delivery Ratio (PDR)
- Latency statistics
- Network overhead
- Route stability

Generates:
- Comparison tables
- Statistical test results
- Visualization plots
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
import argparse


class xMeshAnalyzer:
    """Analyzes xMESH performance data"""
    
    def __init__(self, data_dir):
        self.data_dir = Path(data_dir)
        self.protocols = ['flooding', 'hopcount', 'cost_routing']
        self.data = {}
        
        # Set plot style
        sns.set_style('whitegrid')
        sns.set_palette('Set2')
    
    def load_protocol_data(self, protocol):
        """Load all CSV files for a protocol"""
        protocol_dir = self.data_dir / protocol
        
        if not protocol_dir.exists():
            print(f"⚠️  Directory not found: {protocol_dir}")
            return None
        
        csv_files = list(protocol_dir.glob('run*.csv'))
        
        if not csv_files:
            print(f"⚠️  No CSV files found in {protocol_dir}")
            return None
        
        print(f"Loading {len(csv_files)} files for {protocol}...")
        
        dfs = []
        for csv_file in csv_files:
            try:
                df = pd.read_csv(csv_file)
                df['run'] = csv_file.stem  # Add run identifier
                df['protocol'] = protocol
                dfs.append(df)
            except Exception as e:
                print(f"  ✗ Error loading {csv_file}: {e}")
        
        if dfs:
            combined = pd.concat(dfs, ignore_index=True)
            print(f"  ✓ Loaded {len(combined)} events")
            return combined
        
        return None
    
    def load_all_data(self):
        """Load data for all protocols"""
        print("\n" + "="*60)
        print("📂 LOADING DATA")
        print("="*60)
        
        for protocol in self.protocols:
            data = self.load_protocol_data(protocol)
            if data is not None:
                self.data[protocol] = data
        
        if not self.data:
            print("\n✗ No data loaded. Check data directory.")
            return False
        
        print(f"\n✓ Loaded data for {len(self.data)} protocols")
        return True
    
    def calculate_pdr(self, protocol_data):
        """Calculate Packet Delivery Ratio"""
        # Filter RX events at gateway
        rx_events = protocol_data[protocol_data['event_type'] == 'RX']
        
        if rx_events.empty:
            return None
        
        # Group by run and source
        pdr_by_run = []
        
        for run in rx_events['run'].unique():
            run_data = rx_events[rx_events['run'] == run]
            
            for src in run_data['src_addr'].unique():
                src_data = run_data[run_data['src_addr'] == src]
                
                # Get sequence numbers
                seq_nums = src_data['seq_num'].astype(int)
                
                if len(seq_nums) == 0:
                    continue
                
                # Calculate PDR
                max_seq = seq_nums.max()
                packets_received = len(seq_nums.unique())
                pdr = (packets_received / max_seq) * 100 if max_seq > 0 else 0
                
                pdr_by_run.append({
                    'run': run,
                    'source': src,
                    'max_seq': max_seq,
                    'received': packets_received,
                    'pdr': pdr
                })
        
        return pd.DataFrame(pdr_by_run)
    
    def calculate_overhead(self, protocol_data):
        """Calculate network overhead (placeholder - needs TX counts from all nodes)"""
        # This is a simplified calculation
        # In real scenario, we'd need TX counts from all nodes
        
        rx_events = protocol_data[protocol_data['event_type'] == 'RX']
        
        # For now, just count received packets
        # Actual overhead calculation would need total TX from all nodes
        
        return {
            'packets_received': len(rx_events),
            'note': 'Full overhead calculation requires TX logs from all nodes'
        }
    
    def calculate_route_stability(self, protocol_data):
        """Calculate route change frequency"""
        route_events = protocol_data[protocol_data['event_type'] == 'ROUTE_ENTRY']
        
        if route_events.empty:
            return None
        
        # Track route changes per source
        changes_by_src = []
        
        for src in route_events['src_addr'].unique():
            src_routes = route_events[route_events['src_addr'] == src].sort_values('timestamp')
            
            # Count how many times the via changed
            changes = 0
            prev_via = None
            
            for via in src_routes['dst_addr']:
                if prev_via is not None and via != prev_via:
                    changes += 1
                prev_via = via
            
            changes_by_src.append({
                'source': src,
                'route_changes': changes,
                'samples': len(src_routes)
            })
        
        return pd.DataFrame(changes_by_src)
    
    def print_pdr_summary(self):
        """Print PDR comparison"""
        print("\n" + "="*60)
        print("📊 PACKET DELIVERY RATIO (PDR)")
        print("="*60)
        
        pdr_results = {}
        
        for protocol, data in self.data.items():
            pdr_df = self.calculate_pdr(data)
            
            if pdr_df is not None and not pdr_df.empty:
                mean_pdr = pdr_df['pdr'].mean()
                std_pdr = pdr_df['pdr'].std()
                min_pdr = pdr_df['pdr'].min()
                max_pdr = pdr_df['pdr'].max()
                
                pdr_results[protocol] = {
                    'mean': mean_pdr,
                    'std': std_pdr,
                    'min': min_pdr,
                    'max': max_pdr
                }
                
                print(f"\n{protocol.upper()}:")
                print(f"  Mean PDR: {mean_pdr:.2f}% ± {std_pdr:.2f}%")
                print(f"  Range: {min_pdr:.2f}% - {max_pdr:.2f}%")
                print(f"  Samples: {len(pdr_df)}")
        
        return pdr_results
    
    def print_link_quality_summary(self):
        """Print link quality metrics (for cost routing)"""
        print("\n" + "="*60)
        print("📡 LINK QUALITY METRICS")
        print("="*60)
        
        for protocol, data in self.data.items():
            link_metrics = data[data['event_type'] == 'LINK_METRIC']
            
            if not link_metrics.empty:
                print(f"\n{protocol.upper()}:")
                
                # RSSI statistics
                rssi_values = pd.to_numeric(link_metrics['rssi'], errors='coerce').dropna()
                if not rssi_values.empty:
                    print(f"  RSSI: {rssi_values.mean():.1f} ± {rssi_values.std():.1f} dBm")
                    print(f"    Range: {rssi_values.min():.1f} to {rssi_values.max():.1f} dBm")
                
                # SNR statistics
                snr_values = pd.to_numeric(link_metrics['snr'], errors='coerce').dropna()
                if not snr_values.empty:
                    print(f"  SNR: {snr_values.mean():.1f} ± {snr_values.std():.1f} dB")
                    print(f"    Range: {snr_values.min():.1f} to {snr_values.max():.1f} dB")
                
                # ETX statistics (stored in value field)
                etx_values = pd.to_numeric(link_metrics['value'], errors='coerce').dropna()
                if not etx_values.empty:
                    print(f"  ETX: {etx_values.mean():.2f} ± {etx_values.std():.2f}")
    
    def plot_pdr_comparison(self, output_dir):
        """Create PDR comparison plot"""
        fig, ax = plt.subplots(figsize=(10, 6))
        
        pdr_data = []
        labels = []
        
        for protocol, data in self.data.items():
            pdr_df = self.calculate_pdr(data)
            if pdr_df is not None and not pdr_df.empty:
                pdr_data.append(pdr_df['pdr'].values)
                labels.append(protocol.replace('_', ' ').title())
        
        if pdr_data:
            ax.boxplot(pdr_data, labels=labels)
            ax.set_ylabel('Packet Delivery Ratio (%)')
            ax.set_title('PDR Comparison Across Protocols')
            ax.grid(True, alpha=0.3)
            ax.set_ylim([0, 105])
            
            # Add horizontal line at 95% (target)
            ax.axhline(y=95, color='r', linestyle='--', alpha=0.5, label='Target (95%)')
            ax.legend()
            
            plt.tight_layout()
            
            output_path = Path(output_dir) / 'pdr_comparison.png'
            output_path.parent.mkdir(parents=True, exist_ok=True)
            plt.savefig(output_path, dpi=300)
            print(f"\n✓ Saved plot: {output_path}")
            
            plt.close()
    
    def plot_cost_evolution(self, output_dir):
        """Plot cost evolution over time (cost routing only)"""
        if 'cost_routing' not in self.data:
            return
        
        cost_data = self.data['cost_routing']
        route_entries = cost_data[cost_data['event_type'] == 'ROUTE_ENTRY']
        
        if route_entries.empty:
            return
        
        # Convert timestamp to datetime
        route_entries['datetime'] = pd.to_datetime(route_entries['timestamp'])
        route_entries['cost_val'] = pd.to_numeric(route_entries['cost'], errors='coerce')
        
        fig, ax = plt.subplots(figsize=(12, 6))
        
        # Plot cost for each source
        for src in route_entries['src_addr'].unique():
            src_data = route_entries[route_entries['src_addr'] == src].sort_values('datetime')
            ax.plot(src_data['datetime'], src_data['cost_val'], marker='o', label=f'Node {src}')
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Route Cost')
        ax.set_title('Cost Evolution Over Time (Cost Routing)')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.xticks(rotation=45)
        plt.tight_layout()
        
        output_path = Path(output_dir) / 'cost_evolution.png'
        plt.savefig(output_path, dpi=300)
        print(f"✓ Saved plot: {output_path}")
        
        plt.close()
    
    def generate_report(self):
        """Generate analysis report"""
        print("\n" + "="*60)
        print("📈 PERFORMANCE ANALYSIS REPORT")
        print("="*60)
        
        # PDR Summary
        pdr_results = self.print_pdr_summary()
        
        # Link Quality Summary
        self.print_link_quality_summary()
        
        # Generate plots
        figures_dir = self.data_dir / 'figures'
        print("\n" + "="*60)
        print("📊 GENERATING PLOTS")
        print("="*60)
        
        self.plot_pdr_comparison(figures_dir)
        self.plot_cost_evolution(figures_dir)
        
        print("\n" + "="*60)
        print("✓ ANALYSIS COMPLETE")
        print("="*60)


def main():
    parser = argparse.ArgumentParser(
        description='xMESH Performance Analysis Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        '--data-dir',
        type=str,
        default='experiments/results',
        help='Directory containing protocol subdirectories (default: experiments/results)'
    )
    
    args = parser.parse_args()
    
    analyzer = xMeshAnalyzer(args.data_dir)
    
    if analyzer.load_all_data():
        analyzer.generate_report()
    else:
        print("\n✗ Analysis failed: No data loaded")
        return 1
    
    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
