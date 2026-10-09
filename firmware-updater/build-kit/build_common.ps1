# ============================================================================
# build_common.ps1 —— ESPEZUpdater 通用构建套件（build-kit）
#
# 定位：本脚本属于「独立构建套件」，由使用升级工具的项目内置（vendor）后，在
#       项目侧运行；所有产物写入项目侧 -OutDir，绝不写工具仓。
#
# 用法：
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\build-kit\build_common.ps1 `
#       -Build .\build.json [-OutDir .\dist] [-Bundle]
#
# 参数：
#   -Build    项目侧构建档 build.json（必填）。相对路径按该文件所在目录解析。
#   -OutDir   产物目录，缺省 = <build.json 目录>\dist。
#             firmware\、build_tmp\、verify.log、zip 全部写入此处。
#   -Bundle   产物模式（构建时选择）：
#             缺省        —— 仅固件产物：dist\firmware\...、dist\firmware\verify.log、
#                            dist\固件包_v<版本>.zip
#             -Bundle     —— 自包含分发：另将运行时工具复制进 dist\，产出
#                            dist\固件升级工具_v<版本>.zip
#   -SkipZip  不打包 zip（调试用）；-KeepTmp 保留 build_tmp\
#
# 构建档可选字段：
#   runtimeToolDir  Bundle 模式下运行时工具来源目录；缺省尝试 <kitDir>\runtime。
#                   须包含 server.ps1 / public\ / esptool.exe / flash_cli.bat / *.bat
#
# 流程（对应设计方案第 6.4 节）：解析 -> 清理 -> 逐变体(拷贝/宏/L1/编译/抽件)
#   -> 版本 -> L2/L3 -> manifest/profile/VERSION -> 体积 -> bundle -> zip -> verify.log
# 任一层失败 => exit 1，不产出可分发产物（fail-closed）。
# ============================================================================
param(
    [Parameter(Mandatory = $true)][string]$Build,
    [string]$OutDir = '',
    [switch]$Bundle,
    [switch]$SkipZip,
    [switch]$KeepTmp
)
$ErrorActionPreference = 'Stop'

$Utf8   = New-Object System.Text.UTF8Encoding($false)
$Latin1 = [System.Text.Encoding]::GetEncoding(28591)
$KitDir = $PSScriptRoot
if (-not $KitDir) { $KitDir = Split-Path -Parent $MyInvocation.MyCommand.Path }

function Fail([string]$msg) {
    Write-Host "[ERROR] $msg"
    exit 1
}

# ---------------------------------------------------------------- 0. 解析构建档
if (-not (Test-Path -LiteralPath $Build)) { Fail "构建档不存在: $Build" }
$Build = (Resolve-Path -LiteralPath $Build).Path
$buildDir = Split-Path -Parent $Build
$cfg = Get-Content -LiteralPath $Build -Raw -Encoding UTF8 | ConvertFrom-Json

foreach ($req in @('product', 'sketchDir', 'versionMacro', 'credentialMacros', 'chip', 'fqbn', 'flash', 'profileSource')) {
    if (-not $cfg.PSObject.Properties.Name.Contains($req) -or $null -eq $cfg.$req) {
        Fail "构建档缺少必填字段: $req"
    }
}
if (-not (Test-Path -LiteralPath $cfg.sketchDir)) { Fail "sketchDir 不存在: $($cfg.sketchDir)" }

$sketchName = $cfg.sketchName
if (-not $sketchName) { $sketchName = Split-Path -Leaf ($cfg.sketchDir.TrimEnd('\', '/')) }

# 产物目录：缺省 <build.json 目录>\dist（项目侧）
if (-not $OutDir) { $OutDir = Join-Path $buildDir 'dist' }
if (-not [System.IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path (Get-Location).Path $OutDir }
$OutDir = [System.IO.Path]::GetFullPath($OutDir)

$profileSrc = $cfg.profileSource
if (-not [System.IO.Path]::IsPathRooted($profileSrc)) { $profileSrc = Join-Path $buildDir $profileSrc }
if (-not (Test-Path -LiteralPath $profileSrc)) { Fail "profileSource 不存在: $profileSrc" }

$macros = @()
foreach ($m in $cfg.credentialMacros) {
    foreach ($p in ($m -split '[,;\s]+')) { if ($p) { $macros += $p } }
}
if ($macros.Count -eq 0) { Fail "credentialMacros 为空，拒绝构建" }

$genericNeedles = @()
if ($cfg.PSObject.Properties.Name.Contains('genericNeedles') -and $cfg.genericNeedles) { $genericNeedles = @($cfg.genericNeedles) }
$allowValues = @()
if ($cfg.PSObject.Properties.Name.Contains('allowValues') -and $cfg.allowValues) { $allowValues = @($cfg.allowValues) }
$ipRegex = '(?:192\.168|10|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}'
if ($cfg.PSObject.Properties.Name.Contains('internalIpRegex') -and $cfg.internalIpRegex) { $ipRegex = $cfg.internalIpRegex }

$variants = @()
if ($cfg.PSObject.Properties.Name.Contains('variants') -and $cfg.variants) { $variants = @($cfg.variants) }
if ($variants.Count -eq 0) { $variants = @([pscustomobject]@{ id = 'default'; copySubdir = '' }) }

$flashMode = $cfg.flash.mode; $flashFreq = $cfg.flash.freq; $flashSize = $cfg.flash.size
$appSlot = if ($cfg.PSObject.Properties.Name.Contains('appSlot') -and $cfg.appSlot) { [int]$cfg.appSlot } else { 3145728 }
$appWarn = if ($cfg.PSObject.Properties.Name.Contains('appWarn') -and $cfg.appWarn) { [int]$cfg.appWarn } else { 2831155 }

# 运行时工具来源（仅 Bundle 需要）
$runtimeToolDir = ''
if ($Bundle) {
    if ($cfg.PSObject.Properties.Name.Contains('runtimeToolDir') -and $cfg.runtimeToolDir) {
        $runtimeToolDir = $cfg.runtimeToolDir
        if (-not [System.IO.Path]::IsPathRooted($runtimeToolDir)) { $runtimeToolDir = Join-Path $buildDir $runtimeToolDir }
    } elseif (Test-Path -LiteralPath (Join-Path $KitDir 'runtime')) {
        $runtimeToolDir = Join-Path $KitDir 'runtime'
    }
    if (-not $runtimeToolDir -or -not (Test-Path -LiteralPath $runtimeToolDir)) {
        Fail "-Bundle 需要运行时工具目录：请在 build.json 指定 runtimeToolDir，或将工具复制到 <kitDir>\runtime"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $runtimeToolDir 'server.ps1'))) {
        Fail "runtimeToolDir 中缺少 server.ps1: $runtimeToolDir"
    }
}

$firmwareDir = Join-Path $OutDir 'firmware'
$buildTmp    = Join-Path $OutDir 'build_tmp'

Write-Host "============================================================"
Write-Host " ESPEZUpdater build - $($cfg.product)"
Write-Host " build.json : $Build"
Write-Host " outDir     : $OutDir"
Write-Host " mode       : $(if ($Bundle) { 'bundle (self-contained)' } else { 'firmware-only' })"
Write-Host " variants   : $(($variants | ForEach-Object { $_.id }) -join ', ')"
Write-Host "============================================================"

# ---------------------------------------------------------------- 1. 清理
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path -LiteralPath $firmwareDir) { Remove-Item -LiteralPath $firmwareDir -Recurse -Force }
if (Test-Path -LiteralPath $buildTmp)    { Remove-Item -LiteralPath $buildTmp -Recurse -Force }
New-Item -ItemType Directory -Force -Path $firmwareDir | Out-Null

# ---------------------------------------------------------------- 2. 逐变体构建
foreach ($v in $variants) {
    $vid = $v.id
    if (-not $vid) { Fail "variant 缺少 id" }
    $vdir = Join-Path $firmwareDir $vid
    New-Item -ItemType Directory -Force -Path $vdir | Out-Null

    Write-Host ""
    Write-Host "  [variant $vid]"

    # a. 拷贝源码（仅根目录，不递归）
    $srcDir = Join-Path (Join-Path $buildTmp $vid) $sketchName
    New-Item -ItemType Directory -Force -Path $srcDir | Out-Null
    Get-ChildItem -LiteralPath $cfg.sketchDir -File |
        Where-Object { $_.Extension -in @('.ino', '.h', '.cpp', '.c') } |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $srcDir -Force }
    if (-not (Get-ChildItem -LiteralPath $srcDir -File -Filter '*.ino')) {
        Fail "[$vid] 副本中没有 .ino，无法编译"
    }
    $cfgCopy = Join-Path $srcDir 'config.h'

    # b. 应用变体宏（仅改副本）
    if ($v.undefine) {
        if (-not (Test-Path -LiteralPath $cfgCopy)) { Fail "[$vid] 需要 undefine 但副本缺少 config.h" }
        $t = [System.IO.File]::ReadAllText($cfgCopy, [System.Text.Encoding]::UTF8)
        foreach ($m in $v.undefine) {
            $t = [regex]::Replace($t, ('(?m)^(\s*)#define\s+' + [regex]::Escape($m) + '(\s.*)?$'), ('$1// #define ' + $m))
        }
        [System.IO.File]::WriteAllText($cfgCopy, $t, $Utf8)
        Write-Host "      [variant] undefine: $($v.undefine -join ', ')"
    }
    if ($v.define) {
        if (-not (Test-Path -LiteralPath $cfgCopy)) { Fail "[$vid] 需要 define 但副本缺少 config.h" }
        $t = [System.IO.File]::ReadAllText($cfgCopy, [System.Text.Encoding]::UTF8)
        foreach ($m in $v.define) {
            if ($t -notmatch ('(?m)^\s*#define\s+' + [regex]::Escape($m) + '(\s|$)')) {
                $t = "#ifndef $m`r`n#define $m`r`n#endif`r`n" + $t
            }
        }
        [System.IO.File]::WriteAllText($cfgCopy, $t, $Utf8)
        Write-Host "      [variant] define: $($v.define -join ', ')"
    }

    # c. L1 空凭据（独立进程，沿用 exit code）
    $l1 = Join-Path $KitDir 'empty_creds.ps1'
    if (-not (Test-Path -LiteralPath $l1)) { Fail "缺少 build-kit\empty_creds.ps1: $l1" }
    $macroArg = ($macros -join ',')
    & (Join-Path $PSHOME 'powershell.exe') -NoProfile -ExecutionPolicy Bypass -File $l1 -ConfigH $cfgCopy -Macros $macroArg
    if ($LASTEXITCODE -ne 0) { Fail "[$vid] [L1] 空凭据改写失败，中止构建" }

    # d. 编译
    Write-Host "      [compile] arduino-cli compile --fqbn <cfg> --output-dir firmware\$vid"
    & arduino-cli compile --fqbn $cfg.fqbn --output-dir $vdir $srcDir
    if ($LASTEXITCODE -ne 0) { Fail "[$vid] 编译失败" }

    # e. 抽四件套
    $boot = Join-Path $vdir "$sketchName.ino.bootloader.bin"
    $part = Join-Path $vdir "$sketchName.ino.partitions.bin"
    $app  = Join-Path $vdir "$sketchName.ino.bin"
    $coreBoot = Join-Path $cfg.coreDir 'tools\partitions\boot_app0.bin'
    foreach ($p in @($boot, $part, $app, $coreBoot)) {
        if (-not (Test-Path -LiteralPath $p)) { Fail "[$vid] 缺少产物: $p" }
    }
    Copy-Item -LiteralPath $boot -Destination (Join-Path $vdir 'bootloader.bin') -Force
    Copy-Item -LiteralPath $part -Destination (Join-Path $vdir 'partitions.bin') -Force
    Copy-Item -LiteralPath $app  -Destination (Join-Path $vdir 'app.bin') -Force
    Copy-Item -LiteralPath $coreBoot -Destination (Join-Path $vdir 'boot_app0.bin') -Force
    Write-Host "      bootloader.bin / partitions.bin / boot_app0.bin / app.bin - OK"

    # f. 删除中间产物
    Get-ChildItem -LiteralPath $vdir -File | Where-Object { $_.Name -like "$sketchName.ino.*" } |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------- 3. 版本号
$workCfg = Join-Path $cfg.sketchDir 'config.h'
if (-not (Test-Path -LiteralPath $workCfg)) { Fail "工作目录缺少 config.h: $workCfg" }
$workTxt = [System.IO.File]::ReadAllText($workCfg, [System.Text.Encoding]::UTF8)
$verMatch = [regex]::Match($workTxt, '(?m)^\s*#define\s+' + [regex]::Escape($cfg.versionMacro) + '\s+"([^"]+)"')
if (-not $verMatch.Success) { Fail "未找到 $($cfg.versionMacro) 于 $workCfg" }
$version = $verMatch.Groups[1].Value
Write-Host ""
Write-Host "      Firmware version: $version"

# L3 反查真值
$realNeedles = @()
$vals = @()
foreach ($n in $macros) {
    $m = [regex]::Match($workTxt, '(?ms)^\s*#define\s+' + [regex]::Escape($n) + '\s+"(.*?)"')
    if ($m.Success -and $m.Groups[1].Value.Length -ge 4) { $vals += $m.Groups[1].Value }
}
$tokens = @()
foreach ($val in $vals) {
    if ($val.Length -ge 8) { $tokens += $val }
    foreach ($p in ($val -split '[^A-Za-z0-9+/=._\-@:]')) { if ($p.Length -ge 8) { $tokens += $p } }
}
$realNeedles = @($tokens | Sort-Object -Unique)
if ($realNeedles.Count -gt 0) {
    Write-Host "      [L3] 从工作目录 config.h 提取 $($realNeedles.Count) 个真值用于反查"
} else {
    Write-Host "      [L3] 工作目录 config.h 凭据为空（无真值可反查）"
}

# ---------------------------------------------------------------- 4/5. L2 + L3
$verifyLines = @()
$verifyLines += "$($cfg.product) firmware package - empty-credential verification"
$verifyLines += "version   : $version"
$verifyLines += "generated : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$verifyLines += "mode      : $(if ($Bundle) { 'bundle' } else { 'firmware-only' })"
$verifyLines += "L1 source : config.h copy rewritten to empty (empty_creds.ps1 self-check passed)"
$verifyLines += "L2 source : assertion passed ($($macros.Count) credential macros == empty)"
$verifyLines += "L3 binary : byte-level scan (ASCII + UTF-16LE)"
$verifyLines += "generator : build-kit\build_common.ps1 + empty_creds.ps1 (L1/L2/L3 fail-closed)"
$verifyLines += ""

$items = @(
    @{ name = 'bootloader.bin'; offset = 0x0 },
    @{ name = 'partitions.bin'; offset = 0x8000 },
    @{ name = 'boot_app0.bin';  offset = 0xE000 },
    @{ name = 'app.bin';        offset = 0x10000 }
)

foreach ($v in $variants) {
    $vid = $v.id
    $vdir = Join-Path $firmwareDir $vid
    $srcDir = Join-Path (Join-Path $buildTmp $vid) $sketchName
    $cfgCopy = Join-Path $srcDir 'config.h'

    # ---- L2 ----
    if (-not (Test-Path -LiteralPath $cfgCopy)) { Fail "[$vid] [L2] 副本 config.h 缺失" }
    $ct = [System.IO.File]::ReadAllText($cfgCopy, [System.Text.Encoding]::UTF8)
    $notEmpty = @()
    foreach ($n in $macros) {
        if ($ct -notmatch ('(?m)^\s*#define\s+' + [regex]::Escape($n) + '\s+""\s*$')) { $notEmpty += $n }
    }
    if ($notEmpty.Count -gt 0) { Fail "[$vid] [L2] 副本 config.h 未清空: $($notEmpty -join ', ')" }
    foreach ($f in (Get-ChildItem -LiteralPath $srcDir -File | Where-Object { $_.Extension -in @('.h', '.ino', '.cpp', '.c') })) {
        $fc = [System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)
        foreach ($pat in $genericNeedles) {
            if ($fc.Contains($pat)) { Fail "[$vid] [L2] 源码含敏感特征 '$pat': $($f.Name)" }
        }
        foreach ($m in [regex]::Matches($fc, $ipRegex)) {
            if ($allowValues -notcontains $m.Value) { Fail "[$vid] [L2] 源码含内网 IP '$($m.Value)': $($f.Name)" }
        }
    }
    Write-Host "      [L2] $vid 副本凭据已清空、源码无敏感特征 OK"

    # ---- L3 ----
    $modeHits = @()
    foreach ($it in $items) {
        $path = Join-Path $vdir $it.name
        if (-not (Test-Path -LiteralPath $path)) { Fail "[$vid] 缺少产物: $($it.name)" }
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $texts = @($Latin1.GetString($bytes), [System.Text.Encoding]::Unicode.GetString($bytes))
        foreach ($t in $texts) {
            foreach ($nd in ($realNeedles + $genericNeedles)) {
                if ($allowValues -contains $nd) { continue }
                if ($t.Contains($nd)) { $modeHits += "$($it.name):<$nd>" }
            }
            foreach ($m in [regex]::Matches($t, $ipRegex)) {
                if ($allowValues -contains $m.Value) { continue }
                $modeHits += "$($it.name):$($m.Value)"
            }
        }
    }
    $modeHits = @($modeHits | Sort-Object -Unique)
    if ($modeHits.Count -gt 0) {
        foreach ($h in $modeHits) { Write-Host "        - $($h.Split(':')[0]) 命中" }
        if (Test-Path -LiteralPath $firmwareDir) { Remove-Item -LiteralPath $firmwareDir -Recurse -Force }
        Fail "[$vid] [L3] 固件产物含敏感信息（命中 $($modeHits.Count) 项），已删除 firmware 目录"
    }
    Write-Host "      [L3] $vid 四件套字节级扫描：命中 0 项 OK"
    $verifyLines += "[$vid] scan hits = 0  (bootloader/partitions/boot_app0/app: OK)"

    # ---- 6. manifest / VERSION / profile ----
    $fileList = @()
    foreach ($it in $items) {
        $path = Join-Path $vdir $it.name
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLower()
        $size = (Get-Item -LiteralPath $path).Length
        $fileList += [ordered]@{ name = $it.name; offset = $it.offset; size = $size; sha256 = $hash }
    }
    $manifest = [ordered]@{
        version     = $version
        chip        = $cfg.chip
        credentials = 'empty'
        flash       = [ordered]@{ mode = $flashMode; freq = $flashFreq; size = $flashSize }
        files       = $fileList
    }
    [System.IO.File]::WriteAllText((Join-Path $vdir 'manifest.json'), ($manifest | ConvertTo-Json -Depth 5), $Utf8)
    [System.IO.File]::WriteAllText((Join-Path $vdir 'VERSION.txt'), $version, $Utf8)
    Copy-Item -LiteralPath $profileSrc -Destination (Join-Path $vdir 'profile.json') -Force
    Write-Host "      manifest.json / VERSION.txt / profile.json written"

    # ---- 7. 体积检查 ----
    $appSize = ($fileList | Where-Object { $_.name -eq 'app.bin' }).size
    $pct = [math]::Round($appSize * 100.0 / $appSlot, 1)
    Write-Host ("      app.bin size: {0} / {1} bytes ({2}%)" -f $appSize, $appSlot, $pct)
    if ($appSize -gt $appWarn) {
        Write-Host "[WARN] 固件超过告警阈值，应用槽位接近上限"
    }
}

# 根级 profile（页面引导用：在加载 manifest 前先发现变体）
Copy-Item -LiteralPath $profileSrc -Destination (Join-Path $firmwareDir 'profile.json') -Force

$verifyLines += ""
$verifyLines += "allowlist (legit values in firmware) : $($allowValues -join ', ')"
$verifyLines += "result: PASS - distributed firmware contains no credentials"
[System.IO.File]::WriteAllText((Join-Path $firmwareDir 'verify.log'), ($verifyLines -join "`r`n"), $Utf8)
Write-Host "      verify.log written"

# ---------------------------------------------------------------- 8. bundle：复制运行时工具
if ($Bundle) {
    $toolFiles = @('server.ps1', 'esptool.exe', 'flash_cli.bat', '一键升级.bat', 'upgrade.bat', 'README.md')
    foreach ($f in $toolFiles) {
        $p = Join-Path $runtimeToolDir $f
        if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination $OutDir -Force }
    }
    $pubSrc = Join-Path $runtimeToolDir 'public'
    if (Test-Path -LiteralPath $pubSrc) {
        $pubDst = Join-Path $OutDir 'public'
        if (Test-Path -LiteralPath $pubDst) { Remove-Item -LiteralPath $pubDst -Recurse -Force }
        Copy-Item -LiteralPath $pubSrc -Destination $pubDst -Recurse -Force
    }
    if (-not (Test-Path -LiteralPath (Join-Path $OutDir 'public\esptool-js\bundle.js'))) {
        Write-Host "[WARN] 运行时工具缺少 public\esptool-js\bundle.js（请确保 runtimeToolDir 完整）"
    }
    Write-Host "      runtime tool copied into outDir"
}

if (-not $KeepTmp) { if (Test-Path -LiteralPath $buildTmp) { Remove-Item -LiteralPath $buildTmp -Recurse -Force } }

# ---------------------------------------------------------------- 9. 打包 zip
if (-not $SkipZip) {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    if ($Bundle) { $zipName = "固件升级工具_v$version.zip" } else { $zipName = "固件包_v$version.zip" }
    $zipPath = Join-Path $OutDir $zipName
    Get-ChildItem -Path $OutDir -File -Filter '*.zip' | Remove-Item -Force -ErrorAction SilentlyContinue
    try {
        $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
        if ($Bundle) {
            Get-ChildItem -Path $OutDir -Recurse -File | Where-Object {
                $_.FullName -notmatch '\\build_tmp\\' -and $_.FullName -ne $zipPath -and
                $_.Extension -notin @('.elf', '.map') -and
                $_.Name -ne 'sdkconfig' -and
                $_.Name -notmatch '\.merged\.bin$'
            } | ForEach-Object {
                $rel = $_.FullName.Substring($OutDir.Length + 1)
                [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $rel) | Out-Null
            }
        } else {
            Get-ChildItem -Path $firmwareDir -Recurse -File | Where-Object {
                $_.Extension -notin @('.elf', '.map') -and $_.Name -ne 'sdkconfig' -and
                $_.Name -notmatch '\.merged\.bin$'
            } | ForEach-Object {
                $rel = 'firmware\' + $_.FullName.Substring($firmwareDir.Length + 1)
                [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $rel) | Out-Null
            }
        }
        $zip.Dispose()
        $zipSize = (Get-Item -LiteralPath $zipPath).Length
        Write-Host ("      packaged: {0} ({1:N0} bytes)" -f $zipName, $zipSize)
    } catch {
        Fail "Packaging zip failed: $_"
    }
}

Write-Host ""
Write-Host "============================================================"
Write-Host " DONE -> $OutDir"
Write-Host "   firmware\ ... + firmware\verify.log"
if (-not $SkipZip) { Write-Host "   + zip" }
Write-Host "============================================================"
exit 0
