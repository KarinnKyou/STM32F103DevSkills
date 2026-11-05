---
name: f103-hw
description: STM32F103C8T6 板子的硬件在线操作：用 pyOCD + CMSIS-DAP 识别芯片、烧录、擦除、复位与 GDB 调试，以及核心板/扩展板插座引脚与模块接线。用于烧录失败、探针连不上、断点调试、芯片容量不符与确认模块插哪个引脚。
license: MIT
---

# STM32F103 硬件在线

调试器是 **CMSIS-DAP**，不是 ST-Link：烧录、擦除、复位与调试统一用 pyOCD，不使用 STM32CubeProgrammer。

1. 确认探针与目标：`pyocd list` 看探针，`pyocd list --targets` 确认 `stm32f103c8` 在列表内。
2. 首次上电先核对芯片真身与 Flash 容量，再决定链接脚本；容量不符会导致烧录后不运行或后半段不可用。见[烧录与调试](references/flash-debug.md)。
3. 烧录优先给 ELF（自带地址）；只给 bin 时必须显式 `--base-address 0x08000000`。
4. 需要断点、调用栈、寄存器或 Fault 现场时启动 gdbserver 配合 gdb，命令与约束见[烧录与调试](references/flash-debug.md)。
5. 接线与插座引脚见[引脚表](references/pinmap.md)与[扩展板接口](references/expansion-board.md)；模块与例程的对应见[模块清单](../f103-firmware/references/modules.md)。

烧录前确认板上是否已有他人程序，不确认目标时不做整片擦除。

完成时给出所用命令、写入字节数与校验结果，并说明复位方式。区分"命令成功"与"程序按预期运行"。
