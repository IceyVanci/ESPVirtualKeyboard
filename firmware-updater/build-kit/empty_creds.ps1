# L1 源层：把 config.h 副本中的指定凭据宏强制清空（只改副本，绝不动工作目录）
#
# 用法：
#   empty_creds.ps1 -ConfigH "<build_tmp>\<variant>\<sketchName>\config.h" -Macros @("WIFI_SSID","WIFI_PASSWORD")
#
# 说明：
#   - 兼容「单行字符串宏」与「多行 PEM 宏（带续行/转义）」两种写法；
#   - 改写后逐个自检为 ""；任一失败 => exit 1（fail-closed，不进入编译）。
#   - 判断逻辑与本工具 L2/L3 相互独立、互不复用，避免同源缺陷。
param(
    [Parameter(Mandatory = $true)][string]$ConfigH,
    [Parameter(Mandatory = $true)][string[]]$Macros
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ConfigH)) {
    Write-Host "[ERROR] [L1] config.h 不存在: $ConfigH"
    exit 1
}
if (-not $Macros -or $Macros.Count -eq 0) {
    Write-Host "[ERROR] [L1] 未提供 -Macros（凭据宏清单为空）"
    exit 1
}
# 兼容两种传参：进程内 @("A","B")，或 -File 传入的逗号分隔单串 "A,B"
$MacroList = @()
foreach ($m in $Macros) { foreach ($p in ($m -split '[,;\s]+')) { if ($p) { $MacroList += $p } } }
if ($MacroList.Count -eq 0) {
    Write-Host "[ERROR] [L1] 凭据宏清单解析后为空"
    exit 1
}

$t = [System.IO.File]::ReadAllText($ConfigH, [System.Text.Encoding]::UTF8)
$origLen = $t.Length

# 统一改写：捕获 `#define NAME` 后的第一个字符串字面量（含转义与跨行），置为 ""
#   \\.        匹配转义序列（如 PEM 中的 \n\ 续行）
#   [^"\\]     匹配任意非引号/非反斜杠字符（含换行）
foreach ($n in $MacroList) {
    $pattern = '(?ms)^(\s*#define\s+' + [regex]::Escape($n) + '\s+)"(?:\\.|[^"\\])*"'
    $t = [regex]::Replace($t, $pattern, '$1""')
}

[System.IO.File]::WriteAllText($ConfigH, $t, (New-Object System.Text.UTF8Encoding($false)))

# ---- 自检（L1 内部验证，与 L2 的独立断言互不替代）----
$bad = @()
foreach ($n in $MacroList) {
    if ($t -notmatch ('(?m)^\s*#define\s+' + [regex]::Escape($n) + '\s+""\s*$')) { $bad += $n }
}
if ($bad.Count -gt 0) {
    Write-Host "[ERROR] [L1] 空凭据改写失败（未置空或宏缺失）: $($bad -join ', ')"
    exit 1
}
if ($t -match 'BEGIN PRIVATE KEY') {
    Write-Host "[ERROR] [L1] 改写后仍含 BEGIN PRIVATE KEY"
    exit 1
}

Write-Host ("      [L1] 空凭据改写完成并通过自检（{0} 个宏 -> 空，{1} -> {2} 字节）" -f $MacroList.Count, $origLen, $t.Length)
exit 0
