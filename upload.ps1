param([string]$Port = "")

$ErrorActionPreference = "Stop"
$cli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$sketch = $PSScriptRoot
# Giống hệt tùy chọn board trong Arduino IDE. Đổi PartitionScheme/FlashSize sẽ xóa dữ liệu đã lưu trong ESP32.
$fqbn = "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default"

if (-not $Port) {
  $ports = (& $cli board list --format json | ConvertFrom-Json).detected_ports |
    Where-Object { $_.port.protocol -eq "serial" } |
    ForEach-Object { $_.port.address }
  if (-not $ports) { throw "Khong thay cong COM nao. Cam cap USB vao cong COM cua ESP32." }
  $Port = @($ports)[0]
}

$monitor = Get-Process serial-monitor -ErrorAction SilentlyContinue
if ($monitor) {
  Write-Host "Tat Serial Monitor de giai phong $Port"
  $monitor | Stop-Process -Force
  Start-Sleep -Seconds 1
}

Write-Host "Nap $sketch -> $Port"
& $cli compile --fqbn $fqbn --upload --port $Port $sketch
if ($LASTEXITCODE -ne 0) { throw "Nap that bai (exit $LASTEXITCODE)" }
Write-Host "Nap xong."
