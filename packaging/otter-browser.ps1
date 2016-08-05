# Title: PowerShell package build script
# Description: Build packages with installation
# Type: script
# Author: bajasoft <jbajer@gmail.com>
# Version: 0.2

# How to run PowerShell scripts: http://technet.microsoft.com/en-us/library/ee176949.aspx

# Command line parameters
# [-o] - set output file name (otter-browser.ps1 -o nameOfFile)
# [-a] - set architecture of binaries (Argument: 32/64)
# [-t] - type of build, default is weekly. Ignored if -a is included (Argument: weekly/beta/release)

Param(
  [int]$a,
  [string]$o,
  [string]$t
)

# Global values
$Global:outputPath = "C:\develop\github\"
$Global:inputPath = "C:\downloads\Otter\"
$Global:7ZipBinaryPath = "C:\Program Files\7-Zip\7z.exe"
$Global:innoBinaryPath = "C:\Program Files (x86)\Inno Setup 5\ISCC.exe"
$Global:innoScriptPath = ".\otter-browser.iss"
$Global:updateScriptPath = ".\otter-browser.rb"
$Global:rubyPath = "C:\Ruby22-x64\bin\ruby.exe"
$Global:configPath = ".\otter\config.h"
$Global:updateConfiguration = ".\otter-browser-update-win.json"

# Main-function
function main 
{   
    "Packaging..."

    # Find version information
    ForEach ($line in Get-Content $Global:configPath)
    {
        if ($line -like '*OTTER_VERSION_MAIN*')
        {
            $mainVersion = $line -replace '^#define OTTER_VERSION_MAIN ' -replace '"'
        }
        
        if ($line -like '*OTTER_VERSION_WEEKLY*')
        {
            $weeklyVersion = $line -replace '^#define OTTER_VERSION_WEEKLY ' -replace '"'
        }
        
        if ($line -like '*OTTER_VERSION_CONTEXT*')
        {
            $contextVersion = $line -replace '^#define OTTER_VERSION_CONTEXT ' -replace '"'
        }
    }

    # Set architecture of binaries
    if ($a -eq 64)
    {
        $architecture = 64
    }
    else
    {
        $architecture = 32
    }

    if(!$o)
    {
        # Type of build
        switch ($t)
        {
            "beta"
            {
                $packageName = "otter-browser-win" + $architecture + "-" + $contextVersion
                $setupName = "otter-browser-win" + $architecture + "-" + $contextVersion + "-setup"
            }
            "release"
            {
                #TODO
            }
            default
            {
                $packageName = "otter-browser-win" + $architecture + "-weekly" + $weeklyVersion
                $setupName = "otter-browser-win" + $architecture + "-weekly" + $weeklyVersion + "-setup"
            }
        }
    }
    else
    {
        $packageName = $o;
        $setupName =  $packageName + "-setup";
    }

    $7zipFileName = $Global:outputPath + $packageName + ".7z"
    $zipFileName = $Global:outputPath + $packageName + ".zip"

    # Set values to Inno setup script
    $content = Get-Content $Global:innoScriptPath

    $content |
    Select-String -Pattern "ArchitecturesInstallIn64BitMode=x64" -NotMatch |
    Select-String -Pattern "ArchitecturesAllowed=x64" -NotMatch |
    ForEach-Object {
       if ($architecture -eq 64 -and $_ -match "^DefaultGroupName.+")
       {
            "ArchitecturesInstallIn64BitMode=x64"
            "ArchitecturesAllowed=x64"
       }

       $_ -replace "#define MyAppVersion.+", ("#define MyAppVersion `"$mainVersion" + "$contextVersion`"") -replace "OutputBaseFilename=.+", "OutputBaseFilename=$setupName" -replace "LicenseFile=.+", ("LicenseFile=$Global:inputPath" + "COPYING") -replace "OutputDir=.+", "OutputDir=$Global:outputPath" -replace "VersionInfoVersion=.+", "VersionInfoVersion=$mainVersion"
     } |
    Set-Content $Global:innoScriptPath

    # Set package name to updater config
    $content = Get-Content $Global:updateConfiguration

    $content |
    ForEach-Object {
        $_ -replace ".*? `: \[$", "`t`t`"$packageName`" `: ["
    } |
    Set-Content $Global:updateConfiguration

    # Do full build
    fullBuild $7zipFileName $mainVersion $zipFileName

    $updateXml = ($packageName + ".xml");

    If (Test-Path $updateXml)
    {
        Remove-Item $updateXml
    }

    # Rename update script with current version/platform
    Rename-Item ($Global:outputPath + "/otter-browser-update.xml") $updateXml

    "Finished!"
}

function fullBuild ($7zipFileName, $mainVersion, $zipFileName)
{
    Start-Process -NoNewWindow -Wait $Global:innoBinaryPath -ArgumentList $Global:innoScriptPath
    Start-Process -NoNewWindow -Wait $Global:7ZipBinaryPath -ArgumentList "a", $7zipFileName, ($Global:inputPath + "*"), "-mmt4", "-mx9", "-m0=lzma2"
    Start-Process -NoNewWindow -Wait $Global:rubyPath -ArgumentList $Global:updateScriptPath, $Global:inputPath, $Global:updateConfiguration, $Global:outputPath, "-p", "windows", "-v", $mainVersion
}

# Entry point
main
