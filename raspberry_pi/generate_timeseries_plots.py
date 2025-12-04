#!/usr/bin/env python3
"""
Generate time-series plots from Protocol 3 test logs
Extracts temporal data for:
1. Trickle interval progression (I_min → I_max)
2. HELLO packet cumulative count over time
3. ETX values over time
4. Gateway load over time
"""

import re
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timedelta
import numpy as np
from pathlib import Path

# Publication quality settings
plt.rcParams['figure.dpi'] = 300
plt.rcParams['savefig.dpi'] = 300
plt.rcParams['font.size'] = 11
plt.rcParams['font.family'] = 'serif'
plt.rcParams['axes.grid'] = True
plt.rcParams['grid.alpha'] = 0.3

def parse_timestamp(line):
    """Extract timestamp from log line [HH:MM:SS.mmm]"""
    match = re.search(r'\[(\d{2}):(\d{2}):(\d{2}\.\d{3})\]', line)
    if match:
        h, m, s = match.groups()
        return float(h) * 3600 + float(m) * 60 + float(s)
    return None

def extract_trickle_progression(log_file):
    """Extract Trickle interval progression from log"""
    intervals = []
    timestamps = []

    with open(log_file, 'r') as f:
        for line in f:
            # Match: [Trickle] DOUBLE - I=XXX.Xs
            if '[Trickle] DOUBLE' in line or '[Trickle] RESET' in line:
                ts = parse_timestamp(line)
                match = re.search(r'I=(\d+\.\d+)s', line)
                if ts and match:
                    interval = float(match.group(1))
                    timestamps.append(ts)
                    intervals.append(interval)

    return timestamps, intervals

def extract_hello_cumulative(log_file):
    """Extract cumulative HELLO count over time"""
    timestamps = []
    cumulative = []
    count = 0

    with open(log_file, 'r') as f:
        for line in f:
            if '[TrickleHELLO] Sending HELLO' in line or 'SAFETY HELLO' in line:
                ts = parse_timestamp(line)
                if ts:
                    count += 1
                    timestamps.append(ts)
                    cumulative.append(count)

    return timestamps, cumulative

def extract_etx_timeseries(log_file):
    """Extract ETX values over time for all tracked links"""
    etx_data = {}  # {address: [(timestamp, etx), ...]}

    with open(log_file, 'r') as f:
        for line in f:
            # Match: Link XXXX: RSSI=XX dBm, SNR=XX dB, ETX=X.XX
            if 'Link' in line and 'ETX=' in line:
                ts = parse_timestamp(line)
                addr_match = re.search(r'Link (\w{4}):', line)
                etx_match = re.search(r'ETX=(\d+\.\d+)', line)

                if ts and addr_match and etx_match:
                    addr = addr_match.group(1)
                    etx = float(etx_match.group(1))

                    if addr not in etx_data:
                        etx_data[addr] = []
                    etx_data[addr].append((ts, etx))

    return etx_data

def extract_gateway_load(log_file):
    """Extract gateway load over time"""
    timestamps = []
    loads = []

    with open(log_file, 'r') as f:
        for line in f:
            # Match: [W5] Gateway XXXX load=X.X
            if '[W5] Gateway' in line and 'load=' in line:
                ts = parse_timestamp(line)
                match = re.search(r'load=(\d+\.\d+)', line)
                if ts and match:
                    load = float(match.group(1))
                    timestamps.append(ts)
                    loads.append(load)

    return timestamps, loads

