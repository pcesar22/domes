# AI Development Recommendations

## Maximizing Claude Code Effectiveness for DOMES

This document outlines recommendations for optimizing the project architecture based on best practices from Boris Cherny (Claude Code creator) and Anthropic's engineering guidelines.

---

## Executive Summary

| Category | Current State | Recommended Action |
|----------|---------------|-------------------|
| CLAUDE.md files | ⚠️ Partial | Strengthen root file |
| Slash commands | ❌ Missing | Create .claude/commands/ |
| MCP servers | ❌ Missing | Configure .mcp.json |
| GitHub Action | ❌ Missing | Install @.claude |
| Permissions | ❌ Missing | Pre-allow safe commands |
| Hooks | ❌ Missing | Add code formatting hook |
| Verification loop | ⚠️ Defined only | Automate with commands |

---

## 1. CLAUDE.md Structure

### Current State

| File | Lines | Quality |
|------|-------|---------|
| `/CLAUDE.md` | 45 | Generic boilerplate |
| `/firmware/CLAUDE.md` | 205 | Excellent |
| `/hardware/CLAUDE.md` | 245 | Excellent |

### Recommendation: Rewrite Root CLAUDE.md

The root CLAUDE.md should be project-specific guardrails, not generic documentation. Replace with:

```markdown
# DOMES Project

Distributed Open-source Motion & Exercise System - Reaction training pods.

## Quick Reference
- **Firmware:** ESP-IDF v5.x, C++20, FreeRTOS → see `firmware/CLAUDE.md`
- **Hardware:** JLCPCB sourcing, KiCad → see `hardware/CLAUDE.md`
- **Specs:** `research/*.md` (architecture, roadmap, requirements)

## CRITICAL RULES

### Firmware Development
- NEVER use Arduino framework - ESP-IDF only
- NEVER use std::vector/string/map - use ETL equivalents
- NEVER allocate heap after init phase
- ALWAYS check esp_err_t returns
- ALWAYS create abstract interfaces for driver testability
- ALWAYS use ESP_LOGx macros, never iostream

### Hardware Development
- ALWAYS verify LCSC part numbers before updating BOM
- ALWAYS check stock levels (>100 for prototype, >1000 for production)
- PREFER JLCPCB Basic Parts over Extended Parts
- NEVER substitute packages without noting the change

## Verification Commands
- Build firmware: `cd firmware && idf.py build`
- Flash to DevKit: `cd firmware && idf.py -p /dev/ttyUSB0 flash monitor`
- Run unit tests: `cd firmware && idf.py --preview set-target linux && idf.py build && ./build/test_app`

## Common Mistakes to Avoid
<!-- Add entries here as you encounter them -->
1. [Example] Using `std::string` instead of `etl::string<N>`
2. [Example] Forgetting IRAM_ATTR on ISR functions

## Reference Documents
- `research/SOFTWARE_ARCHITECTURE.md` - Firmware design spec
- `research/SYSTEM_ARCHITECTURE.md` - Hardware architecture
- `research/DEVELOPMENT_ROADMAP.md` - Milestones and dependencies
- `research/ID_REQUIREMENTS.md` - Industrial design constraints
```

### Best Practice: Evolve CLAUDE.md Over Time

> "Anytime we see Claude do something incorrectly we add it to the CLAUDE.md, so Claude knows not to do it next time." — Boris Cherny

Add mistakes and corrections as they occur. The CLAUDE.md should be a living document.

---

## 2. Slash Commands

### Why Slash Commands Matter

> "I use slash commands for every 'inner loop' workflow that I end up doing many times a day. This saves me from repeated prompting." — Boris Cherny

### Recommended Directory Structure

```
.claude/
└── commands/
    ├── build-firmware.md
    ├── flash.md
    ├── verify-firmware.md
    ├── commit-push-pr.md
    ├── source-part.md
    └── create-driver.md
```

### Command Definitions

#### `/build-firmware`
```markdown
Build the ESP32-S3 firmware and report results:

$```bash
cd /home/user/domes/firmware && idf.py build 2>&1
$```

If build fails, identify the errors with file:line references and suggest fixes.
If build succeeds, report the binary size and confirm it's under 4MB.
```

#### `/flash`
```markdown
Flash firmware to the connected DevKit and start monitoring:

$```bash
cd /home/user/domes/firmware && idf.py -p /dev/ttyUSB0 flash monitor
$```

Watch for boot errors, WiFi/BLE initialization, and any crash loops.
```

#### `/verify-firmware`
```markdown
Run the complete firmware verification checklist:

$```bash
cd /home/user/domes/firmware
echo "=== Building firmware ==="
idf.py build 2>&1
BUILD_RESULT=$?

if [ $BUILD_RESULT -eq 0 ]; then
    echo ""
    echo "=== Checking binary size ==="
    SIZE=$(stat -c%s build/domes.bin 2>/dev/null || echo 0)
    echo "Binary size: $SIZE bytes"
    if [ $SIZE -lt 4194304 ]; then
        echo "✅ Size OK (under 4MB OTA limit)"
    else
        echo "❌ Binary too large for OTA partition"
    fi
else
    echo "❌ Build failed"
fi
$```

Report the verification results clearly.
```

#### `/commit-push-pr`
```markdown
Prepare and create a pull request:

$```bash
git status
git diff --stat HEAD
git log --oneline -5
$```

Based on the changes:
1. Stage all relevant files
2. Create a commit with a descriptive message following conventional commits
3. Push to the current branch
4. Create a PR with a summary of changes and test plan
```

#### `/source-part`
```markdown
Search for a component in JLCPCB/LCSC catalog.

Provide:
- Component description or part number to search for

I will:
1. Search the JLCPCB parts catalog
2. Verify the part meets design requirements
3. Check stock levels and pricing
4. Determine if it's a Basic or Extended part
5. Update hardware/BOM.csv with the sourcing information
```

#### `/create-driver`
```markdown
Create a new hardware driver following DOMES conventions.

Provide:
- Driver name (e.g., "Haptic", "Led", "Audio")
- Hardware interface (I2C, SPI, GPIO, etc.)
- Driver IC (e.g., DRV2605L, MAX98357A)

I will create:
1. `interfaces/i{Name}Driver.hpp` - Abstract interface
2. `drivers/{name}Driver.hpp` - Implementation header
3. `drivers/{name}Driver.cpp` - Implementation
4. `test/mocks/mock{Name}Driver.hpp` - Mock for testing

All following the patterns in firmware/CLAUDE.md and firmware/GUIDELINES.md.
```

---

## 3. MCP Server Configuration

### Why MCP Servers Matter

> "Claude Code uses all my tools for me. It often searches and posts to Slack (via the MCP server), runs BigQuery queries, grabs error logs from Sentry, etc." — Boris Cherny

### Recommended Configuration

Create `.mcp.json` in the project root:

```json
{
  "mcpServers": {
    "jlcpcb": {
      "command": "npx",
      "args": ["-y", "@anthropic/mcp-jlcpcb"],
      "description": "JLCPCB/LCSC parts catalog for hardware BOM sourcing"
    },
    "filesystem": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-filesystem", "/home/user/domes"],
      "description": "File system access for project files"
    }
  }
}
```

### Hardware-Specific MCP Usage

The `hardware/CLAUDE.md` already defines the EE agent role for JLCPCB sourcing. With MCP configured, Claude can:

1. Search JLCPCB parts catalog directly
2. Verify stock levels in real-time
3. Compare pricing across alternatives
4. Check if parts are Basic or Extended

---

## 4. GitHub Action Integration

### Installation

Run in Claude Code:
```
/install-github-action
```

### Usage

Once installed, you can:
- Tag `@.claude` in PR comments to request changes
- Have Claude automatically update CLAUDE.md as part of PRs
- Automate code review and suggestions

### Example PR Comment

```
@.claude Please add a note to firmware/CLAUDE.md about the DMA buffer
alignment requirement we discovered in this PR.
```

---

## 5. Permission Pre-allowances

### Why Pre-allow Permissions

> "I use /permissions to pre-allow common bash commands that I know are safe in my environment, to avoid unnecessary permission prompts." — Boris Cherny

### Recommended Configuration

Create `.claude/settings.json`:

```json
{
  "permissions": {
    "allow": [
      "Bash(idf.py:*)",
      "Bash(git:*)",
      "Bash(ls:*)",
      "Bash(cat:*)",
      "Bash(head:*)",
      "Bash(tail:*)",
      "Bash(find:*)",
      "Bash(grep:*)",
      "Bash(make:*)",
      "Bash(cmake:*)",
      "Bash(python:*)",
      "Bash(pip:*)"
    ],
    "deny": [
      "Bash(rm:-rf /*)",
      "Bash(sudo:*)"
    ]
  }
}
```

---

## 6. Code Formatting Hooks

### Why Hooks Matter

> "We use a PostToolUse hook to format Claude's code. Claude usually generates well-formatted code out of the box, and the hook handles the last 10%." — Boris Cherny

### Recommended Hook Configuration

Add to `.claude/settings.json`:

```json
{
  "hooks": {
    "PostToolUse": [
      {
        "matcher": {
          "tool": "Edit",
          "glob": "firmware/**/*.{cpp,hpp,c,h}"
        },
        "command": "clang-format -i $FILE"
      },
      {
        "matcher": {
          "tool": "Edit",
          "glob": "**/*.py"
        },
        "command": "black $FILE"
      },
      {
        "matcher": {
          "tool": "Edit",
          "glob": "**/*.md"
        },
        "command": "prettier --write $FILE"
      }
    ]
  }
}
```

### Prerequisites

Ensure formatting tools are installed:
```bash
# C/C++ formatting
sudo apt install clang-format

