# Download the pinned experimental package. No installation or device access.
param(
    [string]$DestinationDirectory,
    [string]$CacheDirectory
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

$artifactCommit = '72289d5ea911f42e5945f72098dea6d19b308719'
$fileName = 'car-bsp-artifacts-with-initrd.tar.gz'
$expectedBytes = 29094301
$expectedHash = 'a7e612fe27e8151abd5d85c37accc67f24045d02891b9ab09658a503ddeee6e5'
$parts = @(
    @{ Name='car-bsp-a7e612fe.part01'; Bytes=8388608; Sha256='af5393842a50a4dc781244f646f558436381a69eb7c9e13f7f91e424be225a44' },
    @{ Name='car-bsp-a7e612fe.part02'; Bytes=8388608; Sha256='62d719fdb2144baec0dcc5d5d069ce122466429807a067b9d7b2ea5b6d1a4b21' },
    @{ Name='car-bsp-a7e612fe.part03'; Bytes=8388608; Sha256='f7a1eada4a2579fb16dbe3d7bd9b61583f5d48ac20e46e4759a7f2210b9d8d30' },
    @{ Name='car-bsp-a7e612fe.part04'; Bytes=3928477; Sha256='ef7ba2235e91ce7c437965ee57cd279b3b5f4c2442c5b82ffeb0c69436290cf4' }
)

function Matches-File([string]$Path, [long]$Bytes, [string]$Sha256) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    if ((Get-Item -LiteralPath $Path -Force).Length -ne $Bytes) { return $false }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Sha256
}

function Get-VerifiedPart($Part, [int]$Index) {
    $path = Join-Path $CacheDirectory $Part.Name
    if (Matches-File $path $Part.Bytes $Part.Sha256) {
        Write-Host "[$Index/4] Using verified cached part"
        return $path
    }
    $partial = $path + '.partial'
    $lastError = 'Checksum mismatch'
    $urls = @(
        "https://raw.githubusercontent.com/saberDDX/car/$artifactCommit/$($Part.Name)",
        "https://cdn.jsdelivr.net/gh/saberDDX/car@$artifactCommit/$($Part.Name)"
    )
    foreach ($attempt in 1..2) {
        foreach ($url in $urls) {
            try {
                Write-Host "[$Index/4] Downloading part, attempt $attempt"
                Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $partial -TimeoutSec 60
                if (Matches-File $partial $Part.Bytes $Part.Sha256) {
                    Move-Item -LiteralPath $partial -Destination $path -Force
                    return $path
                }
                $lastError = 'Downloaded part has wrong size or SHA256'
            } catch {
                $lastError = $_.Exception.Message
            }
        }
    }
    throw "Cannot download part $Index. Valid cached parts are preserved; rerun to resume. $lastError"
}

if (-not $DestinationDirectory) {
    $DestinationDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::DesktopDirectory)
}
if (-not $DestinationDirectory) { throw 'Cannot locate your Windows desktop directory.' }
if (-not $CacheDirectory) {
    $CacheDirectory = Join-Path ([IO.Path]::GetTempPath()) 'car-bsp-a7e612fe'
}
New-Item -ItemType Directory -Path $DestinationDirectory -Force | Out-Null
$destination = Join-Path $DestinationDirectory $fileName
if (Matches-File $destination $expectedBytes $expectedHash) {
    Write-Output "READY: $destination"
    Write-Output "SHA256: $expectedHash"
    return
}
New-Item -ItemType Directory -Path $CacheDirectory -Force | Out-Null
$lockPath = Join-Path $CacheDirectory 'download.lock'
$lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$assembly = Join-Path $DestinationDirectory ('.car-bsp-' + [Guid]::NewGuid().ToString('N') + '.partial')
try {
    $downloaded = @()
    for ($index = 0; $index -lt $parts.Length; $index++) {
        $downloaded += Get-VerifiedPart $parts[$index] ($index + 1)
    }
    Write-Host 'Combining verified parts...'
    $output = [IO.File]::Open($assembly, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        foreach ($part in $downloaded) {
            $inputStream = [IO.File]::OpenRead($part)
            try { $inputStream.CopyTo($output) } finally { $inputStream.Dispose() }
        }
        $output.Flush($true)
    } finally {
        $output.Dispose()
    }
    if (-not (Matches-File $assembly $expectedBytes $expectedHash)) {
        throw 'Combined file SHA256 mismatch. The desktop destination has not been replaced.'
    }
    if (Test-Path -LiteralPath $destination) {
        $backup = $destination + '.previous-' + [Guid]::NewGuid().ToString('N')
        [IO.File]::Move($destination, $backup)
        Write-Host "Preserved previous different file: $backup"
    }
    [IO.File]::Move($assembly, $destination)
    Write-Output "READY: $destination"
    Write-Output "Bytes: $expectedBytes"
    Write-Output "SHA256: $expectedHash"
    Write-Output 'Download complete. Keep the archive compressed; no system changes were made.'
} finally {
    $lock.Dispose()
    if (Test-Path -LiteralPath $assembly -PathType Leaf) {
        Remove-Item -LiteralPath $assembly -Force
    }
}
