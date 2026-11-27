param([string]$Compiler, [switch]$ControlDiag, [switch]$TouchControl, [switch]$TouchGateTest, [switch]$TouchSpeedTest, [switch]$TouchControlStage2, [switch]$TouchZcStage3, [switch]$TouchZcGateTest, [switch]$TouchZcPotUi, [switch]$GateBaseTest)
$ErrorActionPreference = 'Continue'
$taskRoot = $PSScriptRoot
$taskRepo = Split-Path $taskRoot
if (!$Compiler) {
    $Compiler = (Get-ChildItem 'C:/ST/STM32CubeIDE*/STM32CubeIDE/plugins/*gnu-tools*/tools/bin/arm-none-eabi-gcc.exe' | Select-Object -First 1).FullName
}
if (!$Compiler) { throw 'Pass -Compiler pointing to arm-none-eabi-gcc.exe' }
$taskTools = Split-Path $Compiler
$taskOut = Join-Path $taskRoot 'build'
New-Item -ItemType Directory -Force $taskOut | Out-Null
$taskFlags = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard',
    '-DUSE_HAL_DRIVER','-DSTM32F407xx','-Os','-g3','-Wall','-Wextra',
    '-ffunction-sections','-fdata-sections',"-I$taskRoot/Inc",
    "-I$taskRepo/Drivers/STM32F4xx_HAL_Driver/Inc",
    "-I$taskRepo/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy",
    "-I$taskRepo/Drivers/CMSIS/Device/ST/STM32F4xx/Include",
    "-I$taskRepo/Drivers/CMSIS/Include")
if ($TouchGateTest) { $taskFlags += '-DTOUCH_GATE_TEST' }
if ($TouchSpeedTest) { $taskFlags += '-DTOUCH_SPEED_TEST' }
if ($TouchControlStage2) { $taskFlags += '-DTOUCH_GATE_TEST'; $taskFlags += '-DTOUCH_SPEED_TEST' }
if ($TouchZcStage3) { $taskFlags += '-DTOUCH_SPEED_TEST'; $taskFlags += '-DTOUCH_ZC_STAGE3' }
if ($TouchZcGateTest) { $taskFlags += '-DTOUCH_SPEED_TEST'; $taskFlags += '-DTOUCH_ZC_STAGE3'; $taskFlags += '-DTOUCH_GATE_TEST'; $taskFlags += '-DTOUCH_ZC_GATE_TEST'; $taskFlags += '-DSENSOR_SUPERVISION' }
if ($TouchZcPotUi) { $taskFlags += '-DTOUCH_SPEED_TEST'; $taskFlags += '-DTOUCH_ZC_STAGE3'; $taskFlags += '-DTOUCH_GATE_TEST'; $taskFlags += '-DTOUCH_ZC_GATE_TEST'; $taskFlags += '-DPOT_SPEED_CONTROL' }
$taskMain = if ($GateBaseTest) { "$taskRoot/Src/gate_base_test.c" } elseif ($ControlDiag) { "$taskRoot/Src/control_diag.c" } else { "$taskRoot/Src/main.c" }
$taskSources = @($taskMain, "$taskRoot/Src/fsmc.c",
    "$taskRoot/Src/ili9341.c", "$taskRoot/Src/lcd_font.c")
if (!$ControlDiag -and !$GateBaseTest) { $taskSources += "$taskRoot/Src/spi.c"; $taskSources += "$taskRoot/Src/touch_ui.c" }
$taskSources += @(
    "$taskRepo/Src/system_stm32f4xx.c", "$taskRepo/Src/stm32f4xx_hal_msp.c",
    "$taskRepo/Src/syscalls.c", "$taskRepo/Src/sysmem.c",
    "$taskRepo/Startup/startup_stm32f407vetx.s")
foreach ($taskUnit in @('hal','hal_cortex','hal_dma','hal_gpio','hal_rcc','hal_rcc_ex',
    'hal_flash','hal_flash_ex','hal_pwr','hal_pwr_ex','hal_spi','hal_sram','ll_fsmc')) {
    $taskSources += "$taskRepo/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_$taskUnit.c"
}
$taskName = if ($TouchZcPotUi) { 'SmartDrill_F407_V33_TOUCH_ZC_POT_UI' } elseif ($TouchZcGateTest) { 'SmartDrill_F407_V33_TOUCH_ZC_GATE_TEST' } elseif ($TouchZcStage3) { 'SmartDrill_F407_V33_TOUCH_ZC_STAGE3' } elseif ($TouchControlStage2) { 'SmartDrill_F407_V33_TOUCH_CONTROL_STAGE2' } elseif ($TouchSpeedTest) { 'SmartDrill_F407_V33_TOUCH_SPEED_TEST' } elseif ($TouchGateTest) { 'SmartDrill_F407_V33_TOUCH_GATE_TEST' } elseif ($GateBaseTest) { 'SmartDrill_F407_V33_Q2_BASE_TEST' } elseif ($ControlDiag) { 'SmartDrill_F407_V33_CONTROL_DIAG_R5_PC2' } elseif ($TouchControl) { 'SmartDrill_F407_V33_TOUCH_CONTROL_R6_FAST' } else { 'SmartDrill_F407_V33_TOUCH_CAL_R4' }
$taskElf = "$taskOut/$taskName.elf"
& $Compiler @taskFlags @taskSources "-T$taskRepo/STM32F407VETX_FLASH.ld" --specs=nosys.specs --specs=nano.specs '-Wl,--gc-sections' "-Wl,-Map=$taskOut/$taskName.map" -o $taskElf
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
foreach ($taskFormat in @(@('ihex','hex'),@('binary','bin'))) {
    & "$taskTools/arm-none-eabi-objcopy.exe" -O $taskFormat[0] $taskElf "$taskOut/$taskName.$($taskFormat[1])"
    if ($LASTEXITCODE -ne 0) { throw 'Export failed' }
}
& "$taskTools/arm-none-eabi-size.exe" $taskElf
if ($LASTEXITCODE -ne 0) { throw 'Size failed' }
