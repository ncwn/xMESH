# Upstream Tracking Guide - LoRaMesher Synchronization

## Overview

This document describes how to track and integrate changes from the upstream LoRaMesher repository while maintaining our custom implementations. Our approach keeps the fork clean and makes it easy to contribute improvements back to the community.

## Repository Structure

```
Current Setup:
- Origin: https://github.com/ncwn/xMESH.git (your fork)
- Upstream: https://github.com/LoRaMesher/LoRaMesher.git (original)
- Branch: main (primary development and stable releases)
- Feature branches for specific development tasks
```

## Initial Setup

### 1. Configure Git Remotes

```bash
# Verify current remotes
git remote -v

# Add upstream if not present
git remote add upstream https://github.com/LoRaMesher/LoRaMesher.git

# Verify upstream added
git remote -v
# Should show:
# origin    https://github.com/ncwn/xMESH.git (fetch)
# origin    https://github.com/ncwn/xMESH.git (push)
# upstream  https://github.com/LoRaMesher/LoRaMesher.git (fetch)
# upstream  https://github.com/LoRaMesher/LoRaMesher.git (push)
```

### 2. Fetch Upstream

```bash
# Fetch all branches from upstream
git fetch upstream

# View upstream branches
git branch -r | grep upstream
```

## Synchronization Workflow

### Weekly Sync Process

Create a sync script `sync_upstream.sh`:

```bash
#!/bin/bash
# sync_upstream.sh - Weekly upstream synchronization

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Starting upstream sync process...${NC}"

# Save current branch
CURRENT_BRANCH=$(git branch --show-current)

# Ensure we're on main branch
git checkout main

# Fetch latest upstream
echo -e "${GREEN}Fetching upstream changes...${NC}"
git fetch upstream

# Check if there are changes
CHANGES=$(git log HEAD..upstream/main --oneline)

if [ -z "$CHANGES" ]; then
    echo -e "${GREEN}No upstream changes detected${NC}"
    git checkout $CURRENT_BRANCH
    exit 0
fi

echo -e "${YELLOW}Found upstream changes:${NC}"
echo "$CHANGES"

# Create sync branch
SYNC_BRANCH="sync-upstream-$(date +%Y%m%d)"
git checkout -b $SYNC_BRANCH

# Merge upstream
echo -e "${GREEN}Merging upstream/main...${NC}"
git merge upstream/main

# Check for conflicts
if [ $? -ne 0 ]; then
    echo -e "${RED}Merge conflicts detected!${NC}"
    echo "Resolve conflicts manually, then run:"
    echo "  git add ."
    echo "  git commit"
    echo "  git push origin $SYNC_BRANCH"
    exit 1
fi

# Run tests
echo -e "${GREEN}Running tests...${NC}"
./run_tests.sh

if [ $? -ne 0 ]; then
    echo -e "${RED}Tests failed after merge!${NC}"
    echo "Fix issues before pushing"
    exit 1
fi

# Push sync branch
git push origin $SYNC_BRANCH

echo -e "${GREEN}Sync branch created: $SYNC_BRANCH${NC}"
echo "Create a pull request to merge into main"

# Return to original branch
git checkout $CURRENT_BRANCH
```

### Conflict Resolution

When conflicts occur, follow this process:

1. **Identify Conflicts**
   ```bash
   git status
   # Shows conflicted files in red
   ```

2. **Review Each Conflict**
   ```bash
   # For each conflicted file
   git diff --name-only --diff-filter=U | while read file; do
       echo "Reviewing: $file"
       git diff $file
   done
   ```

3. **Resolution Strategy**
   - **Our Changes Priority**: Keep our modifications for custom features
   - **Upstream Bug Fixes**: Accept upstream bug fixes
   - **API Changes**: Carefully merge, update our code if needed

4. **Mark Resolved**
   ```bash
   # After editing conflicts
   git add <resolved-file>
   git commit -m "Merge upstream: Resolve conflicts in <files>"
   ```

## Change Log Management

### CHANGELOG.md Template

