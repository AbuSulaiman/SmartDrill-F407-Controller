param([switch]$EnableMotor, [switch]$Diagnostic, [switch]$ZeroTest, [switch]$GateTest, [switch]$SyncDiag)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskCompiler = (Get-ChildItem 'C:/ST/STM32CubeIDE*/STM32CubeIDE/plugins/*gnu-tools*/tools/bin/arm-none-eabi-gcc.exe' | Select-Object -First 1).FullName
if (!$taskCompiler) { throw 'STM32 compiler not found' }
$taskOutput = Join-Path $taskRoot 'build'
New-Item -ItemType Directory -Force $taskOutput | Out-Null
$taskNames = @('hal','hal_cortex','hal_gpio','hal_rcc','hal_rcc_ex','hal_flash','hal_flash_ex','hal_pwr','hal_pwr_ex','hal_dma','hal_dma_ex','hal_adc','hal_adc_ex')
$taskSources = @("$taskRoot/Core/Src/main.c", "$taskRoot/Core/Src/system_stm32g4xx.c", "$taskRoot/Core/Startup/startup_stm32g431cbux.s")
foreach ($taskName in $taskNames) { $taskSources += "$taskRoot/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_$taskName.c" }
if ($SyncDiag -and ($EnableMotor -or $Diagnostic -or $ZeroTest -or $GateTest)) { throw 'SyncDiag must be built independently' }
$taskEnabled = if ($EnableMotor -or $SyncDiag) { 1 } else { 0 }
if ($Diagnostic -and $EnableMotor) { throw 'Diagnostic mode cannot enable motor output' }
if ($ZeroTest -and ($EnableMotor -or $Diagnostic)) { throw 'ZeroTest is an independent motor-disabled test' }
if ($GateTest -and ($EnableMotor -or $Diagnostic -or $ZeroTest)) { throw 'GateTest must be built independently' }
$taskFlags = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-DSTM32G431xx','-DUSE_HAL_DRIVER',"-DMOTOR_OUTPUT_ENABLED=$taskEnabled",'-Os','-g3','-Wall','-Wextra','-ffunction-sections','-fdata-sections',"-I$taskRoot/Core/Inc","-I$taskRoot/Drivers/STM32G4xx_HAL_Driver/Inc","-I$taskRoot/Drivers/CMSIS/Device/ST/STM32G4xx/Include","-I$taskRoot/Drivers/CMSIS/Include")
$taskName = if ($EnableMotor) { 'SmartDrill_G431_MOTOR_RC1_2026-10-27' } else { 'drill_bench' }
if ($Diagnostic) { $taskFlags += '-DDRILL_DIAGNOSTIC=1'; $taskName = 'drill_diagnostic' }
if ($ZeroTest) { $taskFlags += '-DDRILL_ZERO_TEST=1'; $taskName = 'SmartDrill_G431_ZC_TEST_2026-10-24' }
if ($GateTest) { $taskFlags += '-DDRILL_GATE_TEST=1'; $taskName = 'SmartDrill_G431_PA5_STABILITY_V3_2026-10-26' }
if ($SyncDiag) { $taskFlags += '-DDRILL_SYNC_DIAG=1'; $taskName = 'SmartDrill_G431_SYNC_DIAG_2026-10-28' }
& $taskCompiler @taskFlags @taskSources "-T$taskRoot/STM32G431CBUX_FLASH.ld" --specs=nosys.specs --specs=nano.specs '-Wl,--gc-sections' -o "$taskOutput/$taskName.elf"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
$taskBin = Split-Path $taskCompiler
& "$taskBin/arm-none-eabi-objcopy.exe" -O ihex "$taskOutput/$taskName.elf" "$taskOutput/$taskName.hex"
if ($LASTEXITCODE -ne 0) { throw 'HEX failed' }
& "$taskBin/arm-none-eabi-objcopy.exe" -O binary "$taskOutput/$taskName.elf" "$taskOutput/$taskName.bin"
if ($LASTEXITCODE -ne 0) { throw 'BIN failed' }
& "$taskBin/arm-none-eabi-size.exe" "$taskOutput/$taskName.elf"
