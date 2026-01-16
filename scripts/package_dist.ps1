$ErrorActionPreference = "Stop"

# Get the root directory relative to script location (scripts/../)
$RootDir = Resolve-Path "$PSScriptRoot/.."
$BuildDir = "$RootDir/build/Release"
$DistDir = "$RootDir/dist"

Write-Host "Packaging Syllable Detector Library..."
Write-Host "Source: $BuildDir"
Write-Host "Dest:   $DistDir"

# Clean dist
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Force -Path "$DistDir/bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$DistDir/lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$DistDir/include" | Out-Null
New-Item -ItemType Directory -Force -Path "$DistDir/wrappers" | Out-Null

# Copy Binaries
Copy-Item "$BuildDir/syllable.dll" "$DistDir/bin/"
Copy-Item "$BuildDir/syllable.lib" "$DistDir/lib/"
if (Test-Path "$BuildDir/syllable.pdb") {
    Copy-Item "$BuildDir/syllable.pdb" "$DistDir/bin/"
}

# Copy Headers
Copy-Item "$RootDir/include/syllable_detector.h" "$DistDir/include/"

# Copy Wrappers
Copy-Item -Recurse "$RootDir/wrappers/*" "$DistDir/wrappers/"

Write-Host "Success! artifacts are in $DistDir"
