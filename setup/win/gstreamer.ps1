#  save as UTF-8 with BOM

# Launch PowerShell x86 as administrator
# Set-ExecutionPolicy RemoteSigned
# Set-ExecutionPolicy Default

function IsGstLaunch {
    # Check via gst-launch-1.0
    try {
        $versionOutput = & gst-launch-1.0 --version 2>$null
        if ($LASTEXITCODE -eq 0 -or $versionOutput) {
            Write-Host "`n✅ GStreamer detected via 'gst-launch-1.0 --version'" -ForegroundColor Yellow
            Write-Host $versionOutput
            return $true
        }
        return $false
    }
    catch {
        return $false
    }
}

function IsGstEnv {
    # Check environment variables
    $envVars = [System.Environment]::GetEnvironmentVariables('Machine')
    $matchEnvVars = @{}
    foreach ($key in $envVars.Keys) {
        if ($key -match '(?i)gstreamer') {
            $matchEnvVars[$key] = $envVars[$key]
        } else {
            if ($envVars[$key] -match '(?i)gstreamer') {
                $splitEnvVars = $envVars[$key] -split ';'
                $gstEnvVars = @()
                foreach ($envVar in $splitEnvVars) {
                    if ($envVar -match '(?i)gstreamer') {
                        $gstEnvVars += $envVar
                        $gstEnvVars += ";"
                    }
                }
                $matchEnvVars[$key] = $gstEnvVars
            }
        }
    }

    if ($matchEnvVars.Count -ne 0) {
        Write-Host "✅ Found GStreamer-related system environment variables:" -ForegroundColor Yellow
        foreach ($key in $matchEnvVars.Keys) {
            Write-Host "${key}: $($matchEnvVars[$key])"
        }
        return $true
    }
    return $false
}

function deleteGstEnv {
    # system environmental variables
    $envVars = [System.Environment]::GetEnvironmentVariables('Machine')
    $gstVars = @{}
    foreach ($key in $envVars.Keys) {
        if ($key -match '(?i)gstreamer') {
            $gstVars[$key] = $envVars[$key]
        }
    }
    foreach ($var in $gstVars.Keys) {
        $response = Read-Host "`nDo you want to remove the ${var}: $($gstVars[$var])? (Y/N)"
        if ($response -match '^[Yy]$') {
            [System.Environment]::SetEnvironmentVariable($var, $null, [System.EnvironmentVariableTarget]::Machine)
        } else {
            return $false
        }
    }

    # path
    $currentPath = [System.Environment]::GetEnvironmentVariable("Path", [System.EnvironmentVariableTarget]::Machine)
    $splitPath = $currentPath -split ';'
    $gstPath = @()
    foreach ($var in $splitPath) {
        if ($var -match '(?i)gstreamer') {
            $gstPath += $var
        }
    }

    if ($gstPath.Count -ne 0) {
        $response = Read-Host "`nDo you want to remove the $gstPath from PATH? (Y/N)"
        if ($response -match '^[Yy]$') {
            $filteredPath = $splitPath | Where-Object {
                $path = $_.Trim()
                -not ($gstPath -contains $path)
            }

            $newPath = ($filteredPath -join ';').TrimEnd(';')
            [System.Environment]::SetEnvironmentVariable("Path", $newPath, [System.EnvironmentVariableTarget]::Machine)
        }
        else {
            return $false
        }
    }

    return $true
}
function GetGStreamerInstallPath {
    $possibleRoots = @(
        "C:\Program Files\gstreamer",
        "C:\Program Files (x86)\gstreamer"
    )

    foreach ($root in $possibleRoots) {
        $match = Get-ChildItem -Path $root -Recurse -Filter "gst-launch-1.0.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($match) {
            return $match.DirectoryName
        }
    }

    Write-Warning "GStreamer executable not found in known locations."
    return $null
}

function SetGStreamerEnvVars {
    param (
        [string]$installPath
    )

    $pathVars = (
        "$installPath\bin",
        "$installPath\lib\gstreamer-1.0",
        "$installPath\lib"
    )
    Write-Host "`n📦 Set system environment variables..." -ForegroundColor Cyan

    # PATH
    $currentPath = [System.Environment]::GetEnvironmentVariable("PATH", [System.EnvironmentVariableTarget]::Machine)
    $tailPath = @()
    foreach ($path in $pathVars) {
        if ($currentPath -notcontains $path) {
            $response = Read-Host "`nDo you want to add $path to PATH? (Y/N)"
            if ($response -match '^[Yy]$') {
                $tailPath += $path
                $tailPath += ";"
                Write-Host "Adding to PATH: $path" -ForegroundColor Cyan
            }
            else {
                Write-Host "Add $path to PATH manually!" -ForegroundColor Red
            }
        } else {
            Write-Host "Already in PATH: $path" -ForegroundColor Green
        }
    }

    Write-Host "`nTAIL: $tailPath"

    if ($tailPath.Count -ne 0) {
        [System.Environment]::SetEnvironmentVariable("PATH", "$currentPath;$tailPath", [System.EnvironmentVariableTarget]::Machine)
    }

    # system environment variables
    # $systemEnvVars = @{
    #     "GSTREAMER_1_0_ROOT_MINGW_X86_64" = $installPath
    #     "GSTREAMER_1_0_ROOT_MSVC_X86_64" = $installPath
    #     "GSTREAMER_1_0_ROOT_X86_64" = $installPath
    #     "GSTREAMER_DIR" = $installPath
    # }
    
    # $envVars = [System.Environment]::GetEnvironmentVariables('Machine')
    # foreach ($key in $systemEnvVars.Keys) {
    #     $value = $systemEnvVars[$key]
    #     $existingValue = [System.Environment]::GetEnvironmentVariables('Machine').Keys | Where-Object { $_ -eq $key }
    #     if (-not $existingValue) {
    #         $response = Read-Host "`nDo you want to add $key to system environment variables? (Y/N)"
    #         if ($response -match '^[Yy]$') {
    #             Write-Host "Adding ${key}: $value to system environment variables..." -ForegroundColor Cyan
    #             [System.Environment]::SetEnvironmentVariable($key, $value, [System.EnvironmentVariableTarget]::Machine)
    #         }
    #         else {
    #             Write-Host "Add ${key}: $value to system environment variables manually!" -ForegroundColor Green
    #         }
    #     }
    #     else {
    #         Write-Host "$key already exists in system environment variables." -ForegroundColor Green
    #     }
    # }
}