# Python formatting
pip install black

# Markdown formatting
npm install -g prettier
```

---

## 7. Verification Feedback Loop

### The Most Important Recommendation

> "Probably the most important thing to get great results out of Claude Code — give Claude a way to verify its work. If Claude has that feedback loop, it will 2-3x the quality of the final result." — Boris Cherny

### Current State

The DEVELOPMENT_ROADMAP.md defines excellent milestones with clear pass criteria:

| Milestone | Pass Criteria |
|-----------|---------------|
| M1: Target Compiles | `idf.py build` exits 0, binary < 4MB |
| M2: DevKit Boots | `app_main` log within 5s, no crash loops |
| M4: RF Validated | WiFi + BLE coexist, no errors for 5 min |
| M5: Unit Tests | All pass, coverage > 70% |

**Problem:** These are documented but not automated.

### Recommendation: Automated Verification Commands

1. **Create `/verify-firmware`** command (see Section 2)

2. **Add to root CLAUDE.md:**
```markdown
## Verification Requirements

ALWAYS run verification after making changes:
- After firmware changes: `/verify-firmware`
- After adding a driver: Run unit tests
- After changing BOM: Verify LCSC stock levels

Never consider a task complete until verification passes.
```

3. **Create milestone-specific verification:**

```markdown
# .claude/commands/verify-m1.md
Verify Milestone 1 (Target Compiles) criteria:

