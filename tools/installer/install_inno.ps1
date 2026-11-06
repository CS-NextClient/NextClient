param([Parameter(Mandatory = $true)][string]$destination)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$destination_path = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($destination)
$compiler = Join-Path $destination_path 'ISCC.exe'
if (Test-Path -LiteralPath $compiler) {
    Write-Output $compiler
    exit 0
}
$download = Join-Path $env:TEMP ('nextclient-inno-' + [Guid]::NewGuid().ToString('N') + '.exe')
try {
    Invoke-WebRequest -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe' -OutFile $download
    $expected = '9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732'
    if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected) {
        throw 'Inno Setup SHA-256 mismatch.'
    }
    $tool_process = Start-Process -FilePath $download -ArgumentList @(
        '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/CURRENTUSER', '/NOICONS', '/TASKS=',
        ('/DIR="' + $destination_path + '"')
    ) -WindowStyle Hidden -Wait -PassThru
    if ($tool_process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $compiler)) {
        throw 'Inno Setup installation failed.'
    }
    Write-Output $compiler
} finally {
    Remove-Item -LiteralPath $download -ErrorAction SilentlyContinue
}