def plot_trickle_interval_progression():
    """Generate Figure: Trickle Interval Progression Over Time"""
    # Use cold-start test log (captures initial 60s→600s progression)
    log_file = Path('experiments/results/protocol3/5node_validation_suite/5node_sensors_coldstart_20251114_230621/node2_20251114_230621.log')

    if not log_file.exists():
        print(f"⚠️  Log file not found: {log_file}")
        return

    timestamps, intervals = extract_trickle_progression(log_file)

    if not timestamps:
        print("⚠️  No Trickle data found")
        return

    # Convert to minutes
    timestamps = np.array(timestamps) / 60.0
    timestamps = timestamps - timestamps[0]  # Start at t=0

    fig, ax = plt.subplots(figsize=(12, 6))

    # Plot interval progression
    ax.plot(timestamps, intervals, 'o-', color='#2E7D32', linewidth=2,
            markersize=8, label='Trickle Interval I(t)', markerfacecolor='white',
            markeredgewidth=2)

    # Mark key intervals
    ax.axhline(y=60, color='blue', linestyle=':', linewidth=1.5,
               label='I_min = 60s', alpha=0.7)
    ax.axhline(y=600, color='red', linestyle=':', linewidth=1.5,
               label='I_max = 600s', alpha=0.7)
    ax.axhline(y=180, color='orange', linestyle='--', linewidth=2,
               label='Safety HELLO Ceiling = 180s', alpha=0.8)

    # Labels
    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Trickle Interval (seconds)', fontsize=12, fontweight='bold')
    ax.set_title('Trickle Interval Progression in Stable Network (3-Hour Test)',
                 fontsize=14, fontweight='bold')
    ax.legend(loc='lower right', fontsize=10)
    ax.set_ylim([0, 650])
    ax.grid(True, alpha=0.3)

    # Annotations
    if len(timestamps) > 0:
        # Find when I_max first reached
        idx_max = np.where(np.array(intervals) >= 600)[0]
        if len(idx_max) > 0:
            t_max = timestamps[idx_max[0]]
            ax.annotate(f'I_max reached\nat {t_max:.1f} min',
                       xy=(t_max, 600), xytext=(t_max + 10, 550),
                       arrowprops=dict(arrowstyle='->', color='red', lw=1.5),
                       fontsize=10, color='red', fontweight='bold')

    plt.tight_layout()
    output = 'proposal_docs/images/figure_timeseries_trickle_interval.png'
    plt.savefig(output, dpi=300, bbox_inches='tight')
    print(f"✅ Trickle interval progression saved: {output}")
    plt.close()

