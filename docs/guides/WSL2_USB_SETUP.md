# USB Device Setup for WSL2

This guide explains how to make USB devices (ESP8266, ESP32) accessible from WSL2.

## Overview

WSL2 runs in a Hyper-V virtual machine, so USB devices aren't automatically accessible. You need to use **USBIPD-WIN** to bridge USB devices from Windows to WSL2.

## Prerequisites

- Windows 10 Build 19041 or later (Windows 11 recommended)
- WSL2 installed and running
- Administrator access to PowerShell

## One-Time Setup

### Step 1: Install USBIPD-WIN

**Option A: Using Winget (Recommended)**
```powershell
winget install usbipd
```

**Option B: Using Chocolatey**
```powershell
choco install usbipd-win
```

**Option C: Manual Installation**
Download from: https://github.com/dorssel/usbipd-win/releases

### Step 2: Verify Installation

```powershell
usbipd --version
```

You should see a version number.

## Attaching a Device

### Method 1: Using the PowerShell Script (Recommended)

1. **Connect your ESP device to USB**

2. **Run the script as Administrator**:
   ```powershell
   & 'C:\path\to\scripts\attach-usb-wsl2.ps1'
   ```

   Or with a specific BUSID:
   ```powershell
   & 'C:\path\to\scripts\attach-usb-wsl2.ps1' -BusId "2-1"
   ```

3. Follow the prompts

### Method 2: Manual Commands

1. **List all USB devices** (from PowerShell as Administrator):
   ```powershell
   usbipd list
   ```

   Output example:
   ```
   BUSID    VID:PID    DEVICE
   2-1      1a86:7523  USB Serial Device (CH340)
   3-2      1366:0105  SEGGER J-Link
   ```

2. **Attach device to WSL2**:
   ```powershell
   usbipd attach --wsl --busid 2-1
   ```

3. **Verify in WSL2**:
   ```bash
   ls -la /dev/ttyUSB* /dev/ttyACM*
   ```

## Flashing Your Device

Once the USB device is attached, you can flash your ESP device:

```bash
cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor

# Flash ESP8266
scripts/flash_device.sh "Big Garage" esp8266

# Flash ESP32
scripts/flash_device.sh "Big Garage" esp32
```

## Detaching a Device

When you're done, detach the device (optional but frees it for Windows use):

```powershell
usbipd detach --busid 2-1
```

## Troubleshooting

### Device Not Found in WSL2

**Check Windows side**:
```powershell
usbipd list
```

**Check WSL2 side**:
```bash
lsusb
ls -la /dev/ttyUSB*
dmesg | tail -20
```

### "Access Denied" Error

- Ensure you're running PowerShell as **Administrator**
- Try detaching and re-attaching the device

### Device Disconnects After WSL2 Restart

- Simply re-attach using: `usbipd attach --wsl --busid <BUSID>`

### Driver Issues (CH340 Serial Chip)

If using a CH340 chip and get permission errors:

```bash
sudo chmod 666 /dev/ttyUSB0
```

## Automating the Setup

### Option 1: Create a Windows Shortcut

Create `attach-esp.cmd` in your project:

```batch
@echo off
powershell -Command "& 'C:\your\path\to\scripts\attach-usb-wsl2.ps1'"
pause
```

Then right-click → Run as administrator

### Option 2: Add to Windows Task Scheduler

Schedule the script to run automatically when a USB device is connected.

## Useful Aliases (Optional)

Add to your `~/.bashrc` in WSL2:

```bash
alias check-usb='ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "No USB devices"'
alias flash-garage='cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor && scripts/flash_device.sh "Big Garage" esp8266'
```

Then you can just run:
```bash
check-usb
flash-garage
```

## Quick Reference

| Task | Command |
|------|---------|
| List USB devices (Windows) | `usbipd list` |
| Attach device to WSL2 | `usbipd attach --wsl --busid 2-1` |
| Detach device | `usbipd detach --busid 2-1` |
| Check USB in WSL2 | `ls /dev/ttyUSB*` |
| Flash device | `scripts/flash_device.sh "Name" esp8266` |

## References

- [USBIPD-WIN GitHub](https://github.com/dorssel/usbipd-win)
- [Microsoft WSL USB Support Docs](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
