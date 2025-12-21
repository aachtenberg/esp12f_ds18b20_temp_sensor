# PowerShell Script to Attach USB Device to WSL2
# Run this as Administrator before flashing ESP devices

param(
    [Parameter(Mandatory=$false)]
    [string]$BusId = ""
)

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] 'Administrator')
if (-not $isAdmin) {
    Write-Host "❌ This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
    exit 1
}

# Check if usbipd is installed
$usbipd = Get-Command usbipd -ErrorAction SilentlyContinue
if (-not $usbipd) {
    Write-Host "❌ USBIPD-WIN is not installed!" -ForegroundColor Red
    Write-Host "Installing USBIPD-WIN..." -ForegroundColor Yellow
    winget install usbipd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Installation failed. Please install manually:" -ForegroundColor Red
        Write-Host "   winget install usbipd" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "`n================================" -ForegroundColor Cyan
Write-Host "USB Device Manager for WSL2" -ForegroundColor Cyan
Write-Host "================================`n" -ForegroundColor Cyan

# List all USB devices
Write-Host "Listing USB devices..." -ForegroundColor Yellow
Write-Host ""
$devices = usbipd list

Write-Host $devices
Write-Host ""

# If no BusId provided, ask user
if ([string]::IsNullOrEmpty($BusId)) {
    $BusId = Read-Host "Enter the BUSID of the device to attach (or 'list' to show again)"
    
    if ($BusId -eq "list") {
        Write-Host ""
        Write-Host $devices
        Write-Host ""
        $BusId = Read-Host "Enter the BUSID of the device to attach"
    }
}

if ([string]::IsNullOrEmpty($BusId)) {
    Write-Host "❌ No BUSID provided. Exiting." -ForegroundColor Red
    exit 1
}

Write-Host "`nAttaching device $BusId to WSL2..." -ForegroundColor Yellow
usbipd attach --wsl --busid $BusId

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Device attached successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "The device should now be available in WSL2 at /dev/ttyUSB0 or /dev/ttyACM0" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now run the flash script in WSL2:" -ForegroundColor Yellow
    Write-Host "  cd /home/$env:USER/PlatformIO/esp12f_ds18b20_temp_sensor" -ForegroundColor Cyan
    Write-Host "  scripts/flash_device.sh 'Big Garage' esp8266" -ForegroundColor Cyan
} else {
    Write-Host "❌ Failed to attach device. Please check the BUSID and try again." -ForegroundColor Red
    exit 1
}

Write-Host ""
