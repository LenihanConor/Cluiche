# Build environment for Cluiche — Windows Server Core with VS Build Tools + Python 3.11
FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]

# Install Chocolatey
RUN Set-ExecutionPolicy Bypass -Scope Process -Force; \
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; \
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Install Python 3.11
RUN choco install python --version=3.11.0 -y --no-progress
RUN python --version

# Install VS Build Tools (C++ workload)
RUN choco install visualstudio2022buildtools -y --no-progress \
    --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools \
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
    --includeRecommended --passive --norestart"

# Verify VCTools are present
RUN if (-not (Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC")) { \
    Write-Error "VCTools not found"; exit 1 }

# Install vs_buildtools marker check
RUN Write-Output "vs_buildtools installed"

WORKDIR C:\workspace
