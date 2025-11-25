# GitHub Copilot Documentation Guidelines

## Critical: Consolidated Documentation Structure

**The project documentation has been consolidated to eliminate redundancy and improve maintainability.**

## ✅ ONLY Update These 3 Files:

### 1. `/docs/reference/PLATFORM_GUIDE.md` - **Primary Documentation**
- **Purpose**: Main platform documentation, architecture, quick start, benefits
- **When to Update**: Architecture changes, new features, platform modifications
- **Content**: Overview, architecture diagrams, project support, deployment basics
- **Target Audience**: New users, architects, decision makers

### 2. `/docs/reference/CONFIG.md` - **Configuration Reference**  
- **Purpose**: Technical configuration details, deployment commands, troubleshooting
- **When to Update**: New deployment options, credential changes, troubleshooting procedures
- **Content**: secrets.h setup, deployment scripts, data queries, detailed troubleshooting
- **Target Audience**: Developers, operators, troubleshooters

### 3. `/README.md` - **Project Entry Point**
- **Purpose**: Project overview, quick start, navigation to detailed docs
- **When to Update**: Project structure changes, new project types, quick start modifications
- **Content**: Project list, system overview, quick start commands, documentation links
- **Target Audience**: Repository visitors, new contributors

## ❌ DO NOT Update These Files:

- **`docs/architecture/overview.md`** - References the consolidated docs but should remain stable
- **`docs/SETUP.md`** - Legacy detailed setup guide, redirect users to CONFIG.md instead
- **`docs/EVENT_LOGGING.md`** - Specific feature documentation, separate from platform docs
- **Any other documentation files** - Changes should go in the 3 primary files above

## Update Strategy by Change Type:

### Architecture Changes
- **Primary**: Update `PLATFORM_GUIDE.md` architecture section and diagrams
- **Secondary**: Update `README.md` system overview if significant
- **Reference**: Update `CONFIG.md` only if deployment procedures change

### New Features  
- **Primary**: Add to `PLATFORM_GUIDE.md` features and benefits sections
- **Secondary**: Update `README.md` if it affects quick start or project list
- **Reference**: Update `CONFIG.md` if new configuration is required

### Configuration Changes
- **Primary**: Update `CONFIG.md` with new setup procedures
- **Secondary**: Update `PLATFORM_GUIDE.md` if it affects architecture
- **Reference**: Update `README.md` quick start if commands change

### Deployment Changes
- **Primary**: Update `CONFIG.md` deployment commands section
- **Secondary**: Update `PLATFORM_GUIDE.md` deployment overview
- **Reference**: Update `README.md` quick start commands

## Documentation Principles:

### ✅ Do This:
- **Single Source of Truth**: Each piece of information should exist in only one place
- **Clear Hierarchy**: README.md → PLATFORM_GUIDE.md → CONFIG.md (general to specific)
- **Cross-Reference**: Link between files but don't duplicate content
- **Update Consistently**: When changing architecture, update all 3 files appropriately

### ❌ Don't Do This:
- **Duplicate Information**: Don't repeat the same content in multiple files
- **Create New Docs**: Don't create additional reference documentation
- **Fragment Updates**: Don't update only one file when changes affect multiple
- **Ignore Hierarchy**: Don't put detailed config in README.md or basic overview in CONFIG.md

## Content Guidelines by File:

### PLATFORM_GUIDE.md Content:
```
✅ Architecture diagrams and explanations
✅ Platform overview and benefits  
✅ Project type support matrix
✅ Basic deployment workflow
✅ High-level troubleshooting
✅ Key features and capabilities
```

### CONFIG.md Content:
```
✅ secrets.h setup examples
✅ Detailed deployment commands
✅ Data query examples
✅ Specific troubleshooting procedures
✅ Configuration file formats
✅ API reference examples
```

### README.md Content:
```
✅ Project list and status
✅ System architecture overview
✅ Quick start commands
✅ Documentation navigation
✅ Hardware requirements
✅ Basic project description
```

## Maintenance Workflow:

1. **Identify Change Type**: Architecture, feature, configuration, or deployment
2. **Select Primary File**: Choose the most appropriate file for the main content
3. **Update Related Files**: Ensure consistency across all 3 files
4. **Verify Links**: Check that cross-references still work
5. **Test User Journey**: Ensure new users can follow README → PLATFORM_GUIDE → CONFIG

## Quality Checklist:

- [ ] Information exists in only one authoritative location
- [ ] Cross-references between files are accurate
- [ ] User can navigate: README → PLATFORM_GUIDE → CONFIG logically  
- [ ] No outdated architecture references (CloudWatch, AWS, etc.)
- [ ] All WiFiManager portal and InfluxDB architecture is current
- [ ] Quick start commands in README match detailed commands in CONFIG.md

---

**Remember**: The goal is **maintainable, non-redundant documentation** that provides a clear user journey from project discovery to detailed configuration. Always consider which of the 3 files is the most appropriate home for new information.

**File Consolidation Completed**: November 24, 2025  
**Previous Files Removed**: COPILOT_INSTRUCTIONS.md, PROJECT_SUMMARY.md, SECRETS_SETUP.md, COMPLETION_SUMMARY.txt  
**Current Structure**: 3-file focused documentation with clear responsibilities
