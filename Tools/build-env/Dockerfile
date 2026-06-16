# Windows Server Core base image for headless MSVC builds
# Scope: headless build + test only (no GPU, no GUI per SD-ENV-008)
FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["cmd", "/S", "/C"]

# Install VS 2022 Build Tools with C++ Desktop workload
RUN curl -fsSL -o vs_buildtools.exe "https://aka.ms/vs/17/release/vs_buildtools.exe" && \
    vs_buildtools.exe --quiet --wait --norestart --nocache \
        --installPath "C:\BuildTools" \
        --add Microsoft.VisualStudio.Workload.VCTools \
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
        --add Microsoft.VisualStudio.Component.Windows11SDK.22621 \
        --includeRecommended && \
    del vs_buildtools.exe

# Install Python 3.11
RUN curl -fsSL -o python_installer.exe "https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe" && \
    python_installer.exe /quiet InstallAllUsers=1 PrependPath=1 Include_test=0 && \
    del python_installer.exe

# Install Git
RUN curl -fsSL -o git_installer.exe "https://github.com/git-for-windows/git/releases/download/v2.44.0.windows.1/Git-2.44.0-64-bit.exe" && \
    git_installer.exe /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS="icons,ext\reg\shellhere,assoc,assoc_sh" && \
    del git_installer.exe

# Configure PATH for MSBuild, Python, Git
ENV PATH="C:\BuildTools\MSBuild\Current\Bin;C:\Program Files\Python311;C:\Program Files\Python311\Scripts;C:\Program Files\Git\cmd;C:\Program Files\Git\bin;${PATH}"

WORKDIR C:\repo
