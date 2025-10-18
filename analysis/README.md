# xMESH Week 6-7 Performance Analysis

This notebook analyzes the performance of three routing protocols:
1. **Flooding Baseline** (v0.2.0-alpha)
2. **Hop-Count Routing** (v0.3.1-alpha) 
3. **Gateway-Aware Cost Routing** (v0.4.0-alpha)

## Setup

### Required Dependencies

```bash
pip install pandas numpy matplotlib seaborn scipy jupyter
```

### Data Structure

Expected CSV format from `data_collection.py`:
```
timestamp,event_type,seq_num,src_addr,dst_addr,hops,value,rssi,snr,cost,raw_line
```

### File Organization

```
experiments/
  results/
    flooding/
      run1.csv
      run2.csv
      run3.csv
    hopcount/
      run1.csv
      run2.csv
      run3.csv
    cost_routing/
      run1.csv
      run2.csv
      run3.csv
    processed/
      combined_data.csv
      summary_stats.csv
    figures/
      pdr_comparison.png
      latency_cdf.png
      overhead_bar.png
```

## Analysis Steps

1. **Load Data** - Read CSV files from all test runs
2. **Clean Data** - Remove duplicates, handle missing values
3. **Calculate Metrics** - PDR, latency, overhead
4. **Statistical Tests** - ANOVA, t-tests for significance
5. **Visualization** - Create comparison plots
6. **Interpretation** - Document findings

## Key Metrics

- **PDR (Packet Delivery Ratio):** Packets received / Packets sent
- **Latency:** Time from TX to RX (if timestamps available)
- **Overhead:** Total transmissions / Delivered packets
- **Route Stability:** Frequency of route changes

## Usage

1. Run data collection for all protocols
2. Place CSV files in appropriate directories
3. Run this notebook cells sequentially
4. Review generated plots and statistics
5. Document findings in markdown cells
