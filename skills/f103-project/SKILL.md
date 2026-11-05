---
name: f103-project
description: 为 STM32F103C8T6 建立可编译的 CMake 工程（HAL1 + arm-none-eabi-gcc + Ninja），含链接脚本、GCC 启动文件与 printf 重定向。用于新建 F103 工程、把厂商 Keil 例程改成 CMake、或处理链接脚本与启动代码相关报错。
license: MIT
---

# STM32F103 工程搭建

目标：把厂商 Keil 例程变成 `arm-none-eabi-gcc` 可编译的 CMake 工程。本技能只做工程骨架与构建；烧录与板端验证属于[硬件技能](../f103-hw/SKILL.md)。

`template/` 提供可直接复制的文件：`CMakeLists.txt`、`linker/STM32F103C8Tx_FLASH.ld`、`src/retarget.c`、`src/iic_probe.c|.h`。

1. 建目录：`linker/`、`startup/`、`drivers/`、`src/`。
2. 复制完整 `Drivers`（核心板"综合测试-HAL"），含 `CMSIS/` 与 `STM32F1xx_HAL_Driver/`。模块例程的 Inc 被裁剪，缺 LL 头会编译失败。
3. 复制 gcc 版启动文件 `CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s` 到 `startup/`。
4. 复制厂商应用源码到 `src/`：`main.c`、`stm32f1xx_it.c`、`system_stm32f1xx.c`、`stm32f1xx_hal_conf.h`、`bsp_*`。
5. 放入模板文件，按实际工程名改 `project()` 与 `SUFFIX`。
6. 配置并构建：

```powershell
$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_ASM_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_SIZE="$gcc\arm-none-eabi-size.exe"
cmake --build build
```

模板的 `CMakeLists.txt` 会自动收集 `src/**` 与 HAL 源码并排除 `*_template.c`；改动目录结构时同步改 GLOB。

## 编译通过之后

```powershell
arm-none-eabi-readelf -h build\<name>.elf | Select-String 'Machine|Entry'
arm-none-eabi-size build\<name>.elf
```

完成标准：`Machine: ARM`、给出 `text/data/bss` 与 ELF 路径。构建成功不等于板端正确。

## 报错处理

编译或链接失败按[从 Keil 到 CMake](references/gcc-port.md)的现象表逐条核查，该表覆盖启动文件、printf 重定向、链接脚本与浮点格式化四类问题。只保留第一条有效错误与其上下文，不要从 `ninja stopped` 这类收尾信息反推原因。