```markdown
# Changelog - xMESH LoRaMesher Fork

## [Unreleased]

### Added (Our Contributions)
- Trickle scheduler for adaptive HELLO intervals
- ETX link quality tracking with sequence-aware detection
- Multi-metric cost calculation for routing
- Gateway bias in route selection
- Duty cycle monitoring and enforcement

### Changed (Our Modifications)
- Modified RoutingTableService to support cost metrics
- Extended RouteNode structure with link quality fields
- Added callbacks for transmit success/failure tracking

## [2024-11-06] - Upstream Sync

### Upstream Version: v0.0.11

### Changes from LoRaMesher:
- [d37dee7] Update version to 0.0.11
- [ae9d886] Merge PR #85: Optimize timeout calculation
- [0674219] Reset timeout when receiving data from device
- [7903068] Fix lock list bug

### Integration Notes:
- ✅ No conflicts with our cost routing implementation
- ✅ Timeout optimization improves our ETX tracking
- ⚠️ Lock list fix may affect our Trickle timer - needs testing

### Testing Status:
- [x] Protocol 1 (Flooding) - Tested, working
- [x] Protocol 2 (Hop-count) - Tested, working
- [ ] Protocol 3 (Gateway routing) - Testing pending

### Migration Required:
None - Changes are backward compatible

## [2024-10-15] - Previous Sync
...

### Local Changes to LoRaMesher Core (Nov 13, 2025)

| File | Purpose | Notes |
|------|---------|-------|
| `src/entities/packets/RoutePacket.h` | Added `uint8_t gatewayLoad` | Encodes packets/min (0–254) or 255 for unknown. |
| `src/entities/routingTable/NetworkNode.h` | Added `gatewayLoad` member | Lets routing tables propagate each gateway’s load. |
| `src/entities/routingTable/RouteNode.h` | Copy ctor copies `gatewayLoad` | Prevents loss when cloning entries. |
| `src/services/PacketService.cpp` | Initializes HELLO `gatewayLoad` to 255 | Avoids garbage when sender lacks data. |
| `src/services/RoutingTableService.cpp` | Copies/updates `gatewayLoad`, snapshots gateway candidates before cost evaluation | Enables downstream nodes to learn load data and prevents mutex issues when Protocol 3 evaluates W5 bias. |

**When syncing with upstream:**
1. `git diff upstream/main -- src/entities src/services` to see if LoRaMesher touched these files.  
2. Reapply the W5 changes as needed (design docs: `W5_LOAD_SHARING_DESIGN.md`, `BUGFIX_W5_SERIALIZATION.md`).  
3. Rebuild (`pio run`) and rerun the dual-gateway test (`w5_gateway_indoor_*`) to confirm serialization still works.  
4. Plan to upstream these patches once the hallway multi-hop validation is complete.
```

### Tracking Spreadsheet

Maintain a spreadsheet (`upstream_tracking.xlsx`) with:

| Date | Upstream Commit | Description | Impact | Integrated | Tested | Notes |
|------|----------------|-------------|---------|------------|---------|-------|
| 2024-11-06 | d37dee7 | v0.0.11 | None | ✅ | ✅ | Version bump only |
| 2024-11-06 | 0674219 | Timeout optimization | Positive | ✅ | ⚠️ | Improves ETX tracking |
| 2024-11-06 | 7903068 | Lock list fix | Unknown | ✅ | ❌ | Need to test with Trickle |

## Testing After Sync

### Automated Test Suite

```bash
#!/bin/bash
# run_tests.sh - Test all protocols after upstream sync

echo "Running post-sync validation tests..."

# Compile all firmware variants
echo "1. Compiling firmware..."
for dir in firmware/*/; do
    echo "  Building: $dir"
    cd "$dir"
    pio run --silent
    if [ $? -ne 0 ]; then
        echo "  ❌ Build failed: $dir"
        exit 1
    fi
    echo "  ✅ Build successful"
    cd ../..
done

# Run unit tests
echo "2. Running unit tests..."
cd firmware/3_gateway_routing
pio test
if [ $? -ne 0 ]; then
    echo "  ❌ Unit tests failed"
    exit 1
fi
echo "  ✅ Unit tests passed"
cd ../..

# Check for breaking API changes
echo "3. Checking API compatibility..."
python scripts/check_api_compatibility.py
if [ $? -ne 0 ]; then
    echo "  ❌ API compatibility issues found"
    exit 1
fi
echo "  ✅ API compatible"

echo "✅ All tests passed!"
```

### API Compatibility Checker

