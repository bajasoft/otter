# Title: PowerShell package build script
# Description: Build packages with installation
# Type: script
# Author: bajasoft <jbajer@gmail.com>
# Version: 0.3

# How to run PowerShell scripts: http://technet.microsoft.com/en-us/library/ee176949.aspx

# Command line parameters
# [-b] - make Release build
# [-f] - set name of json file to parse

Param(
[switch]$b,
[string]$f
)

# Main-function
function main 
{   
    initGlobalVariables

    if (!($Global:architecture -eq "win64"))
    {
        $Global:architecture = "win32"
    }

    $Global:packageName = $Global:packageName + "-" + $Global:architecture + "-" + $Global:contextVersion;

    # Set values to Inno setup script
    if (Test-Path $Global:innoScriptPath) 
    {
        Write-Host "Preparing Inno setup script..."

        $content = Get-Content $Global:innoScriptPath

        $content |
        Select-String -Pattern "ArchitecturesInstallIn64BitMode=x64" -NotMatch |
        Select-String -Pattern "ArchitecturesAllowed=x64" -NotMatch |
        ForEach-Object {
            if ($Global:architecture -eq "win64" -and $_ -match "^DefaultGroupName.+")
            {
                "ArchitecturesInstallIn64BitMode=x64"
                "ArchitecturesAllowed=x64"
            }
            
            $_ -replace "#define MyAppVersion.+", ("#define MyAppVersion `"$Global:mainVersion" + "-" + "$Global:contextVersion`"")`
             -replace "OutputBaseFilename=.+", "OutputBaseFilename=$packageName-setup"`
             -replace "LicenseFile=.+", ("LicenseFile=$Global:inputPath" + "COPYING")`
             -replace "OutputDir=.+", "OutputDir=$Global:outputPath"`
             -replace "VersionInfoVersion=.+", "VersionInfoVersion=$Global:mainVersion"`
             -replace ("Source:.*; + """), ("Source: " + """$Global:inputPath*""" + ";")
        } |
        Set-Content $Global:innoScriptPath
    }
    else
    {
        Write-Host "Inno setup script not found, skipping..." -foregroundcolor red
    }


    # Set package name to updater config
    if (Test-Path $Global:updateConfiguration) 
    {
        Write-Host "Preparing update script..."

        $content = Get-Content $Global:updateConfiguration

        $content |
        ForEach-Object {
            $_ -replace ".*? `: \[$", "`t`t`"$Global:packageName`" `: ["
        } |
        Set-Content $Global:updateConfiguration
    }

    # Run build if required
    if ($b -and (Test-Path $Global:msbuildPath) -and (Test-Path $Global:solutionPath))
    {
        Write-Host "Building solution..."

        Start-Process -NoNewWindow -Wait $Global:msbuildPath -ArgumentList "/p:Configuration=Release", $Global:solutionPath

        Write-Host "Copy executable..."

        Copy-Item ($Global:solutionPath -replace (Get-Item $Global:solutionPath).Name, "Release/*") $Global:inputPath
    }

    # Do full build
    preparePackages

    # Prepare XML for updater
    if ((Test-Path $Global:rubyPath) -and (Test-Path $Global:updateScriptPath))
    {
        $updateXml = ($packageName + ".xml");

        If (Test-Path $updateXml)
        {
            Remove-Item $updateXml
        }

        # Rename update script with current version/platform
        Rename-Item ($Global:outputPath + "/otter-browser-update.xml") $updateXml
    }

    Write-Host "Finished!"
}

function preparePackages
{
    Write-Host "Packaging..."

    # Inno Setup
    if ((Test-Path $Global:innoBinaryPath) -and (Test-Path $Global:innoScriptPath))
    {
         $process = Start-Process -NoNewWindow -Wait -PassThru $Global:innoBinaryPath -ArgumentList $Global:innoScriptPath

         if ($process.ExitCode -eq 1)
         {
            Write-Host "Invalid arguments for Inno Setup..." -foregroundcolor red
         }
         
         if ($process.ExitCode -eq 2)
         {
            Write-Host "Failed to compile setup file..." -foregroundcolor red
         }
    }
    else
    {
        Write-Host "Building installer skipped, script or Inno binary not found..." -foregroundcolor red
    }

    # 7Zip
    if (Test-Path $Global:7ZipBinaryPath)
    {
        Start-Process -NoNewWindow -Wait $Global:7ZipBinaryPath -ArgumentList "a", ($Global:outputPath + $Global:packageName + ".7z"), ($Global:inputPath + "*"), "-mmt4", "-mx9", "-m0=lzma2"
        Start-Process -NoNewWindow -Wait $Global:7ZipBinaryPath -ArgumentList "a", "-tzip", ($Global:outputPath + $Global:packageName + ".zip"), $Global:inputPath, "-mx5"
    }
    else
    {
        Write-Host "Building archives skipped, 7zip not found..." -foregroundcolor red
    }

    # Update via Ruby
    if ((Test-Path $Global:rubyPath) -and (Test-Path $Global:updateScriptPath) -and (Test-Path $Global:updateConfiguration))
    {
        Start-Process -NoNewWindow -Wait $Global:rubyPath -ArgumentList $Global:updateScriptPath, $Global:inputPath, $Global:updateConfiguration, $Global:outputPath, "-p", "windows", "-v", $Global:mainVersion
    }
    else
    {
        Write-Host "Building update skipped, update script or Ruby not found..." -foregroundcolor red
    }
}

function initGlobalVariables
{
    Write-Host "Initializing global variables..."

    $jsonFile = if ($f) {$f} else {".\otter-browser.json"}

    $json = (Get-Content $jsonFile -Raw) | ConvertFrom-Json

    $Global:outputPath = If($json.outputPath) {$json.outputPath} Else {"C:\develop\github\"}
    $Global:inputPath = If($json.inputPath) {$json.inputPath} Else {"C:\downloads\Otter\"}
    $Global:zipBinaryPath = If($json.zipBinaryPath) {$json.zipBinaryPath} Else {"C:\Program Files\7-Zip\7z.exe"}
    $Global:innoBinaryPath = If($json.innoBinaryPath) {$json.innoBinaryPath} Else {"C:\Program Files (x86)\Inno Setup 5\ISCC.exe"}
    $Global:innoScriptPath = If($json.innoScriptPath) {$json.innoScriptPath} Else {".\otter-browser.iss"}
    $Global:updateScriptPath = If($json.updateScriptPath) {$json.updateScriptPath} Else {".\otter-browser.rb"}
    $Global:rubyPath = If($json.rubyPath) {$json.rubyPath} else {"C:\Ruby22-x64\bin\ruby.exe"}
    $Global:msbuildPath = If($json.msbuildPath) {$json.msbuildPath} else {"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"}
    $Global:solutionPath = If($json.solutionPath) {$json.solutionPath} else {"C:\develop\github\otter-browser\otter.sln"}
    $Global:mainVersion = If($json.mainVersion) {$json.mainVersion} Else {"1.0"}
    $Global:contextVersion = If($json.contextVersion) {$json.contextVersion} Else {"dev"}
    $Global:updateConfiguration = If($json.updateConfiguration) {$json.updateConfiguration} Else {".\otter-browser-update-win.json"}
    $Global:architecture = If($json.architecture) {$json.architecture} Else {"64"}
    $Global:packageName = If($json.packageName) {$json.packageName} Else {"otter-browser"}
}

# Entry point
main
