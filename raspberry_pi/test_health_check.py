#!/usr/bin/env python3
"""
Test Health Check - Detects Abnormal Network Behavior
Analyzes test logs to identify issues like excessive resets, route timeouts, etc.
"""

import sys
from pathlib import Path
from collections import defaultdict

class TestHealthAnalyzer:
    def __init__(self, log_files):
        self.log_files = log_files
        self.issues = []
        self.warnings = []
        self.metrics = defaultdict(int)

    def analyze(self):
        """Run all health checks"""
        for log_file in self.log_files:
            self._analyze_file(log_file)

        return self._print_report()

    def _analyze_file(self, log_file):
        """Analyze single log file"""
        with open(log_file, 'r') as f:
            lines = f.readlines()

        # Count events
        counts = {
            'trickle_resets': 0,
            'topology_changes': 0,
            'route_timeouts': 0,
            'trickle_hellos': 0,
            'suppressions': 0,
            'gap_detections': 0,
            'safety_hellos': 0
        }

        for line in lines:
            if "[Trickle] RESET" in line or "[TRICKLE] Topology change" in line:
                counts['trickle_resets'] += 1
            if "[TOPOLOGY]" in line and "size changed" in line:
                counts['topology_changes'] += 1
            if "Route timeout" in line:
                counts['route_timeouts'] += 1
            if "[TrickleHELLO] Sending HELLO" in line:
                counts['trickle_hellos'] += 1
            if "[Trickle] SUPPRESS" in line:
                counts['suppressions'] += 1
            if "GAP DETECTED" in line:
                counts['gap_detections'] += 1
            if "SAFETY HELLO" in line:
                counts['safety_hellos'] += 1

        node_name = log_file.stem

        # Check for abnormalities
        if counts['trickle_resets'] > 10:
            self.issues.append(f"{node_name}: Excessive resets ({counts['trickle_resets']}) - Network unstable")

        if counts['route_timeouts'] > 0:
            self.issues.append(f"{node_name}: Route timeouts ({counts['route_timeouts']}) - Safety HELLO not working")

        if counts['topology_changes'] > 5:
            self.warnings.append(f"{node_name}: Frequent topology changes ({counts['topology_changes']})")

        if counts['suppressions'] == 0 and counts['trickle_hellos'] > 3:
            self.warnings.append(f"{node_name}: No suppressions - Feature may not be active")

        # Accumulate metrics
        for key, value in counts.items():
            self.metrics[key] += value

        # Calculate rates
        if counts['suppressions'] + counts['trickle_hellos'] > 0:
            rate = (counts['suppressions'] / (counts['suppressions'] + counts['trickle_hellos'])) * 100
            print(f"{node_name}: {counts['trickle_hellos']} HELLOs, {counts['suppressions']} suppressed ({rate:.0f}%)")

    def _print_report(self):
        """Print final health report"""
        print("\n" + "="*60)

        if not self.issues and not self.warnings:
            print("✅ ALL CHECKS PASSED")
        else:
            if self.issues:
                print("🚨 ISSUES:")
                for issue in self.issues:
                    print(f"  {issue}")

            if self.warnings:
                print("⚠️  WARNINGS:")
                for warning in self.warnings:
                    print(f"  {warning}")

        print("="*60)
        return 0 if not self.issues else 1

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_health_check.py <test_folder>")
        sys.exit(1)

    test_folder = Path(sys.argv[1])
    if not test_folder.exists():
        print(f"Error: Folder not found: {test_folder}")
        sys.exit(1)

    log_files = sorted(test_folder.glob("node*.log"))
    if not log_files:
        print(f"Error: No node*.log files in {test_folder}")
        sys.exit(1)

    analyzer = TestHealthAnalyzer(log_files)
    sys.exit(analyzer.analyze())

if __name__ == "__main__":
    main()
