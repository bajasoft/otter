# Title: PowerShell package build script
# Description: Build packages with installation
# Type: script
# Author: bajasoft <jbajer@gmail.com>
# Version: 0.4

# How to run PowerShell scripts: http://technet.microsoft.com/en-us/library/ee176949.aspx

# Command line parameters
# [-build] - make Release build
# [-cmake] - run CMake
# [-file] - set name of json file to parse. Example: -f name.of.file

Param(
[switch]$build,
[switch]$cmake,
[string]$file
)

# Main-function
function main 
{   
    initGlobalVariables

    # run CMake only when required by user
    if ($cmake -and (Test-Path $Global:cmakePath))
    {
        Write-Host "Running CMake..."

        Get-ChildItem -Path $Global:solutionPath -Include * -File -Recurse | foreach { $_.Delete()}

        foreach ($argument in $Global:cmakeArguments) # Adding arguments for CMake
        {
            $arguments += " -" + $argument
        }

        $arguments += " -G `"" + $Global:cmakeCompiler + "`" " + $Global:projectPath

        # Results from CMake are written to stderr.txt and stdout.txt in script directory
        Start-Process -NoNewWindow -Wait -WorkingDirectory $Global:solutionPath -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:cmakePath $arguments

        testResults
    }

    if ($Global:architecture -ne "win64")
    {
        $Global:architecture = "win32"
    }

    # Run build if required
    if ($build -and (Test-Path $Global:compilerPath) -and (Test-Path $Global:solutionPath))
    {
        Write-Host "Building solution..."

        if ($Global:cmakeCompiler.Contains("MinGW"))
        {
            Start-Process -NoNewWindow -Wait -WorkingDirectory $Global:solutionPath -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:compilerPath  -ArgumentList "-j4"
        }
        else
        {
            Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:compilerPath -ArgumentList "/p:Configuration=Release", ($Global:solutionPath + "otter-browser.sln")
        }
        
        testResults

        Write-Host "Copy executable..."

        Copy-Item ($Global:solutionPath + "Release/*") $Global:packageInputPath
    }

    $Global:packageName = $Global:packageName + "-" + $Global:architecture + "-" + $Global:contextVersion;

    # Do full build
    preparePackages

    Remove-Item stdout.txt
    Remove-Item stderr.txt

    Write-Host "Finished!"
}

function preparePackages
{
    Write-Host "Packaging..."

    # 7Zip
    if (Test-Path $Global:zipBinaryPath)
    {
        Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:zipBinaryPath -ArgumentList "a", ($Global:packageOutputPath + $Global:packageName + ".7z"), ($Global:packageInputPath + "*"), "-mmt4", "-mx9", "-m0=lzma2"
        
        testResults
        
        #Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt $Global:zipBinaryPath -ArgumentList "a", "-tzip", ($Global:packageOutputPath + $Global:packageName + ".zip"), $Global:packageInputPath, "-mx5"
        
        #testResults
    }
    else
    {
        Write-Host "Building archives skipped, 7zip not found..." -foregroundcolor red
    }

    # Installer
    if (Test-Path $Global:binnaryCreatorPath)
    {
        prepareInstallerFiles

        prepareConfig

        Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt ($Global:binnaryCreatorPath + "binarycreator.exe") -ArgumentList "-n", "-t", ($Global:binnaryCreatorPath + "installerbase.exe"), "-c", $Global:mainPackageConfig, "-p", ($Global:binaryCreatorInputPath + "packages"), ($Global:packageOutputPath + $Global:packageName)
    
        testResults

        Copy-Item ($Global:packageOutputPath + $Global:packageName + ".7z") $dataPackageFolder
        
        Start-Process -NoNewWindow -Wait -RedirectStandardError stderr.txt -RedirectStandardOutput stdout.txt ($Global:binnaryCreatorPath + "repogen.exe") -ArgumentList "--update", "-p", ($Global:binaryCreatorInputPath + "packages"), ($Global:packageOutputPath + "repository")
    
        testResults
    }
    else
    {
        Write-Host "Building installer skipped, QtIF or config files not found..." -foregroundcolor red
    }
}

#Moves required files to installer and output folder
function prepareInstallerFiles
{
$Global:binaryCreatorInputPath

    if (-Not (Test-Path $Global:binaryCreatorInputPath))
    {
        Copy-Item ($Global:projectPath + "packaging\otter-browser") $Global:binaryCreatorInputPath -Recurse
    }

        if (Test-Path $dataPackageFolder)
        {
            Remove-Item ($dataPackageFolder + "*")
        }
        else
        {
            New-Item $dataPackageFolder -Type Directory
        }

        $licence = ($Global:binaryCreatorInputPath + "packages\com.otter.root\meta\COPYING")

        if (-Not (Test-Path $licence))
        {
            Copy-Item ($Global:projectPath + "COPYING") $licence
        }

        $icon = ($Global:binaryCreatorInputPath + "config\otter-browser.png")

        if (-Not (Test-Path $icon))
        {
            Copy-Item ($Global:projectPath + "resources\icons\otter-browser.png") $icon
        }
}

#Sets values to xml config
function prepareConfig
{
    $version = If ($Global:contextVersion -like "*weekly*") {$Global:contextVersion -replace "weekly", ""} Else {"0"}

    [xml]$myXML = Get-Content $Global:otterPackageConfig
    $myXML.Package.Version = ($Global:mainVersion + "." + $version)
    $myXML.Package.ReleaseDate = (Get-Date -UFormat "%Y-%m-%d").ToString()
    $myXML.Package.DownloadableArchives = ($Global:packageName + ".7z")

    #$myXML.Package.Licenses.License.file = ($Global:projectPath + "COPYING")

    $myXML.Save($Global:otterPackageConfig)
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
    $Global:cmakeCompiler = If($json.cmakeCompiler) {$json.cmakeCompiler} Else {"Visual Studio 14 2015 Win64"}
    $Global:cmakeArguments = If($json.cmakeArguments) {$json.cmakeArguments} Else {""}
    $Global:projectPath = If($json.projectPath) {$json.projectPath} Else {"C:\develop\github\otter\"} # Root path of the project with source codes, resources, cmakefiles etc.
    $Global:packageOutputPath = If($json.packageOutputPath) {$json.packageOutputPath} Else {"C:\develop\github\"} # Path containing finished packages together with update repository
    $Global:packageInputPath = If($json.packageInputPath) {$json.packageInputPath} Else {"C:\downloads\Otter\"} # Folder with builded application and all dependencies
    $Global:zipBinaryPath = If($json.zipBinaryPath) {$json.zipBinaryPath} Else {"C:\Program Files\7-Zip\7z.exe"}
    $Global:binnaryCreatorPath = If($json.binnaryCreatorPath) {$json.binnaryCreatorPath} Else {"C:\develop\Qt\QtIFW2.0.3\bin\"}
    $Global:compilerPath = If($json.compilerPath) {$json.compilerPath} else {"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"}
    $Global:solutionPath = If($json.solutionPath) {$json.solutionPath} else {"C:\develop\github\otter-browser\"} # Folder with actual solution to build (mostly separated from project path)
    $Global:mainVersion = If($json.mainVersion) {$json.mainVersion} Else {"1.0"}
    $Global:contextVersion = If($json.contextVersion) {$json.contextVersion} Else {"dev"}
    $Global:architecture = If($json.architecture) {$json.architecture} Else {"64"}
    $Global:packageName = If($json.packageName) {$json.packageName} Else {"otter-browser"}

    # Resolve rest of the used paths
    $Global:binaryCreatorInputPath = ($Global:packageOutputPath + "otter-browser\")
    $Global:dataPackageFolder = ($Global:binaryCreatorInputPath + "packages\com.otter.root\data\")
    $Global:mainPackageConfig = ($Global:binaryCreatorInputPath + "config\otter-browser.xml")
    $Global:otterPackageConfig = ($Global:binaryCreatorInputPath + "packages\com.otter.root\meta\package.xml")
}

# Entry point
main
