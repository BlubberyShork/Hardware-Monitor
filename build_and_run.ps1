Write-Host "Building the project..."
msbuild System_Info.sln /p:Configuration=Release
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful. Running the executable..."
    Set-Location -Path "x64\Release"
    .\System_Info.exe
} else {
    Write-Host "Build failed. Check the output above for errors."
    Pause
}
