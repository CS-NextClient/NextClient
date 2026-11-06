#Requires -Version 5.1

<#
.SYNOPSIS
Builds the Windows installer from a prepared NextClient Release payload.
.DESCRIPTION
Prepares pinned x86 packaging tools, runs installer tests, and produces the EXE,
SHA-256 checksum, and build manifest. Relative paths use the repository root.
.EXAMPLE
.\tools\installer\build_installer.ps1 -Payload install -Version 2.6.0-preview.1
.EXAMPLE
.\tools\installer\build_installer.ps1 -Payload build\installer-payload -SmokeTest
#>

[CmdletBinding()]
param(
    [string]$payload = 'install',
    [string]$version,
    [string]$output,
    [Alias('ToolsDirectory')][string]$tools_directory = 'build\installer-tools',
    [string]$python,
    [string]$iscc,
    [Alias('SkipTests')][switch]$skip_tests,
    [Alias('SmokeTest')][switch]$smoke_test,
    [switch]$help
)

if ($help) {
    @'
Build the Windows installer from a prepared NextClient Release payload.

Usage: .\tools\installer\build_installer.ps1 [options]

Options:
  -Payload <path>          Prepared Windows x86 Release package (default: install).
  -Version <version>       Package version, such as 2.6.0-preview.1.
                          Defaults to the project version with a local Git suffix.
  -Output <path>           Destination (default: out\installer\<version>).
  -ToolsDirectory <path>   Tool cache (default: build\installer-tools).
  -Python <path>           Existing Python 3.12 x86 executable.
  -Iscc <path>             Existing Inno Setup compiler executable.
  -SkipTests              Skip installer unit tests.
  -SmokeTest              Exercise the built installer with synthetic files.
  -Help                   Show this help without preparing tools or building.

Relative paths use the repository root. Missing tools are prepared automatically.
The build produces an EXE, a SHA-256 checksum, and a build manifest.

Example:
  .\tools\installer\build_installer.ps1 -Payload install -Version 2.6.0-preview.1 -SmokeTest
'@
    return
}

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repository_root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function ResolveRepositoryPath([string]$value) {
    if (-not [IO.Path]::IsPathRooted($value)) {
        $value = Join-Path $repository_root $value
    }
    return [IO.Path]::GetFullPath($value)
}

function InvokeChecked([string]$executable, [string[]]$arguments) {
    $previous_preference = $ErrorActionPreference
    try {
        # Windows PowerShell can turn native stderr into PowerShell error records.
        $ErrorActionPreference = 'Continue'
        & $executable @arguments
        $succeeded = $?
        $exit_code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous_preference
    }
    if (-not $succeeded -or $exit_code -ne 0) {
        throw "$([IO.Path]::GetFileName($executable)) failed with exit code $exit_code."
    }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This script requires Windows.'
}
$payload_path = ResolveRepositoryPath $payload
if (-not (Test-Path -LiteralPath $payload_path -PathType Container)) {
    throw "Release payload directory does not exist: $payload_path. Supply -Payload with a prepared Release package."
}
$git = (Get-Command git -CommandType Application | Select-Object -First 1).Source
$revision = (& $git -C $repository_root rev-parse HEAD | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Cannot determine the checked-out Git revision.'
}
if (-not $version) {
    $cmake = Get-Content -LiteralPath (Join-Path $repository_root 'CMakeLists.txt') -Raw
    $match = [regex]::Match($cmake, 'project\(NextClient\s+VERSION\s+(\d+\.\d+\.\d+)\)')
    if (-not $match.Success) {
        throw 'Cannot determine the project version. Supply -Version explicitly.'
    }
    $version = $match.Groups[1].Value + '-local.' + $revision.Substring(0, 8)
}
if ($version -notmatch '^v?\d+\.\d+\.\d+(?:[-.][A-Za-z0-9][A-Za-z0-9.-]*)?$') {
    throw 'Invalid installer version. Use a version such as 2.6.0-preview.1.'
}
if (-not $output) {
    $output = 'out\installer\' + $version
}
$output_path = ResolveRepositoryPath $output
$tools_path = ResolveRepositoryPath $tools_directory
if ($iscc) {
    $iscc = ResolveRepositoryPath $iscc
    if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
        throw "Inno Setup compiler does not exist: $iscc"
    }
}
if ($python) {
    $python = ResolveRepositoryPath $python
    if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
        throw "Python executable does not exist: $python"
    }
}

