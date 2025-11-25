# ESP Sensor Hub Documentation

Streamlined documentation for the ESP multi-device IoT monitoring platform.

## 🚀 Start Here

### **📖 [PLATFORM_GUIDE.md](reference/PLATFORM_GUIDE.md)**  
**Main documentation** - Architecture, features, quick start, and platform overview

### **⚙️ [CONFIG.md](reference/CONFIG.md)**  
**Configuration reference** - Setup details, deployment commands, and troubleshooting

### **📋 [Main README](../README.md)**  
**Project overview** - Quick start and navigation

## 📁 Specialized Documentation

### Core Features
- **[EVENT_LOGGING.md](EVENT_LOGGING.md)** - Device monitoring and event tracking system
- **[CODE_STRUCTURE.md](architecture/CODE_STRUCTURE.md)** - Technical implementation details

### Hardware Projects  
- **[PCB Design](pcb_design/)** - USB-powered temperature sensor board
- **[Solar Monitor](solar-monitor/)** - Victron solar monitoring project

## 🏗️ Streamlined Structure

```
docs/
├── 📖 reference/
│   ├── PLATFORM_GUIDE.md          ← **START HERE** - Main documentation  
│   ├── CONFIG.md                  ← Configuration & troubleshooting
│   └── COPILOT_INSTRUCTIONS.md    ← Development guidelines
├── 📊 EVENT_LOGGING.md            ← Device monitoring system
├── 🔧 architecture/
│   └── CODE_STRUCTURE.md          ← Technical implementation  
├── 🔌 pcb_design/                ← Hardware design files
└── ☀️ solar-monitor/              ← Solar project docs
```

## 📚 Documentation Philosophy

✅ **Three-File Rule**: Core platform information consolidated into PLATFORM_GUIDE.md, CONFIG.md, and main README  
✅ **No Redundancy**: Each piece of information exists in one authoritative location  
✅ **Clear Hierarchy**: README → PLATFORM_GUIDE → CONFIG (general to specific)  
✅ **Specialized Docs**: Technical features and hardware projects kept separate

## Quick Architecture Overview

```
ESP Devices → Raspberry Pi Docker Stack
             ├── InfluxDB (data storage)
             ├── Grafana (dashboards) 
             └── WiFiManager (portal config)
```

**Current Status**:
- ✅ **Temperature Sensors**: 4 devices deployed with DS18B20
- ✅ **Solar Monitor**: ESP32 project for Victron equipment
- ✅ **Self-Hosted**: InfluxDB + Grafana on Raspberry Pi
- ✅ **Portal Config**: WiFiManager eliminates hardcoded credentials

---

**Updated**: November 24, 2025 - Documentation consolidated for maintainability  
**Total Files**: Reduced from 20+ to 8 focused documents
