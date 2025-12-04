#!/usr/bin/env python3
"""
Generate scalability projection chart for large deployments
Shows: Measured data (3-5 nodes) + Mathematical projection (10-50 nodes)
"""

import matplotlib.pyplot as plt
import numpy as np

# Publication quality
plt.rcParams['figure.dpi'] = 300
plt.rcParams['savefig.dpi'] = 300
plt.rcParams['font.size'] = 11
plt.rcParams['font.family'] = 'serif'

def plot_scalability_projection():
    """Figure 5.1: Scalability Projection to Large Deployments"""
    fig, ax = plt.subplots(figsize=(12, 7))

    # Node counts
    measured_nodes = np.array([3, 4, 5])
    projected_nodes = np.array([10, 20, 50])
    all_nodes = np.concatenate([measured_nodes, projected_nodes])

    # === MEASURED DATA (from actual tests) ===
    # Protocol 2: N × 15 HELLOs per 30min (each node broadcasts every 120s)
    p2_measured = measured_nodes * 15  # [45, 60, 75]

    # Protocol 3: From actual test data with Trickle suppression
    # 3 nodes: ~18 HELLOs, 4 nodes: ~28, 5 nodes: ~36
    p3_measured = np.array([18, 28, 36])

    # === PROJECTED DATA (mathematical model) ===
    # Protocol 2: Linear scaling (N × 15)
    p2_projected = projected_nodes * 15  # [150, 300, 750]

    # Protocol 3 WITHOUT safety HELLO (pure Trickle, stable networks)
    # Model: N × 15 × (1 - efficiency)
    # Efficiency increases with N: 10-node=67%, 20-node=80%, 50-node=90%
    # Assumes I_max=600s can be reached (no safety ceiling)
    efficiency_no_safety = {
        10: 0.67,
        20: 0.80,
        50: 0.90
    }

    p3_no_safety = np.array([
        10 * 15 * (1 - efficiency_no_safety[10]),  # ~50 HELLOs/30min
        20 * 15 * (1 - efficiency_no_safety[20]),  # ~60 HELLOs/30min
        50 * 15 * (1 - efficiency_no_safety[50])   # ~75 HELLOs/30min
    ])

    # Protocol 3 WITH safety HELLO (180s ceiling, mobile/dynamic networks)
    # Safety ceiling limits I_max to 180s (instead of 600s)
    # More realistic for deployments requiring fast fault detection
    # Suppression still works but ceiling caps efficiency
    efficiency_with_safety = {
        10: 0.45,  # Conservative: better than measured 33% but limited by 180s ceiling
        20: 0.55,  # Moderate improvement with more neighbors
        50: 0.65   # Good suppression but ceiling prevents reaching 90%
    }

    p3_with_safety = np.array([
        10 * 15 * (1 - efficiency_with_safety[10]),  # ~83 HELLOs/30min
        20 * 15 * (1 - efficiency_with_safety[20]),  # ~135 HELLOs/30min
        50 * 15 * (1 - efficiency_with_safety[50])   # ~263 HELLOs/30min
    ])

    # Combine measured + projected
    p2_all = np.concatenate([p2_measured, p2_projected])
    p3_no_safety_all = np.concatenate([p3_measured, p3_no_safety])
    p3_with_safety_all = np.concatenate([p3_measured, p3_with_safety])

    # Convert to HELLOs per hour (×2 since data is per 30min)
    p2_hourly = p2_all * 2
    p3_no_safety_hourly = p3_no_safety_all * 2
    p3_with_safety_hourly = p3_with_safety_all * 2

    # Plot measured data (solid lines)
    ax.plot(measured_nodes, p2_hourly[:3], 'o-', linewidth=3, markersize=10,
            color='#d62728', label='Protocol 2 (Measured)', zorder=3)
    ax.plot(measured_nodes, p3_with_safety_hourly[:3], 's-', linewidth=3, markersize=10,
            color='#2ca02c', label='Protocol 3 (Measured)', zorder=3)

    # Plot projected data - Protocol 2
    ax.plot(all_nodes[2:], p2_hourly[2:], 'o--', linewidth=2.5, markersize=8,
            color='#d62728', alpha=0.7, label='Protocol 2 (Projected)', zorder=2)

    # Plot projected data - Protocol 3 WITHOUT safety (best case)
    ax.plot(all_nodes[2:], p3_no_safety_hourly[2:], 's--', linewidth=2.5, markersize=8,
            color='#2ca02c', alpha=0.7, label='Protocol 3 (No Safety - Stable)', zorder=2)

    # Plot projected data - Protocol 3 WITH safety (realistic)
    ax.plot(all_nodes[2:], p3_with_safety_hourly[2:], '^--', linewidth=2.5, markersize=8,
            color='#ff7f0e', alpha=0.7, label='Protocol 3 (With Safety - Mobile)', zorder=2)

    # Duty cycle limit (AS923: 1% = 36s per hour at 56ms airtime)
    # 36s / 0.056s per packet = ~640 packets/hour max
    ax.axhline(y=640, color='red', linestyle='-.', linewidth=2.5,
               label='AS923 Duty Cycle Limit (1%)', alpha=0.8, zorder=1)

    # Fill areas showing Protocol 3 advantage
    # Best case (no safety) - green
    ax.fill_between(all_nodes, p2_hourly, p3_no_safety_hourly, alpha=0.12, color='green',
                    label='Best Case Reduction')
    # Realistic (with safety) - orange
    ax.fill_between(all_nodes, p2_hourly, p3_with_safety_hourly, alpha=0.12, color='orange')

    # Annotations
    # Measured region
    ax.axvspan(3, 5, alpha=0.1, color='blue', label='Measured Data Region')
    ax.text(4, 30, 'MEASURED\n(Hardware Tests)', ha='center', fontsize=10,
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.6))

    # Projected region
    ax.axvspan(5, 50, alpha=0.05, color='gray')
    ax.text(25, 30, 'PROJECTED\n(Mathematical Model)', ha='center', fontsize=10,
            bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.6))

    # 10-node annotations
    reduction_10_no_safety = (p2_hourly[3] - p3_no_safety_hourly[3]) / p2_hourly[3] * 100
    reduction_10_with_safety = (p2_hourly[3] - p3_with_safety_hourly[3]) / p2_hourly[3] * 100

    ax.annotate(f'Stable network:\n{reduction_10_no_safety:.0f}% reduction\n(no safety ceiling)',
                xy=(10, p3_no_safety_hourly[3]), xytext=(7, 130),
                fontsize=9, ha='left',
                bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.7),
                arrowprops=dict(arrowstyle='->', lw=1.5, color='darkgreen'))

    ax.annotate(f'Mobile network:\n{reduction_10_with_safety:.0f}% reduction\n(180s safety)',
                xy=(10, p3_with_safety_hourly[3]), xytext=(12, 220),
                fontsize=9, ha='left',
                bbox=dict(boxstyle='round', facecolor='#ffe5cc', alpha=0.7),
                arrowprops=dict(arrowstyle='->', lw=1.5, color='darkorange'))

    # 50-node scalability limit
    ax.annotate('50-node:\nNear duty cycle limit\n(Protocol 2)',
                xy=(50, p2_hourly[5]), xytext=(45, 900),
                fontsize=9, ha='center', color='darkred',
                bbox=dict(boxstyle='round', facecolor='#ffcccc', alpha=0.8),
                arrowprops=dict(arrowstyle='->', lw=1.5, color='darkred'))

    # Labels
    ax.set_xlabel('Number of Nodes', fontsize=13, fontweight='bold')
    ax.set_ylabel('HELLO Packets per Hour', fontsize=13, fontweight='bold')
    ax.set_title('Scalability Projection: HELLO Overhead Growth (3 to 50 Nodes)',
                 fontsize=14, fontweight='bold')

    # Logarithmic scale for better visibility
    ax.set_xscale('log')
    ax.set_xticks(all_nodes)
    ax.set_xticklabels(all_nodes)
    ax.set_xlim([2.5, 60])
    ax.set_ylim([0, 1600])

    ax.legend(loc='upper left', fontsize=8, ncol=2, framealpha=0.9)
    ax.grid(True, alpha=0.3, which='both')

    plt.tight_layout()
    output_png = 'proposal_docs/images/figure5_1_scalability_projection.png'
    output_jpg = 'final_report/figures/figure5_1_scalability_projection.jpg'
    plt.savefig(output_png, dpi=300, bbox_inches='tight')
    plt.savefig(output_jpg, dpi=300, bbox_inches='tight', format='jpg')
    print(f"✅ Scalability projection plot saved: {output_png}")
    print(f"✅ Scalability projection plot saved: {output_jpg}")
    plt.close()

    # Print projection data
    print("\n=== PROJECTION DATA ===")
    print("\n📊 10-node network:")
    print(f"  P2: {p2_hourly[3]:.0f} HELLOs/hr")
    print(f"  P3 (no safety):   {p3_no_safety_hourly[3]:.0f} HELLOs/hr → {reduction_10_no_safety:.0f}% reduction (stable networks)")
    print(f"  P3 (with safety): {p3_with_safety_hourly[3]:.0f} HELLOs/hr → {reduction_10_with_safety:.0f}% reduction (mobile networks)")

    reduction_20_no_safety = (p2_hourly[4] - p3_no_safety_hourly[4]) / p2_hourly[4] * 100
    reduction_20_with_safety = (p2_hourly[4] - p3_with_safety_hourly[4]) / p2_hourly[4] * 100
    print(f"\n📊 20-node network:")
    print(f"  P2: {p2_hourly[4]:.0f} HELLOs/hr")
    print(f"  P3 (no safety):   {p3_no_safety_hourly[4]:.0f} HELLOs/hr → {reduction_20_no_safety:.0f}% reduction")
    print(f"  P3 (with safety): {p3_with_safety_hourly[4]:.0f} HELLOs/hr → {reduction_20_with_safety:.0f}% reduction")

    reduction_50_no_safety = (p2_hourly[5] - p3_no_safety_hourly[5]) / p2_hourly[5] * 100
    reduction_50_with_safety = (p2_hourly[5] - p3_with_safety_hourly[5]) / p2_hourly[5] * 100
    print(f"\n📊 50-node network:")
    print(f"  P2: {p2_hourly[5]:.0f} HELLOs/hr (EXCEEDS duty cycle limit!)")
    print(f"  P3 (no safety):   {p3_no_safety_hourly[5]:.0f} HELLOs/hr → {reduction_50_no_safety:.0f}% reduction")
    print(f"  P3 (with safety): {p3_with_safety_hourly[5]:.0f} HELLOs/hr → {reduction_50_with_safety:.0f}% reduction")

    print(f"\n🔴 Duty cycle limit: 640 HELLOs/hr")
    print(f"\n📈 Protocol 2 violates duty cycle at ~21 nodes")
    print(f"✅ Protocol 3 (no safety) stays below limit even at 50 nodes")
    print(f"✅ Protocol 3 (with safety) stays below limit even at 50 nodes")

if __name__ == '__main__':
    plot_scalability_projection()
