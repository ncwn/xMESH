#!/usr/bin/env python3
"""
Extract plot data from ANALYSIS.md files for thesis figures
"""

import re
import json
from pathlib import Path

def extract_pdr(content):
    """Extract PDR percentage from ANALYSIS content"""
    patterns = [
        r'PDR[:\s]+(\d+\.?\d*)%',
        r'PDR.*?(\d+\.?\d*)\s*%',
        r'delivery ratio.*?(\d+\.?\d*)%',
    ]
    for pattern in patterns:
        matches = re.findall(pattern, content, re.IGNORECASE)
        if matches:
            return float(matches[0])
    return None

def extract_node_count(filepath):
    """Extract node count from folder name"""
    path_str = str(filepath)
    # Look for patterns like "3node", "5node", etc.
    match = re.search(r'(\d+)node', path_str)
    if match:
        return int(match.group(1))
    # Look in content
    return None

def extract_hello_count(content):
    """Extract HELLO packet count"""
    patterns = [
        r'(\d+)\s+HELLO',
        r'HELLO.*?(\d+)\s+packets',
        r'(\d+).*?HELLOs',
    ]
    for pattern in patterns:
        matches = re.findall(pattern, content)
        if matches:
            return [int(m) for m in matches]
    return []

def main():
    base_path = Path('experiments/results')

    data = {
        'protocol1': [],
        'protocol2': [],
        'protocol3': []
    }

    # Process each protocol
    for protocol in ['protocol1', 'protocol2', 'protocol3']:
        protocol_path = base_path / protocol
        if not protocol_path.exists():
            print(f"⚠️  {protocol} directory not found")
            continue

        # Find all ANALYSIS.md files
        for analysis_file in protocol_path.rglob('ANALYSIS.md'):
            print(f"Processing: {analysis_file}")

            with open(analysis_file, 'r') as f:
                content = f.read()

            # Extract metrics
            pdr = extract_pdr(content)
            nodes = extract_node_count(analysis_file)
            hello_counts = extract_hello_count(content)

            test_data = {
                'file': str(analysis_file),
                'pdr': pdr,
                'nodes': nodes,
                'hello_counts': hello_counts
            }

            data[protocol].append(test_data)
            print(f"  PDR: {pdr}%, Nodes: {nodes}, HELLOs: {hello_counts}")

    # Save extracted data
    output_file = 'raspberry_pi/plot_data.json'
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"\n✅ Data extracted to {output_file}")

    # Print summary
    print("\n=== SUMMARY ===")
    for protocol in data:
        print(f"{protocol}: {len(data[protocol])} tests")

if __name__ == '__main__':
    main()
