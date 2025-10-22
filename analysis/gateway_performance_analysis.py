#!/usr/bin/env python3
"""
Gateway-Only Performance Analysis
Week 3: Compare protocols using gateway monitoring data

Analyzes:
- Packet Delivery Ratio (PDR)
- Latency (inter-packet intervals)
- Memory usage stability
- Queue performance

Generates:
- Performance comparison plots
- Statistical comparison tables
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
import argparse


class GatewayPerformanceAnalyzer:
    """Analyzes gateway monitoring data for protocol comparison"""
    
    def __init__(self, data_dir):
        self.data_dir = Path(data_dir)
        self.protocols = {
            'flooding': 'Flooding',
            'hopcount': 'Hop-Count',
            'cost_routing': 'Cost Routing'
        }
        self.data = {}
        
        # Set plot style
        sns.set_style('whitegrid')
        sns.set_palette('Set2')
        plt.rcParams['figure.figsize'] = (12, 7)
        plt.rcParams['font.size'] = 11
    
    def load_protocol_data(self, protocol):
        """Load all CSV files for a protocol"""
        protocol_dir = self.data_dir / protocol
        
        if not protocol_dir.exists():
            print(f"  ⚠️  Directory not found: {protocol_dir}")
            return None
        
        csv_files = list(protocol_dir.glob('run*_gateway.csv'))
        
        if not csv_files:
            print(f"  ⚠️  No CSV files found in {protocol_dir}")
            return None
        
        print(f"  Loading {len(csv_files)} files for {protocol}...")
        
        dfs = []
        for csv_file in csv_files:
            try:
                df = pd.read_csv(csv_file)
                df['run'] = csv_file.stem.replace('_gateway', '')  # e.g., 'run1'
                df['protocol'] = protocol
                df['timestamp'] = pd.to_datetime(df['timestamp'])
                dfs.append(df)
            except Exception as e:
                print(f"    ✗ Error loading {csv_file}: {e}")
        
        if dfs:
            combined = pd.concat(dfs, ignore_index=True)
            print(f"    ✓ Loaded {len(combined)} events ({len(csv_files)} runs)")
            return combined
        
        return None
    
    def load_all_data(self):
        """Load data for all protocols"""
        print("\n" + "="*60)
        print("📂 LOADING GATEWAY DATA")
        print("="*60 + "\n")
        
        for protocol in self.protocols.keys():
            data = self.load_protocol_data(protocol)
            if data is not None:
                self.data[protocol] = data
        
        if not self.data:
            print("\n✗ No data loaded!")
            return False
        
        print(f"\n✓ Loaded data for {len(self.data)} protocols")
        return True
    
    def calculate_pdr(self):
        """Calculate Packet Delivery Ratio for each protocol"""
        print("\n" + "="*60)
        print("📊 CALCULATING PACKET DELIVERY RATIO (PDR)")
        print("="*60 + "\n")
        
        results = []
        
        for protocol, data in self.data.items():
            # Filter packet events only
            packets = data[data['event_type'] == 'packet'].copy()
            
            # Remove duplicate log lines (gateway logs RX + GATEWAY for each packet)
            packets = packets.drop_duplicates(subset=['packet_seq', 'packet_source', 'run'])
            
            print(f"{self.protocols[protocol]}:")
            
            for run in packets['run'].unique():
                run_data = packets[packets['run'] == run]
                
                for source in run_data['packet_source'].unique():
                    src_data = run_data[run_data['packet_source'] == source]
                    
                    # Get sequence numbers
                    seq_nums = src_data['packet_seq'].dropna().astype(int)
                    
                    if len(seq_nums) == 0:
                        continue
                    
                    # Calculate PDR
                    min_seq = seq_nums.min()
                    max_seq = seq_nums.max()
                    expected = max_seq - min_seq + 1
                    received = len(seq_nums.unique())
                    pdr = (received / expected) * 100 if expected > 0 else 0
                    
                    results.append({
                        'Protocol': self.protocols[protocol],
                        'Run': run,
                        'Source': source,
                        'Expected': expected,
                        'Received': received,
                        'PDR': pdr
                    })
                    
                    print(f"  {run} (Source {source}): {received}/{expected} = {pdr:.1f}%")
        
        return pd.DataFrame(results)
    
    def calculate_latency_stats(self):
        """Calculate inter-packet arrival latency"""
        print("\n" + "="*60)
        print("⏱️  CALCULATING LATENCY STATISTICS")
        print("="*60 + "\n")
        
        results = []
        
        for protocol, data in self.data.items():
            packets = data[data['event_type'] == 'packet'].copy()
            packets = packets.drop_duplicates(subset=['packet_seq', 'packet_source', 'run'])
            packets = packets.sort_values(['run', 'timestamp'])
            
            print(f"{self.protocols[protocol]}:")
            
            for run in packets['run'].unique():
                run_data = packets[packets['run'] == run]
                
                # Calculate inter-packet intervals
                if len(run_data) > 1:
                    run_data['time_diff'] = run_data['timestamp'].diff().dt.total_seconds()
                    intervals = run_data['time_diff'].dropna()
                    
                    if len(intervals) > 0:
                        results.append({
                            'Protocol': self.protocols[protocol],
                            'Run': run,
                            'Mean_Latency': intervals.mean(),
                            'Std_Latency': intervals.std(),
                            'Min_Latency': intervals.min(),
                            'Max_Latency': intervals.max()
                        })
                        
                        print(f"  {run}: Mean={intervals.mean():.1f}s, "
                              f"Std={intervals.std():.1f}s, "
                              f"Range=[{intervals.min():.1f}-{intervals.max():.1f}]s")
        
        return pd.DataFrame(results)
    
    def calculate_memory_stats(self):
        """Analyze memory usage patterns"""
        print("\n" + "="*60)
        print("💾 ANALYZING MEMORY USAGE")
        print("="*60 + "\n")
        
        results = []
        
        for protocol, data in self.data.items():
            monitoring = data[data['event_type'] == 'monitoring'].copy()
            
            print(f"{self.protocols[protocol]}:")
            
            for run in monitoring['run'].unique():
                run_data = monitoring[monitoring['run'] == run]
                
                mem_free = run_data['memory_free_kb'].dropna()
                
                if len(mem_free) > 0:
                    results.append({
                        'Protocol': self.protocols[protocol],
                        'Run': run,
                        'Mean_Free_Memory': mem_free.mean(),
                        'Std_Free_Memory': mem_free.std(),
                        'Min_Free_Memory': mem_free.min(),
                        'Memory_Stability': mem_free.std() / mem_free.mean() * 100  # CV%
                    })
                    
                    print(f"  {run}: Mean={mem_free.mean():.0f} KB, "
                          f"Min={mem_free.min():.0f} KB, "
                          f"Stability={mem_free.std():.1f} KB std")
        
        return pd.DataFrame(results)
    
    def plot_pdr_comparison(self, pdr_data, output_dir):
        """Generate PDR comparison bar chart"""
        print("\n📊 Generating PDR comparison plot...")
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Group by protocol and calculate mean PDR
        pdr_summary = pdr_data.groupby('Protocol')['PDR'].agg(['mean', 'std'])
        
        protocols = pdr_summary.index
        means = pdr_summary['mean']
        stds = pdr_summary['std']
        
        x = np.arange(len(protocols))
        bars = ax.bar(x, means, yerr=stds, capsize=5, alpha=0.7, edgecolor='black')
        
        # Color bars
        colors = ['#8dd3c7', '#ffffb3', '#bebada']
        for bar, color in zip(bars, colors):
            bar.set_color(color)
        
        ax.set_xlabel('Protocol', fontsize=12, fontweight='bold')
        ax.set_ylabel('Packet Delivery Ratio (%)', fontsize=12, fontweight='bold')
        ax.set_title('Protocol PDR Comparison (Gateway Monitoring)', fontsize=14, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(protocols)
        ax.set_ylim([0, 105])
        ax.axhline(y=100, color='green', linestyle='--', alpha=0.5, label='100% PDR')
        ax.grid(axis='y', alpha=0.3)
        ax.legend()
        
        # Add value labels on bars
        for i, (mean, std) in enumerate(zip(means, stds)):
            ax.text(i, mean + std + 2, f'{mean:.1f}%', ha='center', va='bottom', fontweight='bold')
        
        plt.tight_layout()
        output_path = output_dir / 'performance_pdr_comparison.png'
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"  ✓ Saved: {output_path}")
    
    def plot_latency_comparison(self, latency_data, output_dir):
        """Generate latency comparison box plot"""
        print("\n📊 Generating latency comparison plot...")
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Prepare data for box plot
        protocols = latency_data['Protocol'].unique()
        data_to_plot = [latency_data[latency_data['Protocol'] == p]['Mean_Latency'].values 
                        for p in protocols]
        
        bp = ax.boxplot(data_to_plot, labels=protocols, patch_artist=True, 
                        showmeans=True, meanline=True)
        
        # Color boxes
        colors = ['#8dd3c7', '#ffffb3', '#bebada']
        for patch, color in zip(bp['boxes'], colors):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)
        
        ax.set_xlabel('Protocol', fontsize=12, fontweight='bold')
        ax.set_ylabel('Inter-Packet Latency (seconds)', fontsize=12, fontweight='bold')
        ax.set_title('Protocol Latency Comparison (Gateway Monitoring)', fontsize=14, fontweight='bold')
        ax.grid(axis='y', alpha=0.3)
        
        plt.tight_layout()
        output_path = output_dir / 'performance_latency_comparison.png'
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"  ✓ Saved: {output_path}")
    
    def plot_memory_comparison(self, memory_data, output_dir):
        """Generate memory usage comparison"""
        print("\n📊 Generating memory comparison plot...")
        
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Group by protocol
        mem_summary = memory_data.groupby('Protocol')['Mean_Free_Memory'].agg(['mean', 'std'])
        
        protocols = mem_summary.index
        means = mem_summary['mean']
        stds = mem_summary['std']
        
        x = np.arange(len(protocols))
        bars = ax.bar(x, means, yerr=stds, capsize=5, alpha=0.7, edgecolor='black')
        
        colors = ['#8dd3c7', '#ffffb3', '#bebada']
        for bar, color in zip(bars, colors):
            bar.set_color(color)
        
        ax.set_xlabel('Protocol', fontsize=12, fontweight='bold')
        ax.set_ylabel('Free Memory (KB)', fontsize=12, fontweight='bold')
        ax.set_title('Protocol Memory Usage Comparison (Gateway)', fontsize=14, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(protocols)
        ax.grid(axis='y', alpha=0.3)
        
        # Add value labels
        for i, (mean, std) in enumerate(zip(means, stds)):
            ax.text(i, mean + std + 5, f'{mean:.0f} KB', ha='center', va='bottom', fontweight='bold')
        
        plt.tight_layout()
        output_path = output_dir / 'performance_memory_comparison.png'
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"  ✓ Saved: {output_path}")
    
    def generate_summary_table(self, pdr_data, latency_data, memory_data, output_dir):
        """Generate combined summary table"""
        print("\n📋 Generating summary table...")
        
        summary = []
        
        for protocol in self.protocols.values():
            pdr_protocol = pdr_data[pdr_data['Protocol'] == protocol]
            latency_protocol = latency_data[latency_data['Protocol'] == protocol]
            memory_protocol = memory_data[memory_data['Protocol'] == protocol]
            
            summary.append({
                'Protocol': protocol,
                'Mean_PDR': pdr_protocol['PDR'].mean(),
                'Std_PDR': pdr_protocol['PDR'].std(),
                'Mean_Latency': latency_protocol['Mean_Latency'].mean(),
                'Std_Latency': latency_protocol['Std_Latency'].mean(),
                'Mean_Free_Memory': memory_protocol['Mean_Free_Memory'].mean(),
                'Memory_Stability_CV': memory_protocol['Memory_Stability'].mean()
            })
        
        summary_df = pd.DataFrame(summary)
        
        # Save to CSV
        output_path = output_dir / 'performance_summary.csv'
        summary_df.to_csv(output_path, index=False, float_format='%.2f')
        
        print(f"  ✓ Saved: {output_path}")
        
        # Print to console
        print("\n" + "="*60)
        print("📊 PERFORMANCE SUMMARY")
        print("="*60)
        print(summary_df.to_string(index=False))
        print("="*60)
        
        return summary_df
    
    def generate_report(self, output_dir='experiments/results/figures'):
        """Generate complete performance analysis report"""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        print("\n" + "="*60)
        print("🎨 GENERATING PERFORMANCE ANALYSIS")
        print("="*60)
        
        # Calculate metrics
        pdr_data = self.calculate_pdr()
        latency_data = self.calculate_latency_stats()
        memory_data = self.calculate_memory_stats()
        
        # Generate plots
        if not pdr_data.empty:
            self.plot_pdr_comparison(pdr_data, output_dir)
        
        if not latency_data.empty:
            self.plot_latency_comparison(latency_data, output_dir)
        
        if not memory_data.empty:
            self.plot_memory_comparison(memory_data, output_dir)
        
        # Generate summary
        summary = self.generate_summary_table(pdr_data, latency_data, memory_data, output_dir)
        
        print("\n" + "="*60)
        print("✅ PERFORMANCE ANALYSIS COMPLETE")
        print("="*60)
        print(f"\nAll results saved to: {output_dir}")
        
        return summary


def main():
    parser = argparse.ArgumentParser(
        description='Gateway-Only Performance Analysis for xMESH'
    )
    
    parser.add_argument(
        '--data-dir',
        type=str,
        default='experiments/results',
        help='Directory containing protocol subdirectories (default: experiments/results)'
    )
    
    parser.add_argument(
        '--output-dir',
        type=str,
        default='experiments/results/figures',
        help='Directory for output plots and tables (default: experiments/results/figures)'
    )
    
    args = parser.parse_args()
    
    analyzer = GatewayPerformanceAnalyzer(args.data_dir)
    
    if analyzer.load_all_data():
        analyzer.generate_report(args.output_dir)
        return 0
    else:
        print("\n✗ Analysis failed: No data loaded")
        return 1


if __name__ == '__main__':
    import sys
    sys.exit(main())
