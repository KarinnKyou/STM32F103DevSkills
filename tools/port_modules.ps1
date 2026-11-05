# port_modules.ps1 — port Wildfire F103 module examples to the CMake skeleton and build them.
#
# For each module folder under $DocRoot holding a Wildfire HAL project,
# assemble a build tree (complete Drivers + that module's User + gcc startup + our template)
# and run cmake/ninja. Reports a table of results.
#
# Usage:
#   .\port_modules.ps1                       # all modules
#   .\port_modules.ps1 -Only buzzer,ldr       # substring filter on folder name

param(
    [string]$DocRoot = 'D:\DSH_Workspace\04_Embedded\f103_doc',
    [string]$WorkRoot = 'D:\DSH_Workspace\04_Embedded\_smoke\mods',
    [string]$Template = 'D:\DSH_Workspace\04_Embedded\.agents\skills\f103-project\template',
    [string[]]$Only = @()
)

$ErrorActionPreference = 'Continue'
$env:PATH = "C:\Users\karin\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin;" +
            "C:\Users\karin\AppData\Local\stm32cube\bundles\ninja\1.13.2+st.1\bin;" +
            "C:\Users\karin\AppData\Local\stm32cube\bundles\cmake\4.4.0+st.1\bin;" + $env:PATH
Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue

$gccBin = 'C:\Users\karin\AppData\Local\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin'
$gcc    = "$gccBin\arm-none-eabi-gcc.exe"
$size   = "$gccBin\arm-none-eabi-size.exe"

# Complete vendor Drivers (module examples ship a trimmed Inc/ that misses 22 LL headers)
$umbrella = Get-ChildItem $DocRoot -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -eq '0-综合测试-HAL' } | Select-Object -First 1
if (-not $umbrella) { throw 'missing the complete Drivers source (0-综合测试-HAL)' }
$driversSrc = Join-Path $umbrella.FullName 'Drivers'
$startupSrc = Join-Path $driversSrc 'CMSIS\Device\ST\STM32F1xx\Source\Templates\gcc\startup_stm32f103xb.s'
if (-not (Test-Path $startupSrc)) { throw "missing GCC startup file: $startupSrc" }

function Find-HalUser([string]$moduleDir) {
    Get-ChildItem $moduleDir -Recurse -Directory -Filter 'User' -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match '核心板' -and
            ($_.FullName -match 'HAL') -and
            (Test-Path (Join-Path $_.FullName 'main.c'))
        } | Select-Object -First 1
}

$results = @()
$modules = Get-ChildItem $DocRoot -Directory | Where-Object { $_.Name -like '野火小智*' }
foreach ($m in $modules) {
    if ($Only.Count -gt 0 -and -not ($Only | Where-Object { $m.Name -like "*$_*" })) { continue }

    $short = ($m.Name -replace '野火小智', '' -replace '【|】', '' -replace '模块资料', '' -replace '_\d+$', '').Trim()
    if (-not $short) { $short = $m.Name }
    $out = Join-Path $WorkRoot $short

    $user = Find-HalUser $m.FullName
    if (-not $user) {
        $results += [pscustomobject]@{ Module = $short; Status = 'SKIP'; Detail = 'no C8 core-board HAL example'; Text = '' }
        continue
    }

    if (Test-Path $out) { Remove-Item $out -Recurse -Force }
    New-Item -ItemType Directory -Path "$out\src" -Force | Out-Null
    Copy-Item "$($user.FullName)\*" "$out\src" -Recurse -Force
    Copy-Item $driversSrc "$out\drivers" -Recurse -Force
    New-Item -ItemType Directory -Path "$out\startup" -Force | Out-Null
    Copy-Item $startupSrc "$out\startup" -Force
    Copy-Item "$Template\CMakeLists.txt" "$out" -Force
    Copy-Item "$Template\linker" "$out" -Recurse -Force
    Copy-Item "$Template\src\retarget.c" "$out\src\retarget.c" -Force

    $cfg = & cmake -S $out -B "$out\build" -G Ninja `
        -DCMAKE_C_COMPILER="$gcc" -DCMAKE_ASM_COMPILER="$gcc" -DCMAKE_SIZE="$size" 2>&1
    $build = & cmake --build "$out\build" 2>&1
    $code = $LASTEXITCODE
    $elf = Join-Path $out 'build\f103_app.elf'
    if ($code -eq 0 -and (Test-Path $elf)) {
        $s = (& $size $elf) -split "`n" | Select-Object -Last 1
        $results += [pscustomobject]@{ Module = $short; Status = 'OK'; Detail = ($s.Trim() -replace '\s+', ' '); Text = '' }
    } else {
        $err = ($build | Select-String -Pattern 'error:|fatal error:|undefined reference' | Select-Object -First 2) -join ' | '
        if (-not $err) { $err = ($cfg | Select-String -Pattern 'Error' | Select-Object -First 2) -join ' | ' }
        $results += [pscustomobject]@{ Module = $short; Status = 'FAIL'; Detail = $err; Text = $user.FullName }
    }
}

$results | Format-Table -AutoSize -Wrap | Out-String -Width 200
"OK=$(($results | Where-Object Status -eq 'OK').Count)  FAIL=$(($results | Where-Object Status -eq 'FAIL').Count)  SKIP=$(($results | Where-Object Status -eq 'SKIP').Count)"
