<#
=====================================================================
 ensure-python.ps1  -  Detect a usable Python 3, installing one if
 none is found, then print its path/command on stdout.

 Used by the Windows launcher (new-nrl-project.bat).
 A brand-new student who has never installed Python can double-click the
 launcher and have it "just work": this script installs Python 3 via
 winget (built into Win10/11) and falls back to the official python.org
 silent installer on older Windows.

 Contract:
   - stdout : exactly ONE line, the interpreter to run (e.g. a full path
              to python.exe, or "py" when the launcher is on PATH).
   - stderr : all human-readable progress / status / error text.
   - exit 0 : interpreter found/installed; its path is on stdout.
   - exit 1 : no interpreter could be found or installed.
=====================================================================
#>
$ErrorActionPreference = 'Stop'

# Anything we want the user to read goes to stderr so stdout stays clean
# for the single interpreter path the .bat captures.
function Say([string]$msg) { [Console]::Error.WriteLine($msg) }

# Emit the chosen interpreter on stdout and exit success.
function Emit([string]$exe) {
    Write-Output $exe
    exit 0
}

# Resolve a candidate to its real python.exe IFF it is a Python 3 that ALSO
# has tkinter -- the wizard's folder picker needs it. Prints the resolved
# path, or $null when the candidate is missing / not Python 3 / has no tkinter.
function Resolve-Usable([string]$exe, [string[]]$prefix) {
    # Probe inside a job with a timeout. A Microsoft Store "python.exe" alias
    # stub (present by default when no real Python is installed) HANGS when
    # invoked -- it tries to open the Store -- so a plain call could freeze the
    # launcher. An installed interpreter prints its path in well under a second;
    # anything that hasn't answered in time is treated as unusable.
    $job = $null
    try {
        $job = Start-Job -ScriptBlock {
            param($e, $p)
            $o = & $e @p -c "import sys, tkinter; print(sys.executable)" 2>$null
            if ($LASTEXITCODE -eq 0) { $o }
        } -ArgumentList $exe, $prefix
        if (Wait-Job $job -Timeout 30) {
            $out = Receive-Job $job
            if ($out) {
                return ([string]($out | Where-Object { $_ } | Select-Object -Last 1)).Trim()
            }
        } else {
            Stop-Job $job -ErrorAction SilentlyContinue
        }
    } catch {
    } finally {
        if ($job) { Remove-Job $job -Force -ErrorAction SilentlyContinue }
    }
    return $null
}

# ----- 1) Detect an existing, USABLE interpreter ---------------------------
# We require a "real" Python 3 WITH tkinter. PlatformIO's bundled Python is
# deliberately NOT accepted here: it ships without tkinter, so the wizard's
# folder picker would silently break. When only that exists we install a real
# Python below instead.
function Find-Python {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        $p = Resolve-Usable 'py' @('-3'); if ($p) { return $p }
    }
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        $p = Resolve-Usable $python.Source @(); if ($p) { return $p }
    }
    return $null
}

# Search the well-known per-user install locations for an interpreter that a
# fresh install just dropped (PATH is NOT refreshed in this session). Each
# candidate is validated with Resolve-Usable so we only return a tkinter-
# capable Python 3.
function Find-FreshlyInstalled {
    $roots = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Python'),   # python.org / winget per-user
        (Join-Path $env:LOCALAPPDATA 'Python'),            # Windows Python Install Manager (pythoncore-*)
        'C:\Program Files\Python*',
        'C:\Python3*'
    )
    foreach ($root in $roots) {
        $exes = Get-ChildItem -Path $root -Filter 'python.exe' -Recurse -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending
        foreach ($exe in $exes) {
            $p = Resolve-Usable $exe.FullName @(); if ($p) { return $p }
        }
    }
    return $null
}

# Discover the latest STABLE Python from python.org's release index, verifying
# that the Windows amd64 installer actually exists for it (newest first). This
# keeps us on the current release instead of a hard-coded, ageing version.
# Returns @{ Version='3.14.6'; Minor='3.14'; Url='https://.../python-3.14.6-amd64.exe' } or $null.
function Get-LatestPython {
    try {
        $ProgressPreference = 'SilentlyContinue'
        $idx = Invoke-WebRequest -Uri 'https://www.python.org/ftp/python/' -UseBasicParsing -TimeoutSec 20
        # Match only pure X.Y.Z folders -> pre-releases like 3.15.0a1/ are excluded.
        $vers = [regex]::Matches($idx.Content, 'href="(\d+\.\d+\.\d+)/"') |
                ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique |
                Sort-Object { [version]$_ } -Descending
        foreach ($v in ($vers | Select-Object -First 12)) {
            $url = "https://www.python.org/ftp/python/$v/python-$v-amd64.exe"
            try {
                $h = Invoke-WebRequest -Uri $url -Method Head -UseBasicParsing -TimeoutSec 15
                if ($h.StatusCode -eq 200) {
                    return @{ Version = $v; Minor = (($v -split '\.')[0..1] -join '.'); Url = $url }
                }
            } catch {}   # no binary installer for this version (e.g. security-only) -> try next
        }
    } catch {}
    return $null
}

