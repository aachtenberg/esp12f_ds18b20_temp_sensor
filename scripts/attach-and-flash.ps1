# PowerShell Script to Attach USB and Flash ESP Device
# Run this as Administrator from Windows

param(
    [Parameter(Mandatory=$false)]
    [string]$ProjectType = "temp",
    
    [Parameter(Mandatory=$false)]
    [string]$Board = "",
    
    [Parameter(Mandatory=$false)]
    [string]$DeviceName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$BusId = ""
)

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] 'Administrator')
if (-not $isAdmin) {
    Write-Host "`n❌ This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell and select 'Run as Administrator'`n" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Check if usbipd is installed
$usbipd = Get-Command usbipd -ErrorAction SilentlyContinue
if (-not $usbipd) {
    Write-Host "`n❌ USBIPD-WIN is not installed!" -ForegroundColor Red
    Write-Host "Installing USBIPD-WIN..." -ForegroundColor Yellow
    winget install usbipd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Installation failed. Please install manually:" -ForegroundColor Red
        Write-Host "   winget install usbipd`n" -ForegroundColor Yellow
        Read-Host "Press Enter to exit"
        exit 1
    }
    Write-Host "✅ USBIPD-WIN installed successfully!`n" -ForegroundColor Green
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "   ESP Device Flash Tool (WSL2)" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Step 1: List USB devices
Write-Host "[Step 1/4] Listing USB devices..." -ForegroundColor Yellow
Write-Host ""
$devices = usbipd list
Write-Host $devices
Write-Host ""

# Get BusId if not provided
if ([string]::IsNullOrEmpty($BusId)) {
    $BusId = Read-Host "Enter the BUSID of your ESP device (e.g., 1-4)"
    if ([string]::IsNullOrEmpty($BusId)) {
        Write-Host "❌ No BUSID provided. Exiting.`n" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
}

# Step 2: Bind (share) the device first
Write-Host "`n[Step 2/4] Binding device $BusId..." -ForegroundColor Yellow
usbipd bind --busid $BusId

if ($LASTEXITCODE -ne 0) {
    Write-Host "⚠️  Device may already be bound (this is OK)" -ForegroundColor Yellow
}

# Step 3: Attach USB device to WSL2
Write-Host "`n[Step 3/4] Attaching device $BusId to WSL2..." -ForegroundColor Yellow
usbipd attach --wsl --busid $BusId

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Failed to attach device. Please check the BUSID and try again.`n" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "✅ Device attached successfully!`n" -ForegroundColor Green
Start-Sleep -Seconds 2

# Step 4: Get device details if not provided
if ([string]::IsNullOrEmpty($DeviceName)) {
    Write-Host "`n[Step 4/5] Select project and board to flash:" -ForegroundColor Yellow
    Write-Host "  1. Temperature Sensor (ESP8266)" -ForegroundColor Cyan
    Write-Host "  2. Temperature Sensor (ESP32)" -ForegroundColor Cyan
    Write-Host "  3. Solar Monitor (ESP32)" -ForegroundColor Cyan
    Write-Host "  4. Custom configuration" -ForegroundColor Cyan
    Write-Host ""
    
    $choice = Read-Host "Enter choice (1-4)"
    
    switch ($choice) {
        "1" { $ProjectType = "temp"; $Board = "esp8266" }
        "2" { $ProjectType = "temp"; $Board = "esp32" }
        "3" { $ProjectType = "solar"; $Board = "esp32" }
        "4" { 
            $ProjectType = Read-Host "Enter project type (temp/solar)"
            $Board = Read-Host "Enter board type (esp8266/esp32)"
            $DeviceName = Read-Host "Enter device name (optional, configurable via WiFi portal)"
        }
        default {
            Write-Host "❌ Invalid choice. Exiting.`n" -ForegroundColor Red
            Read-Host "Press Enter to exit"
            exit 1
        }
    }
}

# Step 5: Flash device via WSL
$projectName = if ($ProjectType -eq "solar") { "Solar Monitor" } else { "Temperature Sensor" }
Write-Host "`n[Step 5/5] Flashing $projectName ($Board)..." -ForegroundColor Yellow
Write-Host ""

$wslCommand = "cd /home/aachten/PlatformIO/esp12f_ds18b20_temp_sensor && ./scripts/flash_device.sh $ProjectType $Board"
if ($DeviceName) {
    $wslCommand += " '$DeviceName'"
}

Write-Host "Executing in WSL: $wslCommand" -ForegroundColor Gray
Write-Host ""

wsl bash -c $wslCommand

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "✅ Flash Complete!" -ForegroundColor Green
    Write-Host "========================================`n" -ForegroundColor Green
    Write-Host "$projectName has been flashed successfully." -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "  1. Unplug the ESP device from USB" -ForegroundColor Cyan
    Write-Host "  2. Power it with 5V adapter" -ForegroundColor Cyan
    Write-Host "  3. Configure WiFi and device name via the setup portal" -ForegroundColor Cyan
    Write-Host "  4. Check serial monitor: wsl -e platformio device monitor`n" -ForegroundColor Cyan
} else {
    Write-Host "`n❌ Flash failed! Check the errors above.`n" -ForegroundColor Red
}

# Detach USB (optional)
$detach = Read-Host "Detach USB device from WSL? (y/n)"
if ($detach -eq "y") {
    Write-Host "Detaching device..." -ForegroundColor Yellow
    usbipd detach --busid $BusId
    Write-Host "✅ Device detached.`n" -ForegroundColor Green
}

Read-Host "Press Enter to exit"
