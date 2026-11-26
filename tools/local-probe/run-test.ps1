param([ValidateSet('normal','full','smoke','preflight','asio','asio-selftest')][string]$Mode = 'normal')
$ErrorActionPreference = 'Stop'
# Do not inherit PowerShell 7 or user-installed module paths into Windows PowerShell 5.1.
$env:PSModulePath = [System.IO.Path]::Combine($PSHOME,'Modules')
Import-Module -Name ([System.IO.Path]::Combine($PSHOME,'Modules','Microsoft.PowerShell.Management')) -ErrorAction Stop
Import-Module -Name ([System.IO.Path]::Combine($PSHOME,'Modules','Microsoft.PowerShell.Utility')) -ErrorAction Stop
$root = $PSScriptRoot
function Verify-Hash([string]$Path,[string]$Expected) {
    if ($Expected -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid expected SHA256' }
    $stream=[System.IO.File]::OpenRead($Path)
    try {
        $sha=[System.Security.Cryptography.SHA256]::Create()
        try {
            $actual=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()
        } finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
    if ($actual -ne $Expected.ToLowerInvariant()) { throw "SHA256 mismatch: $Path" }
    return $actual
}
function Write-ReportZip([string]$Folder,[string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($Folder,$Destination,[System.IO.Compression.CompressionLevel]::Optimal,$false)
}
function Test-Reports([bool]$Parallel,[int[]]$Rates,[int]$Groups,[int]$NativeExit) {
    $prefix=if($Parallel){'CYBERATOR-rt-parallel-'}else{'CYBERATOR-rt-'}
    $summary=@(Import-Csv -LiteralPath (Join-Path $report ($prefix+'summary.csv')))
    $raw=@(Import-Csv -LiteralPath (Join-Path $report ($prefix+'groups.csv')))
    $instances=if($Parallel){@(4)}else{@(1,4)}
    $expected=@{}
    foreach($rate in $Rates){foreach($block in @(64,256)){foreach($n in $instances){foreach($active in @(0,1)){
        $expected["$rate/$block/$n/$active"]=$true
    }}}}
    if($summary.Count -ne $expected.Count -or $raw.Count -ne ($expected.Count*$Groups)){throw "Incomplete $prefix data"}
    $seen=@{}
    $misses=$false
    foreach($s in $summary){
        $key="$($s.rate)/$($s.block)/$($s.instances)/$($s.active)"
        if(-not $expected.ContainsKey($key) -or $seen.ContainsKey($key) -or [int]$s.groups -ne $Groups){throw "Unexpected $prefix case: $key"}
        $seen[$key]=$true
        $duration=[int]$s.duration_misses
        $completion=[int]$s.completion_misses
        if($duration -lt 0 -or $duration -gt $Groups -or $completion -lt 0 -or $completion -gt $Groups){throw "Invalid miss counts in $key"}
        $failed=($duration -gt 0 -or $completion -gt 0)
        if(($failed -and $s.status -ne 'FAIL') -or (-not $failed -and $s.status -ne 'PASS')){throw "Inconsistent status in $key"}
        $misses=$misses -or $failed
    }
    if($Parallel){
        $tracks=@(Import-Csv -LiteralPath (Join-Path $report ($prefix+'tracks.csv')))
        if($tracks.Count -ne $expected.Count*$Groups*4){throw 'Incomplete per-track evidence'}
    }
    if(($misses -and $NativeExit -ne 3) -or (-not $misses -and $NativeExit -ne 0)){throw "Native exit contradicts summary: $prefix"}
}
if ($Mode -eq 'preflight') {
    $fixture=[System.IO.Path]::Combine([System.IO.Path]::GetTempPath(),('CYBERATOR-preflight-'+[Guid]::NewGuid().ToString('N')))
    [void][System.IO.Directory]::CreateDirectory($fixture)
    $archive="$fixture.zip"
    try {
        $file=Join-Path $fixture 'abc.txt'
        [System.IO.File]::WriteAllBytes($file,[byte[]]@(97,98,99))
        $known='ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
        if ((Verify-Hash $file $known) -ne $known) { throw 'Hash positive control failed' }
        [System.IO.File]::AppendAllText($file,'d')
        $rejected=$false
        try { [void](Verify-Hash $file $known) } catch { $rejected=$_.Exception.Message.StartsWith('SHA256 mismatch:') }
        if (-not $rejected) { throw 'Hash corruption negative control failed' }
        [ordered]@{complete=$true;count=2} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture 'result.json') -Encoding UTF8
        $data=Get-Content -LiteralPath (Join-Path $fixture 'result.json') -Raw | ConvertFrom-Json
        if (-not $data.complete -or $data.count -ne 2) { throw 'JSON round-trip failed' }
        [System.IO.File]::WriteAllText((Join-Path $fixture 'case.csv'),"rate,groups"+[Environment]::NewLine+"48000,2048")
        $row=@(Import-Csv -LiteralPath (Join-Path $fixture 'case.csv'))
        if ($row.Count -ne 1 -or $row[0].groups -ne '2048') { throw 'CSV round-trip failed' }
        Write-ReportZip $fixture $archive
        $z=[System.IO.Compression.ZipFile]::OpenRead($archive)
        try {
            if ($z.Entries.Count -ne 3 -or $null -eq $z.GetEntry('result.json')) { throw 'ZIP content verification failed' }
        } finally { $z.Dispose() }
        Write-Host 'Launcher preflight: SHA256 positive/negative, JSON, CSV and ZIP PASS'
        exit 0
    } finally {
        if ([System.IO.File]::Exists($archive)) { [System.IO.File]::Delete($archive) }
        [System.IO.Directory]::Delete($fixture,$true)
    }
}
$stamp = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$report = Join-Path $root "CYBERATOR-report-$stamp"
New-Item -ItemType Directory -Path $report | Out-Null
$zip = "$report.zip"
$result = [ordered]@{
    mode=$Mode
    start_utc=[DateTime]::UtcNow.ToString('o')
    physical_asio='NOT RUN'
    diagnostic='Paced VST3 host, no physical audio device. Wall-clock callback duration includes preemption.'
    serial_exit=$null
    parallel_exit=$null
    complete=$false
    verdict='EXECUTION_FAILURE'
    error=$null
}
$exitCode=1
try {
    $manifestPath=Join-Path $root 'manifest.json'
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $hostPath=Join-Path $root 'vst3_host_test.exe'
    $modulePath=Join-Path $root '125A CYBERATOR.vst3\Contents\x86_64-win\125A CYBERATOR.vst3'
    $infoPath=Join-Path $root '125A CYBERATOR.vst3\Contents\Resources\moduleinfo.json'
    $result.module_sha256=Verify-Hash $modulePath $manifest.module_sha256
    $result.host_sha256=Verify-Hash $hostPath $manifest.host_sha256
    $result.moduleinfo_sha256=Verify-Hash $infoPath $manifest.moduleinfo_sha256
    Copy-Item -LiteralPath $manifestPath -Destination $report
    # Deliberately omit user name, device serials, computer name and network data.
    $system=[ordered]@{
        logical_processors=[Environment]::ProcessorCount
        os_version=[Environment]::OSVersion.VersionString
        is_64bit_os=[Environment]::Is64BitOperatingSystem
        powershell_version=$PSVersionTable.PSVersion.ToString()
        cpu=$null
        os=$null
        power_plan=$null
        metadata_warnings=@()
    }
    try {
        $system.cpu=@(Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed)
        $system.os=Get-CimInstance Win32_OperatingSystem -ErrorAction Stop | Select-Object Caption,Version,BuildNumber,OSArchitecture,TotalVisibleMemorySize
    } catch { $system.metadata_warnings+=('CIM metadata unavailable: '+$_.Exception.Message) }
    try {
        $plan=& "$env:SystemRoot\System32\powercfg.exe" /getactivescheme
        if ($LASTEXITCODE -ne 0) { throw 'powercfg failed' }
        $system.power_plan=($plan | Out-String).Trim()
    } catch { $system.metadata_warnings+=('Power plan unavailable: '+$_.Exception.Message) }
    $system | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $report 'system.json') -Encoding UTF8
    if($Mode -in @('asio','asio-selftest')){
        $result.diagnostic='Native ASIO callback diagnostic; synthetic input, muted physical outputs. JUCE callback excludes driver buffer conversion and outputReady. No sample-position or acoustic loopback proof.'
        $relative='..\125A CYBERATOR.vst3\Contents\x86_64-win\125A CYBERATOR.vst3'
        Push-Location -LiteralPath $report
        try {
            & $hostPath $relative $Mode | Tee-Object -FilePath 'asio.stdout.txt'
            $code=$LASTEXITCODE
        } finally { Pop-Location }
        $result.asio_exit=$code
        if($code -notin @(0,3)){throw "ASIO diagnostic incomplete: $code"}
        $summary=@(Import-Csv -LiteralPath (Join-Path $report 'CYBERATOR-asio-summary.csv'))
        $raw=@(Import-Csv -LiteralPath (Join-Path $report 'CYBERATOR-asio-groups.csv'))
        if($summary.Count -ne 6){throw 'ASIO six-case matrix incomplete'}
        $total=0;$failed=$false
        for($i=0;$i -lt 6;$i++){
            $s=$summary[$i]
            if([int]$s.case -ne $i -or [int]$s.groups -le 0 -or $s.groups -ne $s.expected -or $s.status -notin @('SELFTEST','FAIL','PASS_IN_OBSERVED_METRICS')){throw 'Invalid ASIO case'}
            if(($Mode -eq 'asio-selftest') -ne ($s.status -eq 'SELFTEST')){throw 'ASIO evidence mode mismatch'}
            $total += [int]$s.groups
            if($s.status -eq 'FAIL'){$failed=$true}
        }
        if($raw.Count -ne $total -or ($failed -ne ($code -eq 3))){throw 'ASIO report inconsistent'}
        $result.complete=$true
        if($Mode -eq 'asio-selftest'){$result.verdict='SELFTEST_ONLY'}
        else{
            $result.physical_asio='EXECUTED'
            $result.verdict=if($failed){'DEADLINE_MISSES_OR_DRIVER_EVENTS'}else{'PASS_IN_OBSERVED_METRICS'}
        }
        $exitCode=$code
    }else{
    $modes=switch($Mode){
        'normal' {@('rt-local','rt-local-parallel')}
        'full' {@('rt-local-full','rt-local-full-parallel')}
        'smoke' {@('rt','rt-parallel')}
    }
    $rates=switch($Mode){
        'normal' {@(44100,48000)}
        'full' {@(44100,48000,96000,192000)}
        'smoke' {@(48000,96000,192000)}
    }
    $groups=if($Mode -eq 'smoke'){512}else{2048}
    # ASCII relative module path avoids Windows narrow argv encoding of a
    # possibly non-ASCII extraction directory; the host resolves its cwd natively.
    $relative='..\125A CYBERATOR.vst3\Contents\x86_64-win\125A CYBERATOR.vst3'
    for($i=0;$i -lt 2;$i++){
        $stage=if($i -eq 0){'serial'}else{'parallel'}
        Write-Host "Running $stage VST3 test ($Mode); please leave the computer idle."
        $stdout=Join-Path $report "$stage.stdout.txt"
        $stderr=Join-Path $report "$stage.stderr.txt"
        $process=Start-Process -FilePath $hostPath -ArgumentList @(('"' + $relative + '"'),$modes[$i]) -WorkingDirectory $report -RedirectStandardOutput $stdout -RedirectStandardError $stderr -NoNewWindow -Wait -PassThru
        $code=$process.ExitCode
        if($i -eq 0){$result.serial_exit=$code}else{$result.parallel_exit=$code}
        Get-Content -LiteralPath $stdout
        if($code -notin @(0,3)){throw "Native $stage execution failed: $code. See stderr."}
        Test-Reports ($i -eq 1) $rates $groups $code
    }
    $result.complete=$true
    if($result.serial_exit -eq 0 -and $result.parallel_exit -eq 0){
        $result.verdict='PASS_IN_TESTED_MATRIX'
        $exitCode=0
    }else{
        $result.verdict='DEADLINE_MISSES'
        $exitCode=3
    }
    }
}catch{
    $result.error=$_.Exception.Message
    Write-Host "Test execution failed: $($result.error)"
}finally{
    $result.end_utc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $report 'result.json') -Encoding UTF8
    Write-ReportZip $report $zip
    Write-Host "Result: $($result.verdict)"
    Write-Host "Report ZIP: $zip"
}
exit $exitCode
