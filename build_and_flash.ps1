# Script build và flash BLUFI cho ESP32
# Sử dụng: .\build_and_flash.ps1 [esp32|esp32s2|esp32s3|esp32c3|esp32c6] [PORT]

param(
    [Parameter(Position=0)]
    [ValidateSet("esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32c2")]
    [string]$Target = "esp32s3",
    
    [Parameter(Position=1)]
    [string]$Port = ""
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  ESP32 BLUFI Build & Flash Script" -ForegroundColor Cyan
Write-Host "  Target: $Target" -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Cyan

# Kiểm tra ESP-IDF đã được source chưa
if (-not $env:IDF_PATH) {
    Write-Host "ERROR: ESP-IDF chưa được khởi tạo!" -ForegroundColor Red
    Write-Host "Vui lòng chạy: . $env:IDF_PATH\export.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "`n[1/5] Thiết lập target: $Target" -ForegroundColor Green
idf.py set-target $Target
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Không thể thiết lập target!" -ForegroundColor Red
    exit 1
}

Write-Host "`n[2/5] Build project..." -ForegroundColor Green
idf.py build
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build thất bại!" -ForegroundColor Red
    exit 1
}

Write-Host "`n[3/5] Tìm cổng COM..." -ForegroundColor Green
if ($Port -eq "") {
    # Tự động tìm cổng COM
    $ports = Get-WmiObject -Class Win32_SerialPort | Where-Object { $_.Description -like "*USB*" -or $_.Description -like "*Serial*" }
    if ($ports) {
        $Port = $ports[0].DeviceID
        Write-Host "Tìm thấy cổng: $Port" -ForegroundColor Yellow
    } else {
        Write-Host "Không tìm thấy cổng COM. Vui lòng chỉ định cổng thủ công." -ForegroundColor Yellow
        $Port = Read-Host "Nhập cổng COM (ví dụ: COM3)"
    }
}

Write-Host "`n[4/5] Flash firmware vào $Port..." -ForegroundColor Green
idf.py -p $Port flash
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Flash thất bại!" -ForegroundColor Red
    exit 1
}

Write-Host "`n[5/5] Khởi động serial monitor..." -ForegroundColor Green
Write-Host "Nhấn Ctrl+] để thoát monitor" -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Cyan
idf.py -p $Port monitor

