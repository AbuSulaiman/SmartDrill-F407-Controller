param([string]$Compiler)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if (!$Compiler) {
    $toolCandidate = Get-ChildItem -Path 'C:/ST/STM32CubeIDE*/STM32CubeIDE/plugins/*gnu-tools*/tools/bin/arm-none-eabi-gcc.exe' | Select-Object -First 1
    if (!$toolCandidate) { throw 'Pass -Compiler with the path to arm-none-eabi-gcc.exe.' }
    $Compiler = $toolCandidate.FullName
}
$toolDirectory = Split-Path $Compiler
$outputDirectory = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$commonFlags = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard',
    '-DUSE_HAL_DRIVER','-DSTM32F407xx', '-O0','-g3','-ffunction-sections','-fdata-sections','-Wall','-Wextra',
    "-I$projectRoot/Inc","-I$projectRoot/Drivers/STM32F4xx_HAL_Driver/Inc",
    "-I$projectRoot/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy",
    "-I$projectRoot/Drivers/CMSIS/Device/ST/STM32F4xx/Include",
    "-I$projectRoot/Drivers/CMSIS/Include")
$objectFiles = @()
foreach ($source in Get-Content (Join-Path $projectRoot 'build-sources.txt')) {
    $objectFile = Join-Path $outputDirectory (($source -replace '[/\\]', '_') -replace '\.[cs]$', '.o')
    & $Compiler @commonFlags -c (Join-Path $projectRoot $source) -o $objectFile
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
    $objectFiles += $objectFile
}
$elfPath = Join-Path $outputDirectory 'SmartDrill_F407.elf'
& $Compiler @commonFlags @objectFiles -o $elfPath "-T$projectRoot/STM32F407VETX_FLASH.ld" --specs=nosys.specs --specs=nano.specs "-Wl,-Map=$outputDirectory/SmartDrill_F407.map" '-Wl,--gc-sections' -static '-Wl,--start-group' -lc -lm '-Wl,--end-group'
if ($LASTEXITCODE -ne 0) { throw 'Link failed.' }
& "$toolDirectory/arm-none-eabi-size.exe" $elfPath
& "$toolDirectory/arm-none-eabi-objcopy.exe" -O ihex $elfPath "$outputDirectory/SmartDrill_F407.hex"
if ($LASTEXITCODE -ne 0) { throw 'HEX export failed.' }
& "$toolDirectory/arm-none-eabi-objcopy.exe" -O binary $elfPath "$outputDirectory/SmartDrill_F407.bin"
if ($LASTEXITCODE -ne 0) { throw 'BIN export failed.' }

