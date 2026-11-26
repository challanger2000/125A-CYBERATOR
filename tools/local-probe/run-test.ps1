param([ValidateSet('normal','full','smoke')][string]$Mode = 'normal')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
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
function Verify-Hash([string]$Path,[string]$Expected) {
    if ($Expected -notmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid expected SHA256' }
    $actual=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) { throw "SHA256 mismatch: $Path" }
    return $actual
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
    $cpu=Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed
    $os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture,TotalVisibleMemorySize
    [ordered]@{cpu=@($cpu);os=$os;power_plan=(& powercfg.exe /getactivescheme | Out-String).Trim()} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $report 'system.json') -Encoding UTF8
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
}catch{
    $result.error=$_.Exception.Message
    Write-Host "Test execution failed: $($result.error)"
}finally{
    $result.end_utc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $report 'result.json') -Encoding UTF8
    Compress-Archive -Path (Join-Path $report '*') -DestinationPath $zip -CompressionLevel Optimal
    Write-Host "Result: $($result.verdict)"
    Write-Host "Report ZIP: $zip"
}
exit $exitCode