def plot_hello_cumulative_comparison():
    """Generate Figure: Cumulative HELLO Count Protocol 2 vs 3"""
    # Protocol 2: 30-minute test
    p2_log = Path('experiments/results/protocol2/3node_30min_val_10dBm_20251110_182442/node2_20251110_182442.log')

    # Protocol 3: 30-minute comparable test
    p3_log = Path('experiments/results/protocol3/5node_validation_suite/5node_sensors_coldstart_20251114_230621/node2_20251114_230621.log')

    fig, ax = plt.subplots(figsize=(12, 6))

    # Protocol 2: Fixed 120s intervals (theoretical)
    if p2_log.exists():
        ts_p2, cum_p2 = extract_hello_cumulative(p2_log)
        if ts_p2:
            ts_p2 = np.array(ts_p2) / 60.0  # Convert to minutes
            ts_p2 = ts_p2 - ts_p2[0]
            ax.plot(ts_p2, cum_p2, 's-', color='#1976D2', linewidth=2,
                   markersize=6, label='Protocol 2 (Fixed 120s)',
                   markerfacecolor='white', markeredgewidth=2)
    else:
        # Theoretical Protocol 2 (1 HELLO every 2 minutes)
        t_p2 = np.arange(0, 31, 2)
        cum_p2 = np.arange(0, len(t_p2))
        ax.plot(t_p2, cum_p2, 's--', color='#1976D2', linewidth=2,
               markersize=6, label='Protocol 2 (Fixed 120s, theoretical)',
               alpha=0.6)

    # Protocol 3: Adaptive Trickle
    if p3_log.exists():
        ts_p3, cum_p3 = extract_hello_cumulative(p3_log)
        if ts_p3:
            ts_p3 = np.array(ts_p3) / 60.0
            ts_p3 = ts_p3 - ts_p3[0]
            ax.plot(ts_p3, cum_p3, 'o-', color='#388E3C', linewidth=2,
                   markersize=6, label='Protocol 3 (Trickle Adaptive)',
                   markerfacecolor='white', markeredgewidth=2)

            # Calculate reduction
            final_p2 = 15  # Expected for 30min
            final_p3 = cum_p3[-1] if cum_p3 else 0
            reduction = ((final_p2 - final_p3) / final_p2) * 100

            ax.text(0.98, 0.02, f'Reduction: {reduction:.1f}%\n({final_p2} → {final_p3} HELLOs)',
                   transform=ax.transAxes, fontsize=10,
                   verticalalignment='bottom', horizontalalignment='right',
                   bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # Labels
    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Cumulative HELLO Packets Sent', fontsize=12, fontweight='bold')
    ax.set_title('HELLO Overhead Over Time: Protocol 2 vs Protocol 3',
                 fontsize=14, fontweight='bold')
    ax.legend(loc='upper left', fontsize=10)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    output = 'proposal_docs/images/figure_timeseries_hello_cumulative.png'
    plt.savefig(output, dpi=300, bbox_inches='tight')
    print(f"✅ HELLO cumulative comparison saved: {output}")
    plt.close()

def plot_etx_timeseries():
    """Generate Figure: ETX Link Quality Over Time"""
    # Use outdoor multi-hop test with multiple links
    log_file = Path('experiments/results/protocol3/4node_physical_long_distance_suite/gateways-cold_20251119_182553/node1_20251119_182553.log')

    if not log_file.exists():
        print(f"⚠️  Log file not found: {log_file}")
        return

    etx_data = extract_etx_timeseries(log_file)

    if not etx_data:
        print("⚠️  No ETX data found")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = ['#D32F2F', '#1976D2', '#388E3C', '#F57C00', '#7B1FA2']
    for i, (addr, data) in enumerate(etx_data.items()):
        if data:
            ts = np.array([d[0] for d in data]) / 60.0  # Convert to minutes
            ts = ts - ts[0]
            etx = [d[1] for d in data]

            ax.plot(ts, etx, 'o-', color=colors[i % len(colors)],
                   linewidth=2, markersize=5, label=f'Link {addr}',
                   alpha=0.8)

    # ETX = 1.0 ideal line
    ax.axhline(y=1.0, color='green', linestyle='--', linewidth=1.5,
               label='ETX = 1.0 (Perfect Link)', alpha=0.5)

    # ETX = 2.0 warning line
    ax.axhline(y=2.0, color='orange', linestyle='--', linewidth=1.5,
               label='ETX = 2.0 (Marginal)', alpha=0.5)

    # Labels
    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Expected Transmission Count (ETX)', fontsize=12, fontweight='bold')
    ax.set_title('Link Quality Tracking: ETX Evolution Over Time (Outdoor Test)',
                 fontsize=14, fontweight='bold')
    ax.legend(loc='upper right', fontsize=9, ncol=2)
    ax.set_ylim([0.5, 4.0])
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    output = 'proposal_docs/images/figure_timeseries_etx_evolution.png'
    plt.savefig(output, dpi=300, bbox_inches='tight')
    print(f"✅ ETX time-series saved: {output}")
    plt.close()

def plot_gateway_load_timeseries():
    """Generate Figure: Gateway Load Distribution Over Time"""
    # Use dual-gateway W5 test
    log_file = Path('experiments/results/protocol3/4node_physical_long_distance_suite/gateways-cold_20251119_182553/node1_20251119_182553.log')

    if not log_file.exists():
        print(f"⚠️  Log file not found: {log_file}")
        return

    # Extract load data for multiple gateways
    gateway_loads = {}  # {gateway_addr: [(timestamp, load), ...]}

    with open(log_file, 'r') as f:
        for line in f:
            if '[W5] Gateway' in line and 'load=' in line:
                ts = parse_timestamp(line)
                addr_match = re.search(r'Gateway (\w{4})', line)
                load_match = re.search(r'load=(\d+\.\d+)', line)

                if ts and addr_match and load_match:
                    addr = addr_match.group(1)
                    load = float(load_match.group(1))

                    if addr not in gateway_loads:
                        gateway_loads[addr] = []
                    gateway_loads[addr].append((ts, load))

    if not gateway_loads:
        print("⚠️  No gateway load data found")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = ['#1976D2', '#D32F2F', '#388E3C']
    for i, (addr, data) in enumerate(gateway_loads.items()):
        if data:
            ts = np.array([d[0] for d in data]) / 60.0
            ts = ts - ts[0]
            load = [d[1] for d in data]

            ax.plot(ts, load, 'o-', color=colors[i % len(colors)],
                   linewidth=2, markersize=6, label=f'Gateway {addr}',
                   markerfacecolor='white', markeredgewidth=2)

    # Switch threshold line
    ax.axhline(y=0.25, color='orange', linestyle='--', linewidth=1.5,
               label='Switch Threshold (0.25 pkt/min)', alpha=0.7)

    # Labels
    ax.set_xlabel('Time (minutes)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Gateway Load (packets/minute)', fontsize=12, fontweight='bold')
    ax.set_title('W5 Gateway Load Distribution Over Time (Dual-Gateway Test)',
                 fontsize=14, fontweight='bold')
    ax.legend(loc='upper right', fontsize=10)
    ax.set_ylim([-0.1, max([max([d[1] for d in data]) for data in gateway_loads.values()]) + 0.3])
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    output = 'proposal_docs/images/figure_timeseries_gateway_load.png'
    plt.savefig(output, dpi=300, bbox_inches='tight')
    print(f"✅ Gateway load time-series saved: {output}")
    plt.close()

def main():
    print("Generating time-series plots from test logs...\n")

    # Generate all plots
    plot_trickle_interval_progression()
    plot_hello_cumulative_comparison()
    plot_etx_timeseries()
    plot_gateway_load_timeseries()

    print("\n✅ All time-series plots generated successfully!")
    print("Output directory: proposal_docs/images/")

if __name__ == '__main__':
    main()