```python
# scripts/check_api_compatibility.py
import re
import sys
from pathlib import Path

def check_api_compatibility():
    """
    Verify our code still works with upstream API
    """
    issues = []

    # Check for removed/renamed functions
    critical_functions = [
        'LoraMesher::getInstance',
        'LoraMesher::begin',
        'LoraMesher::sendReliable',
        'RoutingTableService::processNetworkPacket',
        'RouteNode::getNextHop'
    ]

    lib_path = Path('src')

    for func in critical_functions:
        found = False
        for file in lib_path.rglob('*.cpp'):
            content = file.read_text()
            if func in content:
                found = True
                break

        if not found:
            issues.append(f"Missing function: {func}")

    # Check for changed signatures
    # ... additional checks ...

    if issues:
        print("API Compatibility Issues:")
        for issue in issues:
            print(f"  - {issue}")
        return False

    print("✅ No API compatibility issues found")
    return True

if __name__ == "__main__":
    if not check_api_compatibility():
        sys.exit(1)
```

## Contribution Back to Upstream

### Identifying Contributable Features

Features suitable for upstream contribution:

1. **Trickle Scheduler** - Generic, RFC-based implementation
2. **ETX Tracking** - Useful link quality metric
3. **Transmit Callbacks** - Success/failure notifications
4. **Duty Cycle Monitoring** - Regulatory compliance

Features specific to our research (keep separate):

1. **Gateway-Aware Cost Routing** - Research-specific
2. **Multi-metric weights** - Experimental parameters
3. **Test configurations** - Project-specific

### Preparing a Pull Request

```bash
# Create feature branch from upstream/main
git checkout -b feature/trickle-scheduler upstream/main

# Cherry-pick our commits
git cherry-pick <commit-hash>

# Clean up commit history
git rebase -i upstream/main

# Update documentation
echo "Update README with feature description"

# Create PR-ready branch
git push origin feature/trickle-scheduler
```

### Pull Request Template

```markdown
## Description
Add Trickle timer implementation for adaptive HELLO scheduling based on RFC 6206.

## Motivation
Reduces control overhead by 97% in stable networks while maintaining route quality.

## Changes
- Add `TrickleTimer` class to utilities/
- Integrate with `RoutingTableService`
- Add configuration parameters
- Update examples with Trickle usage

## Testing
- Tested on ESP32-S3 with SX1262
- 3-6 node networks for 60+ hours
- Maintains 95%+ PDR with reduced overhead

## Backwards Compatibility
- Fully backward compatible
- Trickle disabled by default
- Enable with `config.enableTrickle = true`

## Documentation
- Added Trickle example
- Updated API documentation
- Included configuration guide
```

## Monitoring Upstream Activity

### GitHub Watch Settings

1. Watch upstream repository for:
   - Releases only (minimum)
   - Issues and PRs (recommended)
   - All activity (if actively contributing)

2. Set up notifications:
   ```bash
   # GitHub CLI
   gh repo set-default LoRaMesher/LoRaMesher
   gh watch
   ```

### RSS Feed Monitoring

```python
# scripts/monitor_upstream.py
import feedparser
import json
from datetime import datetime

def check_upstream_updates():
    """
    Check for new releases and commits
    """
    # Releases RSS
    releases_feed = feedparser.parse(
        "https://github.com/LoRaMesher/LoRaMesher/releases.atom"
    )

    # Commits RSS
    commits_feed = feedparser.parse(
        "https://github.com/LoRaMesher/LoRaMesher/commits/main.atom"
    )

    updates = {
        'timestamp': datetime.now().isoformat(),
        'releases': [],
        'recent_commits': []
    }

    # Parse releases
    for entry in releases_feed.entries[:5]:
        updates['releases'].append({
            'title': entry.title,
            'date': entry.published,
            'link': entry.link
        })

    # Parse commits
    for entry in commits_feed.entries[:10]:
        updates['recent_commits'].append({
            'title': entry.title,
            'author': entry.author,
            'date': entry.published,
            'link': entry.link
        })

    # Save to file
    with open('upstream_status.json', 'w') as f:
        json.dump(updates, f, indent=2)

    # Check if action needed
    if updates['releases']:
        print(f"⚠️ New release available: {updates['releases'][0]['title']}")
        return True

    print("✅ No new releases")
    return False

if __name__ == "__main__":
    check_upstream_updates()
```

### Automated Weekly Report

