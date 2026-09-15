param(
    [Parameter(Mandatory = $true)][string]$FwDir,
    [Parameter(Mandatory = $true)][string]$ConfigH,
    [Parameter(Mandatory = $true)][string]$PublicDir
)
$ErrorActionPreference = 'Stop'
$Utf8 = New-Object System.Text.UTF8Encoding($false)

# ---- 0. EMPTY-CREDENTIAL ASSERTION (D14): distributable package must NOT embed real WiFi credentials ----
$ssidLine = Select-String -Path $ConfigH -Pattern '^\s*#define\s+WIFI_SSID\s+""\s*$' | Select-Object -First 1
$passLine = Select-String -Path $ConfigH -Pattern '^\s*#define\s+WIFI_PASSWORD\s+""\s*$' | Select-Object -First 1
if (-not $ssidLine -or -not $passLine) {
    Write-Host '[ERROR] EMPTY-CREDENTIAL ASSERTION FAILED:'
    Write-Host '        config.h copy must define WIFI_SSID and WIFI_PASSWORD as empty strings.'
    Write-Host '        Refusing to build a package that may leak real WiFi credentials.'
    Write-Host "        File checked: $ConfigH"
    exit 1
}
Write-Host '      credentials: EMPTY (asserted OK)'

# ---- 1. Extract FW_VERSION from config.h ----
$verLine = Select-String -Path $ConfigH -Pattern '#define\s+FW_VERSION\s+"([^"]+)"' | Select-Object -First 1
if (-not $verLine) {
    Write-Host "[ERROR] FW_VERSION not found in $ConfigH"
    exit 1
}
$version = $verLine.Matches[0].Groups[1].Value
$verNum = $version -replace '^v', ''   # 避免 vv1.0 式重复前缀
Write-Host "      Firmware version: $version"

# ---- 2. manifest.json / VERSION.txt ----
$items = @(
    @{ name = 'bootloader.bin';  offset = 0x0      },
    @{ name = 'partitions.bin';  offset = 0x8000   },
    @{ name = 'boot_app0.bin';   offset = 0xE000   },
    @{ name = 'app.bin';         offset = 0x10000  }
)
if (-not (Test-Path $FwDir)) {
    Write-Host "[ERROR] Missing firmware directory: $FwDir"
    exit 1
}
[System.IO.File]::WriteAllText((Join-Path $FwDir 'VERSION.txt'), $version, $Utf8)

$fileList = @()
foreach ($it in $items) {
    $path = Join-Path $FwDir $it.name
    if (-not (Test-Path $path)) {
        Write-Host "[ERROR] Missing image: $($it.name) in $FwDir"
        exit 1
    }
    $hash = (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLower()
    $size = (Get-Item $path).Length
    $fileList += [ordered]@{ name = $it.name; offset = $it.offset; size = $size; sha256 = $hash }
    Write-Host ("      {0,-15} offset=0x{1:X5}  size={2,10}  sha256={3}..." -f $it.name, $it.offset, $size, $hash.Substring(0, 12))
}

$manifest = [ordered]@{
    version     = $version
    chip        = 'esp32c3'
    credentials = 'empty'
    flash       = [ordered]@{ mode = 'dio'; freq = '80m'; size = '4MB' }
    files       = $fileList
}
$json = $manifest | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText((Join-Path $FwDir 'manifest.json'), $json, $Utf8)
Write-Host '      manifest.json / VERSION.txt written'

# ---- Size check (app slot = 3MB, warn over 2.7MB) ----
$appSize = ($fileList | Where-Object { $_.name -eq 'app.bin' }).size
$slot = 3145728
$warn = 2831155
$pct = [math]::Round($appSize * 100.0 / $slot, 1)
Write-Host ("      app.bin size: {0} / {1} bytes ({2}% of 3MB slot)" -f $appSize, $slot, $pct)
if ($appSize -gt $warn) {
    Write-Host '[WARN] Firmware exceeds 2.7MB - app slot filling up. Consider trimming.'
}

# ---- 3. Download esptool-js bundle if missing ----
$bundlePath = Join-Path $PublicDir 'esptool-js\bundle.js'
if (-not (Test-Path $bundlePath)) {
    Write-Host '      Downloading esptool-js bundle (0.6.1)...'
    try {
        curl.exe -sSL -o $bundlePath 'https://unpkg.com/esptool-js@0.6.1/bundle.js'
        if (-not (Test-Path $bundlePath)) { throw 'download failed' }
        $len = (Get-Item $bundlePath).Length
        if ($len -lt 100000) { throw ("bundle too small: $len bytes") }
        Write-Host ("      bundle.js downloaded ({0} bytes)" -f $len)
    } catch {
        Write-Host "[ERROR] Failed to download esptool-js bundle: $_"
        exit 1
    }
} else {
    Write-Host '      bundle.js already present'
}

# ---- 4. Package distribution zip ----
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$toolRoot = Split-Path -Parent $FwDir
$zipName = "固件升级工具_v$verNum.zip"
$zipPath = Join-Path $toolRoot $zipName
Get-ChildItem -Path $toolRoot -File -Filter '*.zip' | Remove-Item -Force
try {
    $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    Get-ChildItem -Path $toolRoot -Recurse -File | Where-Object {
        $_.FullName -notmatch '\\build_tmp\\' -and $_.FullName -ne $zipPath -and
        $_.Name -notmatch '^ESPVirtualKeyboard\.ino\.(bin|elf|map|merged\.bin)$'
    } | ForEach-Object {
        $rel = $_.FullName.Substring($toolRoot.Length + 1)
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $rel) | Out-Null
    }
    $zip.Dispose()
    $zipSize = (Get-Item $zipPath).Length
    Write-Host ("      packaged: {0} ({1:N0} bytes, build_tmp excluded)" -f $zipName, $zipSize)
} catch {
    Write-Host "[ERROR] Packaging zip failed: $_"
    exit 1
}

exit 0