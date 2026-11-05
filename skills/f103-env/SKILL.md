---
name: f103-env
description: 定位并验证 STM32F103 开发工具链（arm-none-eabi-gcc、CMake、Ninja、pyOCD、CMSIS-DAP）与野火小智资料树。用于找不到编译器或烧录器、工具不在 PATH、资料编码乱码、以及确认本机具备哪些 F1 开发能力。
license: MIT
---

# STM32F103 环境

只处理 STM32F1 系列（Cortex-M3、HAL1、传统 CMSIS 工程）。C5 使用 Cortex-M33、HAL2 与 CubeMX2，两条路线不能互相套用。芯片订货号与工程锁定版本优先于本文示例。

上电前先确认工作目录与目标板型号（C8T6 = 64 KiB Flash / 20 KiB RAM），再按顺序确认能力。

1. 按[工具发现](references/tools.md)定位 gcc / CMake / Ninja / pyOCD 的实际路径与版本。工具链随 cube bundle 分发，通常不在 PATH，需在本次会话内前置。
2. 运行 cube 或工具链子进程前移除 `ELECTRON_RUN_AS_NODE`：DSH 会向每个 shell 注入该变量，后端与部分工具链脚本会异常。
3. 用真实命令验证能力，不把"找到 exe"当成"工具可用"：`arm-none-eabi-gcc --version`、`pyocd list --targets`。
4. 需要厂商源码、例程或原理图时按[资料树](references/sources.md)取；`Drivers` 必须用核心板"综合测试-HAL"的完整版。
5. 不引入 CubeMX：F1 的图形化生成器只导出 Makefile/MDK/IAR，不导出 CMake。本路线用手写 CMake，见[工程技能](../f103-project/SKILL.md)。

完成时给出工具路径、版本、验证命令及其输出。区分"已安装""命令可用""板端已跑通"三种状态。

按需读取：源码编码与 PDF 处理见[资料树](references/sources.md)；需要新建或改工程转到[工程技能](../f103-project/SKILL.md)，板端烧录调试转到[硬件技能](../f103-hw/SKILL.md)。
