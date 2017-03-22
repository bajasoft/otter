# Title: PowerShell package build script
# Description: Build packages with installation
# Type: script
# Author: bajasoft <jbajer@gmail.com>
# Version: 0.5

# How to run PowerShell scripts: http://technet.microsoft.com/en-us/library/ee176949.aspx

# Command line parameters
# [-build] - make Release build
# [-cmake] - run CMake
# [-pack] - create packages for distribution
# [-file] - set name of json file to parse. Example: -f name.of.file

Param(
[switch]$build,
[switch]$cmake,
[switch]$pack,
[string]$file
)

# Main-function
function main 
{   
    initGlobalVariables

    # run CMake only when requested by user
    if (($cmake -or !(Test-Path $Global:buildPath)) -and (Test-Path $Global:cmakePath))
    {
        Write-Host "Running CMake..."

        Get-ChildItem -Path $Global:buildPath -Include * -File -Recurse | foreach { $_.Delete()}

        foreach ($argument in $Global:cmakeArguments) # Adding arguments for CMake
        {
            $arguments += " -D" + $argument
        }

        $arguments += " " + $Global:projectPath

        # Results from CMake are written to stderr.txt and stdout.txt in script directory
        $process = Start-Process -NoNewWindow -passthru -WorkingDirectory $Global:buildPath -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:cmakePath $arguments
        $process.WaitForExit();

        testResults
    }

    if ($Global:architecture -ne "win64")
    {
        $Global:architecture = "win32"
    }

    $Global:packageName = $Global:packageName + "-" + $Global:architecture + "-" + $Global:contextVersion;

    # Run build if required
    if ($build -and (Test-Path $Global:compilerPath) -and (Test-Path $Global:buildPath))
    {
        Write-Host "Building solution..."

        if ($Global:cmakeArguments.Contains("MinGW"))
        {
            Start-Process -NoNewWindow -Wait -WorkingDirectory $Global:buildPath -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:compilerPath  -ArgumentList "-j4"
        }
        else
        {
            Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:compilerPath -ArgumentList "/p:Configuration=Release", ($Global:buildPath + "otter-browser.sln")
        }
        
        testResults

        Write-Host "Copying executable..."

        if (!(Test-Path $Global:packageInputPath))
        {
            New-Item $Global:packageInputPath -type directory
        }

        if (!(Test-Path $Global:PDBOutputPath))
        {
            New-Item $Global:PDBOutputPath -type directory
        }

        Copy-Item ($Global:buildPath + "Release/*") $Global:packageInputPath
        Copy-Item ($Global:buildPath + "Release/*.pdb") ($Global:PDBOutputPath + $Global:packageName + ".pdb")
    }

    # Run packaging if required
    if ($pack)
    {
        # Set values to Inno Setup script
        if (Test-Path $Global:innoScriptPath) 
        {
            Write-Host "Preparing Inno Setup script..."

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
                 -replace "LicenseFile=.+", ("LicenseFile=$Global:packageInputPath" + "COPYING")`
                 -replace "OutputDir=.+", "OutputDir=$Global:packageOutputPath"`
                 -replace "VersionInfoVersion=.+", "VersionInfoVersion=$Global:mainVersion"`
                 -replace ("Source:.*; + """), ("Source: " + """$Global:packageInputPath*""" + ";")
            } |
            Set-Content $Global:innoScriptPath
        }
        else
        {
            Write-Host "Inno Setup script not found, skipping..." -foregroundcolor red
        }

        # Do full build
        preparePackages
    }

    #Remove-Item stdout.txt
    Remove-Item stderr.txt

    Write-Host "Finished!"
}

function preparePackages
{
    Write-Host "Packaging..."

    # Inno Setup
    if ((Test-Path $Global:innoBinaryPath) -and (Test-Path $Global:innoScriptPath))
    {
         Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:innoBinaryPath -ArgumentList $Global:innoScriptPath

         testResults
    }
    else
    {
        Write-Host "Building installer skipped, script or Inno binary not found..." -foregroundcolor red
    }

    # 7Zip
    if (Test-Path $Global:zipBinaryPath)
    {
        Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:zipBinaryPath -ArgumentList "a", ($Global:packageOutputPath + $Global:packageName + ".7z"), ($Global:packageInputPath + "*"), "-mmt4", "-mx9", "-m0=lzma2"
        
        testResults
        
        Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:zipBinaryPath -ArgumentList "a", "-tzip", ($Global:packageOutputPath + $Global:packageName + ".zip"), $Global:packageInputPath, "-mx5"
        
        testResults
    }
    else
    {
        Write-Host "Building archives skipped, 7zip not found..." -foregroundcolor red
    }
}

# Prints error from output and closes script
function testResults
{
    $errContent = (Get-Content stderr.txt)

    If ($errContent -ne $Null)
    {
        Write-Host "`n"
        
        $errContent
        
        Write-Host "`nExiting due to critical error! Please check stdout.txt for more info" -ForegroundColor Red

        exit
    }
}

function initGlobalVariables
{
    Write-Host "Initializing global variables..."

    $jsonFile = if ($file) {$file} else {".\otter-browser.json"}

    $json = (Get-Content $jsonFile -Raw) | ConvertFrom-Json
    
    $Global:cmakePath = If($json.cmakePath) {$json.cmakePath} Else {"C:\Program Files (x86)\CMake\bin\cmake.exe"}
    $Global:cmakeArguments = If($json.cmakeArguments) {$json.cmakeArguments} Else {""}
    $Global:projectPath = If($json.projectPath) {$json.projectPath} Else {"C:\develop\github\otter\"}
    $Global:packageOutputPath = If($json.packageOutputPath) {$json.packageOutputPath} Else {"C:\develop\github\"}
    $Global:packageInputPath = If($json.packageInputPath) {$json.packageInputPath} Else {"C:\downloads\Otter\"}
    $Global:PDBOutputPath = If($json.PDBOutputPath) {$json.PDBOutputPath} Else {"C:\develop\PDBs\"}
    $Global:zipBinaryPath = If($json.zipBinaryPath) {$json.zipBinaryPath} Else {"C:\Program Files\7-Zip\7z.exe"}
    $Global:innoBinaryPath = If($json.innoBinaryPath) {$json.innoBinaryPath} Else {"C:\Program Files (x86)\Inno Setup 5\ISCC.exe"}
    $Global:innoScriptPath = If($json.innoScriptPath) {$json.innoScriptPath} Else {".\otter-browser.iss"}
    $Global:compilerPath = If($json.compilerPath) {$json.compilerPath} else {"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"}
    $Global:buildPath = If($json.buildPath) {$json.buildPath} else {"C:\develop\github\otter-browser\"}
    $Global:mainVersion = If($json.mainVersion) {$json.mainVersion} Else {"1.0"}
    $Global:contextVersion = If($json.contextVersion) {$json.contextVersion} Else {"dev"}
    $Global:architecture = If($json.architecture) {$json.architecture} Else {"64"}
    $Global:packageName = If($json.packageName) {$json.packageName} Else {"otter-browser"}
}

# Entry point
main
