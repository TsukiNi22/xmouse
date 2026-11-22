<#
Edition:
##  22/11/2026 by Tsukini

File Name:
##  install.ps1

File Description:
##  Install xmouse on Windows from its zip (release or local): files, PATH, Start menu, firewall, autostart
#>
[CmdletBinding()]
param(
    [string]$Zip = "",                  # local xmouse-windows-x64.zip (default: the folder of this script, else the latest release)
    [string]$Repo = "TsukiNi22/xmouse",
    [string]$Prefix = "$env:LOCALAPPDATA\Programs\xmouse",
    [int]$Port = 47900,
    [switch]$Autostart,                 # xmouse at login (tray)
    [switch]$NoFirewall,                # do not open the port for the LAN
    [switch]$Uninstall
)
$ErrorActionPreference = "Stop"

function Say([string]$Text) { Write-Host "xmouse " -ForegroundColor Blue -NoNewline; Write-Host $Text }
$StartMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\xmouse"
$Startup = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Startup\xmouse.lnk"

# ===== Uninstall =====
if ($Uninstall) {
    Get-Process xmouse -ErrorAction SilentlyContinue | Stop-Process -Force
    Remove-Item -Recurse -Force $Prefix, $StartMenu -ErrorAction SilentlyContinue
    Remove-Item -Force $Startup -ErrorAction SilentlyContinue
    $path = [Environment]::GetEnvironmentVariable("Path", "User") -split ";" | Where-Object { $_ -and $_ -ne $Prefix }
    [Environment]::SetEnvironmentVariable("Path", ($path -join ";"), "User")
    Start-Process -Verb RunAs -WindowStyle Hidden netsh -ArgumentList "advfirewall firewall delete rule name=xmouse" -ErrorAction SilentlyContinue
    Say "removed (settings and logs stay in $env:LOCALAPPDATA\xmouse)"
    exit 0
}

# ===== Files =====
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Zip -and (Test-Path (Join-Path $here "xmouse.exe"))) {
    $source = $here                     # run from the extracted zip
} else {
    if (-not $Zip) {
        Say "downloading the latest release of $Repo"
        $Zip = Join-Path $env:TEMP "xmouse-windows-x64.zip"
        # The newest release carrying the zip (pre-releases included: releases/latest skips them)
        $asset = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases" | ForEach-Object { $_.assets } |
            Where-Object { $_.name -eq "xmouse-windows-x64.zip" } | Select-Object -First 1
        if (-not $asset) { throw "No xmouse-windows-x64.zip in the releases of $Repo" }
        Invoke-WebRequest $asset.browser_download_url -OutFile $Zip
    }
    $extract = Join-Path $env:TEMP "xmouse-install"
    Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
    Expand-Archive -Path $Zip -DestinationPath $extract -Force
    $source = (Get-ChildItem $extract -Recurse -Filter xmouse.exe | Select-Object -First 1).DirectoryName
}
Get-Process xmouse -ErrorAction SilentlyContinue | Stop-Process -Force
New-Item -ItemType Directory -Force $Prefix | Out-Null
Copy-Item -Recurse -Force (Join-Path $source "*") $Prefix
Say "files in $Prefix"

# ===== PATH =====
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (($userPath -split ";") -notcontains $Prefix) {
    [Environment]::SetEnvironmentVariable("Path", "$userPath;$Prefix", "User")
    Say "added to the PATH (new terminals): xmouse join <link of a member>"
}

# ===== Shortcuts =====
$shell = New-Object -ComObject WScript.Shell
New-Item -ItemType Directory -Force $StartMenu | Out-Null
function Link([string]$Path, [string]$Arguments, [string]$Description) {
    $link = $shell.CreateShortcut($Path)
    $link.TargetPath = Join-Path $Prefix "xmouse.exe"
    $link.Arguments = $Arguments
    $link.WorkingDirectory = $Prefix
    $link.Description = $Description
    $link.WindowStyle = 7               # minimized console (the window is the Qt panel)
    $link.Save()
}
Link (Join-Path $StartMenu "xmouse.lnk") "--gui" "One cursor, keyboard and clipboard for every device of your desk"
if ($Autostart) { Link $Startup "--gui" "xmouse at login"; Say "starts at login" }

# ===== Firewall (LAN clients) =====
if (-not $NoFirewall) {
    $exe = Join-Path $Prefix "xmouse.exe"
    $rule = "advfirewall firewall add rule name=xmouse dir=in action=allow program=`"$exe`" protocol=TCP localport=$Port profile=private,domain"
    $discovery = "advfirewall firewall add rule name=xmouse dir=in action=allow program=`"$exe`" protocol=UDP localport=47901 profile=private,domain"
    $moves = "advfirewall firewall add rule name=xmouse dir=in action=allow program=`"$exe`" protocol=UDP localport=$Port profile=private,domain" # encrypted udp of the moves
    try {
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList "advfirewall firewall delete rule name=xmouse"
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList $rule
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList $discovery
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList $moves
        Say "port $Port opened for the private networks"
    } catch { Say "firewall not changed (administrator refused): the members on the LAN may be blocked" }
}
Say "installed: $(& (Join-Path $Prefix 'xmouse.exe') --version)"