$```bash
cd /home/user/domes/firmware
echo "=== M1: Target Compiles Verification ==="

echo "Checking ESP-IDF version..."
idf.py --version

echo "Building firmware..."
idf.py set-target esp32s3
idf.py build

echo "Checking binary size..."
SIZE=$(stat -c%s build/domes.bin 2>/dev/null || echo 0)
echo "Binary size: $SIZE bytes (limit: 4194304)"
$```

Report pass/fail for each M1 criterion.
```

---

## 8. Subagents (Optional)

### When to Use Subagents

> "I use a few subagents regularly: code-simplifier simplifies the code after Claude is done working, verify-app has detailed instructions for testing." — Boris Cherny

### Recommended Subagents for DOMES

#### Code Simplifier
```markdown
# .claude/agents/code-simplifier.md

Review the recently changed code and simplify it:

1. Remove unnecessary complexity
2. Consolidate duplicate code
3. Improve naming for clarity
4. Ensure consistency with firmware/GUIDELINES.md
5. Remove dead code

Do not change functionality, only improve code quality.
```

#### Hardware Reviewer
```markdown
# .claude/agents/hardware-reviewer.md

Review hardware changes against design requirements:

1. Verify BOM entries have valid LCSC part numbers
2. Check that specifications match SYSTEM_ARCHITECTURE.md
3. Confirm stock levels are sufficient
4. Flag any Extended Parts that could be Basic Parts
5. Verify package/footprint compatibility
```

---

## 9. Parallel Claude Workflows

### Running Multiple Claudes

> "I run 5 Claudes in parallel in my terminal... I also run 5-10 Claudes on claude.ai/code, in parallel with my local Claudes." — Boris Cherny

### Recommended Parallel Workflows for DOMES

| Claude Instance | Task |
|-----------------|------|
| Terminal 1 | Firmware driver development |
| Terminal 2 | Unit test writing |
| Terminal 3 | Documentation updates |
| claude.ai/code | Long-running BOM sourcing |
| claude.ai/code | Architecture research |

### Session Handoff

Use `&` to hand off sessions between terminal and web:
```
/compact
& "Continue this session on claude.ai/code"
```

Use `--teleport` to transfer sessions back and forth.

---

## 10. Plan Mode Workflow

### When to Use Plan Mode

> "Most sessions start in Plan mode (shift+tab twice). If my goal is to write a Pull Request, I will use Plan mode, and go back and forth with Claude until I like its plan." — Boris Cherny

### Recommended Workflow

1. **Start in Plan mode** for any multi-file change
2. **Iterate on the plan** until it aligns with architecture docs
3. **Switch to auto-accept** for implementation
4. **Verify** with `/verify-firmware`

### Example Planning Session

```
User: I need to implement the haptic driver