```bash
#!/bin/bash
# weekly_upstream_report.sh

echo "Weekly Upstream Report - $(date +%Y-%m-%d)"
echo "========================================"

# Fetch latest
git fetch upstream --quiet

# Show commits in last week
echo -e "\n📝 Upstream commits (last 7 days):"
git log upstream/main --since="7 days ago" --oneline

# Check for divergence
echo -e "\n📊 Fork divergence:"
AHEAD=$(git rev-list --count upstream/main..main)
BEHIND=$(git rev-list --count main..upstream/main)
echo "  We are $AHEAD commits ahead, $BEHIND commits behind"

# Check for issues mentioning our features
echo -e "\n🔍 Relevant upstream issues:"
gh issue list --repo LoRaMesher/LoRaMesher --search "trickle OR etx OR cost"

# Our unmerged contributions
echo -e "\n🎯 Our pending PRs:"
gh pr list --repo LoRaMesher/LoRaMesher --author ncwn

# Generate action items
echo -e "\n⚡ Action Items:"
if [ $BEHIND -gt 0 ]; then
    echo "  - [ ] Sync with upstream (behind by $BEHIND commits)"
fi
if [ $AHEAD -gt 10 ]; then
    echo "  - [ ] Consider upstream contribution (ahead by $AHEAD commits)"
fi
```

## Best Practices

### DO's
- ✅ Sync weekly to stay current
- ✅ Test thoroughly after each sync
- ✅ Document all integration issues
- ✅ Keep our changes modular and separate
- ✅ Contribute generic improvements upstream
- ✅ Maintain compatibility with upstream API
- ✅ Use feature flags for experimental code

### DON'Ts
- ❌ Modify core LoRaMesher files unnecessarily
- ❌ Break backward compatibility
- ❌ Sync during critical experiments
- ❌ Force push to main branch
- ❌ Ignore upstream bug fixes
- ❌ Submit research-specific code upstream
- ❌ Delay syncing for more than 2 weeks

## Rollback Procedures

### Emergency Rollback

If upstream changes break our implementation:

```bash
#!/bin/bash
# emergency_rollback.sh

# Save current state
git stash
git branch backup-$(date +%Y%m%d-%H%M%S)

# Find last known good commit
LAST_GOOD=$(git log --grep="All tests passed" --format="%H" -1)

if [ -z "$LAST_GOOD" ]; then
    echo "❌ Cannot find last good commit"
    exit 1
fi

echo "Rolling back to: $LAST_GOOD"
git checkout main
git reset --hard $LAST_GOOD

# Force push (requires confirmation)
read -p "Force push to origin? (y/N): " confirm
if [ "$confirm" == "y" ]; then
    git push --force-with-lease origin main
fi

echo "✅ Rollback complete"
echo "Investigate issues in backup branch"
```

### Selective Revert

For reverting specific upstream commits:

```bash
# Identify problematic commit
git bisect start
git bisect bad HEAD
git bisect good <last-known-good>

# Test each commit
git bisect run ./run_tests.sh

# Revert problematic commit
git revert <bad-commit>
```

## Communication

### Internal Documentation

After each sync, update:

1. `CHANGELOG.md` with integration notes
2. `upstream_status.json` with current state
3. Slack/Discord notification to team
4. GitHub issue if problems found

### Upstream Communication

When to communicate with upstream:

1. **Bug Reports**: Found bug in LoRaMesher
2. **Feature Requests**: Need new capability
3. **Contributions**: Submitting improvements
4. **Questions**: API clarification needed

Template for upstream issues:

```markdown
## Environment
- LoRaMesher Version: 0.0.11
- Hardware: ESP32-S3 + SX1262
- PlatformIO: 6.x
- Our Fork: https://github.com/ncwn/xMESH

## Description
[Clear description of issue/request]

## Steps to Reproduce (for bugs)
1. [Step 1]
2. [Step 2]

## Expected Behavior
[What should happen]

## Actual Behavior
[What actually happens]

## Possible Solution
[If you have suggestions]

## Additional Context
Working on research project implementing gateway-aware routing.
Happy to contribute fix if confirmed.
```

## Archive and History

### Maintaining Sync History

```bash
# Create sync archive
mkdir -p .sync_history/$(date +%Y%m)

# Document each sync
cat > .sync_history/$(date +%Y%m)/sync_$(date +%Y%m%d).md << EOF
# Sync Report - $(date +%Y-%m-%d)

## Upstream Changes
$(git log HEAD..upstream/main --oneline)

## Integration Status
- Conflicts: None/Resolved
- Tests: Pass/Fail
- Issues: None/Listed

## Notes
[Any special considerations]
EOF
```

Remember: **Always test on hardware after syncing with upstream!**
