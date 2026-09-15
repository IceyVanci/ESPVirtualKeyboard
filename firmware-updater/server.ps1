param(
    [int]$Port = 8123,
    [switch]$NoBrowser
)
$ErrorActionPreference = 'Stop'

Write-Host '============================================================'
Write-Host '  ESP Virtual Keyboard - Firmware Updater'
Write-Host '============================================================'
Write-Host ''

$Root   = Split-Path -Parent $MyInvocation.MyCommand.Path
$Public = Join-Path $Root 'public'
$FwDir  = Join-Path $Root 'firmware'
$Url    = 'http://127.0.0.1:' + $Port + '/'

$Mime = @{
    '.html' = 'text/html; charset=utf-8'
    '.js'   = 'application/javascript; charset=utf-8'
    '.mjs'  = 'application/javascript; charset=utf-8'
    '.css'  = 'text/css; charset=utf-8'
    '.json' = 'application/json; charset=utf-8'
    '.txt'  = 'text/plain; charset=utf-8'
    '.bin'  = 'application/octet-stream'
    '.ico'  = 'image/x-icon'
    '.png'  = 'image/png'
    '.svg'  = 'image/svg+xml'
}

function Get-MimeFor([string]$relPath) {
    $ext = [System.IO.Path]::GetExtension($relPath).ToLower()
    if ($Mime.ContainsKey($ext)) { return $Mime[$ext] }
    return 'application/octet-stream'
}

function Get-SafeFilePath([string]$baseDir, [string]$relPath) {
    if ([string]::IsNullOrWhiteSpace($relPath)) { return $null }
    try {
        $full = [System.IO.Path]::GetFullPath((Join-Path $baseDir $relPath))
    } catch { return $null }
    $fullBase = [System.IO.Path]::GetFullPath($baseDir)
    if (-not $full.StartsWith($fullBase, [System.StringComparison]::OrdinalIgnoreCase)) { return $null }
    return $full
}

function Resolve-Request([string]$rawPath, [ref]$outPath, [ref]$outType) {
    if ($rawPath -eq '/' -or $rawPath -eq '') { $rawPath = '/index.html' }
    $clean = [uri]::UnescapeDataString($rawPath.Split('?')[0])
    $clean = $clean -replace '/', '\'
    if ($clean -like '\firmware*') {
        $rel = $clean.Substring('\firmware'.Length)
        $outPath.Value = Get-SafeFilePath -baseDir $FwDir -relPath $rel
        $outType.Value = Get-MimeFor $rel
    } else {
        $outPath.Value = Get-SafeFilePath -baseDir $Public -relPath $clean
        $outType.Value = Get-MimeFor $clean
    }
    if ($null -eq $outPath.Value) { return $false }
    if (-not (Test-Path -LiteralPath $outPath.Value)) { return $false }
    if ((Get-Item -LiteralPath $outPath.Value).PSIsContainer) { return $false }
    return $true
}

# ================= Server core =================
$UseTcpFallback = $false
$listener = $null
try {
    $listener = New-Object System.Net.HttpListener
    $listener.Prefixes.Add($Url)
    $listener.Start()
} catch {
    Write-Host "[WARN] HttpListener unavailable ($($_.Exception.Message)) - falling back to TcpListener."
    $UseTcpFallback = $true
}

if (-not $UseTcpFallback) {
    Write-Host "Serving root: $Root"
    Write-Host "Open: $Url   (Ctrl+C or close this window to stop)"
    if (-not $NoBrowser) { Start-Process $Url }
    try {
        while ($listener.IsListening) {
            $ctx = $listener.GetContext()
            try {
                $pRef = [ref]$null; $tRef = [ref]$null
                if (Resolve-Request $ctx.Request.Url.AbsolutePath $pRef $tRef) {
                    $bytes = [System.IO.File]::ReadAllBytes($pRef.Value)
                    $ctx.Response.StatusCode = 200
                    $ctx.Response.ContentType = $tRef.Value
                    $ctx.Response.ContentLength64 = $bytes.Length
                    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
                } else {
                    $ctx.Response.StatusCode = 404
                    $msg = [System.Text.Encoding]::UTF8.GetBytes('404 Not Found')
                    $ctx.Response.ContentType = 'text/plain; charset=utf-8'
                    $ctx.Response.ContentLength64 = $msg.Length
                    $ctx.Response.OutputStream.Write($msg, 0, $msg.Length)
                }
            } catch { Write-Host "[WARN] request error: $($_.Exception.Message)" }
            finally { try { $ctx.Response.OutputStream.Close() } catch {} }
        }
    } finally { $listener.Stop() }
    exit 0
}

# ---- Fallback: minimal TcpListener HTTP server ----
function Send-NotFound([System.IO.Stream]$s) {
    $msg = [System.Text.Encoding]::UTF8.GetBytes('404 Not Found')
    $b = [System.Text.Encoding]::ASCII.GetBytes("HTTP/1.1 404 Not Found`r`nContent-Type: text/plain; charset=utf-8`r`nContent-Length: $($msg.Length)`r`nConnection: close`r`n`r`n")
    $s.Write($b, 0, $b.Length); $s.Write($msg, 0, $msg.Length)
}
function Send-Bytes([System.IO.Stream]$s, [byte[]]$body, [string]$ctype) {
    $head = "HTTP/1.1 200 OK`r`nContent-Type: $ctype`r`nContent-Length: $($body.Length)`r`nCache-Control: no-store`r`nConnection: close`r`n`r`n"
    $hb = [System.Text.Encoding]::ASCII.GetBytes($head)
    $s.Write($hb, 0, $hb.Length); $s.Write($body, 0, $body.Length)
}

Write-Host "[INFO] Using TcpListener fallback on port $Port"
Write-Host "Serving root: $Root"
Write-Host "Open: $Url   (close this window to stop)"
if (-not $NoBrowser) { Start-Process $Url }
$tcp = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
$tcp.Start()
try {
    while ($true) {
        $client = $tcp.AcceptTcpClient()
        try {
            $stream = $client.GetStream()
            $stream.ReadTimeout = 5000
            $buf = New-Object byte[] 8192
            $sb = New-Object System.Text.StringBuilder
            while (-not $sb.ToString().Contains("`r`n`r`n")) {
                $n = $stream.Read($buf, 0, $buf.Length)
                if ($n -le 0) { break }
                [void]$sb.Append([System.Text.Encoding]::ASCII.GetString($buf, 0, $n))
            }
            $reqLine = ($sb.ToString() -split "`r`n")[0]
            if ($reqLine -match '^GET\s+(\S+)') {
                $pRef = [ref]$null; $tRef = [ref]$null
                if (Resolve-Request $Matches[1] $pRef $tRef) {
                    $bytes = [System.IO.File]::ReadAllBytes($pRef.Value)
                    Send-Bytes $stream $bytes $tRef.Value
                } else {
                    Send-NotFound $stream
                }
            }
        } catch { Write-Host "[WARN] request error: $($_.Exception.Message)" }
        finally { try { $client.Close() } catch {} }
    }
} finally { $tcp.Stop() }