[Plan mode]
Claude: Based on firmware/CLAUDE.md and SYSTEM_ARCHITECTURE.md, here's my plan:

1. Create interface: interfaces/iHapticDriver.hpp
   - Virtual methods: init(), playEffect(uint8_t), setIntensity(uint8_t)

2. Create driver: drivers/hapticDriver.hpp/cpp
   - DRV2605L over I2C
   - Use I2C address 0x5A
   - Support LRA mode

3. Create mock: test/mocks/mockHapticDriver.hpp

4. Verify: idf.py build passes

Does this plan look correct?
```

---

## Implementation Checklist

### Phase 1: Foundation (High Priority)

- [ ] Rewrite root CLAUDE.md with project-specific rules
- [ ] Create `.claude/commands/` directory
- [ ] Add `/build-firmware` command
- [ ] Add `/verify-firmware` command
- [ ] Add `/commit-push-pr` command
- [ ] Install GitHub Action (`/install-github-action`)

### Phase 2: Tooling (Medium Priority)

- [ ] Create `.claude/settings.json` with permissions
- [ ] Configure permission pre-allowances
- [ ] Add PostToolUse hook for clang-format
- [ ] Create `.mcp.json` for JLCPCB MCP
- [ ] Add `/source-part` command

### Phase 3: Refinement (Ongoing)

- [ ] Add mistakes to CLAUDE.md as encountered
- [ ] Create subagents for repetitive workflows
- [ ] Add milestone verification commands
- [ ] Document parallel workflow patterns

---

## Quick Reference: File Structure

```
domes/
├── .claude/
│   ├── commands/
│   │   ├── build-firmware.md
│   │   ├── flash.md
│   │   ├── verify-firmware.md
│   │   ├── commit-push-pr.md
│   │   ├── source-part.md
│   │   └── create-driver.md
│   ├── agents/
│   │   ├── code-simplifier.md
│   │   └── hardware-reviewer.md
│   └── settings.json
├── .mcp.json
├── CLAUDE.md                    # Project-specific (rewritten)
├── firmware/
│   └── CLAUDE.md               # Firmware guidelines (keep)
├── hardware/
│   └── CLAUDE.md               # Hardware guidelines (keep)
└── research/
    └── AI_DEVELOPMENT_RECOMMENDATIONS.md  # This file
```

---

## References

- [Claude Code Best Practices](https://www.anthropic.com/engineering/claude-code-best-practices) - Anthropic Engineering
- [Using CLAUDE.md Files](https://claude.com/blog/using-claude-md-files) - Anthropic Blog
- [Claude Code Slash Commands](https://code.claude.com/docs/en/slash-commands) - Documentation
- [Claude Code Hooks](https://code.claude.com/docs/en/hooks-guide) - Documentation
- [Claude Code Sub-agents](https://code.claude.com/docs/en/sub-agents) - Documentation

---

*Document Created: 2026-01-04*
*Project: DOMES*
*Based on: Boris Cherny's Claude Code workflow recommendations*