$found = Find-Python
if ($found) { Emit $found }

# ----- 2) No usable Python found: install one ------------------------------
# (Reached when there is no Python at all, OR only PlatformIO's bundled Python,
#  which lacks the tkinter the wizard's folder picker needs.)
Say ''
Say '[setup] No usable Python 3 (with tkinter) was found on this PC.'
Say '[setup] Installing Python 3 automatically (one-time, ~1-2 min)...'
Say ''

$installed = $false

# Discover the LATEST stable Python from python.org's release listing, so we
# don't pin to an ageing version. Returns @{Version; Minor; Url} or $null.
$latest = Get-LatestPython
if ($latest) { Say "[setup] Latest stable Python is $($latest.Version)." }

# a) Preferred: winget (built into Windows 10 1709+/11, no admin needed).
#    Target the latest minor line (e.g. Python.Python.3.14) so we don't pin
#    to an old one; fall back to a recent id if discovery failed.
$winget = Get-Command winget -ErrorAction SilentlyContinue
if ($winget) {
    $wingetId = if ($latest) { "Python.Python.$($latest.Minor)" } else { 'Python.Python.3.13' }
    Say "[setup] Using winget to install $wingetId ..."
    try {
        # Route winget's chatter to stderr so stdout stays a clean interpreter path.
        & winget install -e --id $wingetId --scope user `
            --accept-source-agreements --accept-package-agreements 2>&1 |
            ForEach-Object { Say ([string]$_) }
        if ($LASTEXITCODE -eq 0) { $installed = $true }
        else { Say "[setup] winget exited with code $LASTEXITCODE; trying the python.org installer..." }
    } catch {
        Say "[setup] winget failed ($($_.Exception.Message)); trying the python.org installer..."
    }
}

# b) Fallback: download the official python.org installer (latest) silently.
if (-not $installed) {
    $ver = if ($latest) { $latest.Version } else { '3.13.1' }
    $url = if ($latest) { $latest.Url } else { "https://www.python.org/ftp/python/$ver/python-$ver-amd64.exe" }
    $dest = Join-Path $env:TEMP "python-$ver-amd64.exe"
    Say "[setup] Downloading the official Python $ver installer..."
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
        Say '[setup] Running the installer silently (per-user)...'
        $p = Start-Process -FilePath $dest -Wait -PassThru -ArgumentList `
            '/quiet','InstallAllUsers=0','PrependPath=1','Include_launcher=1'
        if ($p.ExitCode -eq 0) { $installed = $true }
        else { Say "[setup] Installer exited with code $($p.ExitCode)." }
    } catch {
        Say "[setup] Could not download/run the python.org installer: $($_.Exception.Message)"
    }
}

if (-not $installed) {
    Say ''
    Say '[error] Automatic install of Python 3 failed.'
    Say '   Please install Python 3 from https://www.python.org/downloads/'
    Say '   and tick "Add python.exe to PATH" during setup, then re-run.'
    exit 1
}

# ----- 3) Re-resolve the just-installed interpreter (PATH not refreshed) ----
$fresh = Find-FreshlyInstalled
if (-not $fresh) {
    # winget may have updated this process's environment block; try detection again.
    $fresh = Find-Python
}
if ($fresh) {
    Say '[setup] Python 3 installed successfully.'
    # The installer adds Python to PATH, but already-open terminals (and new tabs
    # of an existing one) keep the OLD PATH. Tell the user so `python` "not found"
    # in a stale window isn't mistaken for a failed install. The wizard itself
    # runs fine right now -- it uses the full path below, not PATH.
    Say "[setup] To use the 'python' command yourself, open a NEW terminal window."
    Emit $fresh
}

Say ''
Say '[error] Python 3 was installed but could not be located automatically.'
Say '   Please close this window and re-run -- it should be found now.'
exit 1