function InstallGStreamer {
    $versions = @{
        "1" = @{
            Name    = "GStreamer 1.24.12 (MSVC) - STABLE"
            Url     = "https://gstreamer.freedesktop.org/data/pkg/windows/1.24.12/msvc/gstreamer-1.0-msvc-x86_64-1.24.12.msi"
            UrlDev  = "https://gstreamer.freedesktop.org/data/pkg/windows/1.24.12/msvc/gstreamer-1.0-devel-msvc-x86_64-1.24.12.msi"
        }
        "2" = @{
            Name = "GStreamer 1.26.0 (MSVC)"
            Url  = "https://gstreamer.freedesktop.org/data/pkg/windows/1.26.0/msvc/gstreamer-1.0-msvc-x86_64-1.26.0.msi"
            UrlDev  = "https://gstreamer.freedesktop.org/data/pkg/windows/1.26.0/msvc/gstreamer-1.0-devel-msvc-x86_64-1.26.0.msi"
        }
    }

    Write-Host "`n🎛️ Choose GStreamer version to install:" -ForegroundColor Cyan
    $versions.GetEnumerator() | ForEach-Object {
        Write-Host "[$($_.Key)] $($_.Value.Name)"
    }

    do {
        $choice = Read-Host "Enter number [1-2]"
    } while (-not $versions.ContainsKey($choice))

    $selected = $versions[$choice]
    $installerUrl = $selected.Url
    $installerFile = "$env:TEMP\$(Split-Path $installerUrl -Leaf)"
    $installerUrlDev = $selected.UrlDev
    $installerFileDev = "$env:TEMP\$(Split-Path $installerUrlDev -Leaf)"

    Write-Host "`n⬇️ Downloading $($selected.Name)..." -ForegroundColor Cyan
    # download if file not exist
    if (-Not (Test-Path -Path $installerFile)) {
        Invoke-WebRequest -Uri $installerUrl -OutFile $installerFile
    }
    else {
        Write-Host "`nFile already exists: $installerFile"
    }
    # if (-Not (Test-Path -Path $installerFileDev)) {
    #     Invoke-WebRequest -Uri $installerUrlDev -OutFile $installerFileDev
    # }
    # else {
    #     Write-Host "`nFile already exists: $installerFileDev"
    # }

    Write-Host "`n📦 Launching installer..." -ForegroundColor Cyan
    Start-Process "msiexec.exe" -ArgumentList "/i `"$installerFile`" /qn /norestart INSTALLDIR=`"C:\Program Files\gstreamer`"" -Wait -Verb RunAs

    # Write-Host "`n📦 Launching installer (dev packages)..." -ForegroundColor Cyan
    # Start-Process "msiexec.exe" -ArgumentList "/i `"$installerFileDev`" /qn /norestart INSTALLDIR=`"C:\Program Files\gstreamer`"" -Wait -Verb RunAs

    $installPathBin = GetGStreamerInstallPath
    $installPath = Split-Path $installPathBin -Parent
    Write-Host "`n✅ Installation complete! GStreamer was installed to $installPath" -ForegroundColor Green

    # set PATH variables
    SetGStreamerEnvVars  -installPath $installPath

    Remove-Item -Path $installerFile -Force -ErrorAction SilentlyContinue
    # Remove-Item -Path $installerFileDev -Force -ErrorAction SilentlyContinue
}

# Main
Write-Host "========== GStreamer Installer ==========" -ForegroundColor Magenta

# check installation
Write-Host "`n🔍 Checking for existing GStreamer installation..." -ForegroundColor Cyan

if (IsGstLaunch) {
    Write-Host "`n⚠️ GStreamer appears to be already installed. Please uninstall it manually before running this script again." -ForegroundColor Red
    return
}

if (IsGstEnv) {
    Write-Host "`n⚠️ GStreamer system environment variables appear to be set" -ForegroundColor Red
    $response = Read-Host "`nDo you want to remove the GStreamer environment variables now? (Y/N)"
    if ($response -match '^[Yy]$') {
        if (deleteGstEnv) {
            Write-Host "`n✅ GStreamer system environment variables were successfully removed. Please reboot PC and run this script again." -ForegroundColor Green
        }
        else {
            Write-Host "`n⚠️ Please clean GStreamer system environment variables manually before running this script again." -ForegroundColor Red
        }
    }
    else {
        Write-Host "`n⚠️ Please clean GStreamer system environment variables manually before running this script again." -ForegroundColor Red
    }

    return
}

# install gstreamer
Write-Host "❌ GStreamer not detected. Starting installation..." -ForegroundColor Yellow
InstallGStreamer