$previous_tls = [Net.ServicePointManager]::SecurityProtocol
Push-Location -LiteralPath $repository_root
try {
    [Net.ServicePointManager]::SecurityProtocol = $previous_tls -bor [Net.SecurityProtocolType]::Tls12
    if (-not $python) {
        $python_directory = Join-Path $tools_path 'pythonx86-3.12.10'
        $python = Join-Path $python_directory 'tools\python.exe'
        if (-not (Test-Path -LiteralPath $python -PathType Leaf)) {
            New-Item -ItemType Directory -Path $tools_path -Force | Out-Null
            $archive = Join-Path $tools_path 'pythonx86-3.12.10.zip'
            $expected = '49abd1c405addfd41cf955c2a2a7abcc3c3ee06e8b8013c70a71f61680aed208'
            if (-not (Test-Path -LiteralPath $archive) -or
                (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected) {
                Write-Host 'Downloading Python 3.12.10 x86...'
                $download = Join-Path $tools_path ([Guid]::NewGuid().ToString('N') + '.zip')
                try {
                    Invoke-WebRequest -UseBasicParsing -Uri 'https://api.nuget.org/v3-flatcontainer/pythonx86/3.12.10/pythonx86.3.12.10.nupkg' -OutFile $download
                    if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected) {
                        throw 'Python archive SHA-256 mismatch.'
                    }
                    Move-Item -LiteralPath $download -Destination $archive -Force
                } finally {
                    if (Test-Path -LiteralPath $download) {
                        Remove-Item -LiteralPath $download
                    }
                }
            }
            Write-Host 'Preparing Python 3.12.10 x86...'
            Expand-Archive -LiteralPath $archive -DestinationPath $python_directory -Force
        }
    }

    $probe = "import struct, sys; assert sys.version_info[:2] == (3, 12) and struct.calcsize('P') == 4, 'Python 3.12 x86 is required'"
    InvokeChecked -Executable $python -Arguments @('-I', '-c', $probe)
    $validate = 'import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); from tools.installer.build_installer import NumericVersion, ValidatePayload; NumericVersion(sys.argv[2]); ValidatePayload(Path(sys.argv[3]))'
    InvokeChecked -Executable $python -Arguments @('-I', '-c', $validate, $repository_root, $version, $payload_path)

    $environment = Join-Path $tools_path 'venv-x86'
    $packaging_python = Join-Path $environment 'Scripts\python.exe'
    if (-not (Test-Path -LiteralPath $packaging_python -PathType Leaf)) {
        Write-Host 'Creating the x86 packaging environment...'
        InvokeChecked -Executable $python -Arguments @('-I', '-m', 'venv', $environment)
    }
    InvokeChecked -Executable $packaging_python -Arguments @('-I', '-c', $probe)
    Write-Host 'Preparing installer dependencies...'
    InvokeChecked -Executable $packaging_python -Arguments @(
        '-I', '-m', 'pip', '--disable-pip-version-check', 'install', '-r',
        (Join-Path $repository_root 'installer\requirements-build.txt')
    )
    if (-not $iscc) {
        $inno_directory = Join-Path $tools_path 'inno-6.7.3'
        & (Join-Path $PSScriptRoot 'install_inno.ps1') -Destination $inno_directory | Write-Host
        if ($LASTEXITCODE -ne 0) {
            throw 'Inno Setup preparation failed.'
        }
        $iscc = Join-Path $inno_directory 'ISCC.exe'
    }
    if (-not $skip_tests) {
        Write-Host 'Running installer tests...'
        InvokeChecked -Executable $packaging_python -Arguments @('-I', '-m', 'unittest', 'discover', '-s', 'installer/tests', '-t', '.')
    }
    Write-Host "Building NextClient $version from $payload_path..."
    InvokeChecked -Executable $packaging_python -Arguments @(
        '-I', (Join-Path $PSScriptRoot 'build_installer.py'), '--payload', $payload_path, '--version', $version,
        '--revision', $revision, '--iscc', $iscc, '--output', $output_path
    )
    $prefix = Join-Path $output_path ('NextClient-' + $version + '-setup')
    if ($smoke_test) {
        $manifest = Get-Content -LiteralPath ($prefix + '.manifest.json') -Raw | ConvertFrom-Json
        $helper = $manifest.artifacts | Where-Object { [IO.Path]::GetFileName($_.path) -eq 'nextclient-install.exe' }
        if (-not $helper) {
            throw 'The build manifest has no installer helper.'
        }
        $smoke_directory = Join-Path $repository_root ('build\installer-smoke\' + [Guid]::NewGuid().ToString('N'))
        InvokeChecked -Executable $packaging_python -Arguments @(
            '-I', (Join-Path $PSScriptRoot 'smoke_installer.py'), '--python', $packaging_python,
            '--iscc', $iscc, '--helper', $helper.path, '--work-dir', $smoke_directory
        )
        Write-Host "Verification logs: $smoke_directory"
    }
    foreach ($extension in @('.exe', '.sha256', '.manifest.json')) {
        Write-Host (Get-Item -LiteralPath ($prefix + $extension)).FullName
    }
} finally {
    Pop-Location
    [Net.ServicePointManager]::SecurityProtocol = $previous_tls
}
