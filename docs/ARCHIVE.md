# Documentation Archive

## Memory Bank Context Files

The `memory-bank/` directory contained AI context templates and project tracking files used during development for Copilot/Claude interactions:

- `activeContext.md` - Active context snapshots
- `architect.md` - Architecture decision records
- `decisionLog.md` - Development decisions
- `productContext.md` - Product/project overview
- `progress.md` - Progress tracking
- `projectBrief.md` - Project brief
- `systemPatterns.md` - Design patterns

**Status**: These files have been removed from the main documentation tree as they are internal development artifacts, not user-facing documentation. If needed for reference, they can be recovered from git history:

```bash
git log --follow -- memory-bank/
git show <commit>:memory-bank/filename.md
```

## Consolidated Documentation

The following documents were consolidated into project READMEs to reduce duplication:

### Surveillance Camera
- ✅ `DEPLOYMENT_GUIDE.md` → Consolidated into `surveillance/README.md`
- ✅ `PERFORMANCE_OPTIMIZATIONS.md` → Consolidated into `surveillance/README.md`
- ✅ `MODERN_UI_CHANGELOG.md` → Removed (UI redesign completed)
- ✅ `MODERN_UI_COMPLETION.md` → Removed (project status documented)
- ✅ `MODERN_UI_REFERENCE.md` → Removed (API reference in main README)

### Scripts Documentation
- ✅ Updated `scripts/README.md` to document all three projects (temperature, solar, surveillance)
- ✅ Removed surveillance-only focus
- ✅ Added cross-project build system overview

## Current Documentation Structure

```
docs/
├── README.md                           # Documentation index
├── reference/
│   ├── PLATFORM_GUIDE.md              # Architecture & platform overview
│   ├── CONFIG.md                      # Configuration reference
│   └── COPILOT_INSTRUCTIONS.md        # Development guidelines
├── architecture/
│   └── CODE_STRUCTURE.md              # Technical implementation
├── hardware/
│   └── OLED_DISPLAY_GUIDE.md          # Optional OLED integration
├── pcb_design/                        # Hardware design docs
└── solar-monitor/                     # Solar-specific documentation
```

## Key Documentation Locations

| Topic | Location |
|-------|----------|
| Project Overview | [README.md](../README.md) |
| Architecture & Design | [docs/reference/PLATFORM_GUIDE.md](reference/PLATFORM_GUIDE.md) |
| Configuration & Troubleshooting | [docs/reference/CONFIG.md](reference/CONFIG.md) |
| Development Guidelines | [docs/reference/COPILOT_INSTRUCTIONS.md](reference/COPILOT_INSTRUCTIONS.md) |
| Code Structure | [docs/architecture/CODE_STRUCTURE.md](architecture/CODE_STRUCTURE.md) |
| OLED Display Setup | [docs/hardware/OLED_DISPLAY_GUIDE.md](hardware/OLED_DISPLAY_GUIDE.md) |
| Temperature Sensor | [README.md](../README.md) (section) |
| Solar Monitor | [solar-monitor/README.md](../solar-monitor/README.md) |
| Surveillance Camera | [surveillance/README.md](../surveillance/README.md) |
| Build & Flash Scripts | [scripts/README.md](../scripts/README.md) |

## Philosophy

- **Single Source of Truth**: Each piece of information exists in one authoritative location
- **No Redundancy**: Deployment guides, performance tuning, and reference docs consolidated into project READMEs
- **Clear Hierarchy**: Main README → PLATFORM_GUIDE → CONFIG (general to specific)
- **Specialized Docs**: Hardware projects and technical details kept organized by function
- **Audit Trail**: Git history preserves all previous documentation for reference

---

**Last Updated**: December 20, 2025
