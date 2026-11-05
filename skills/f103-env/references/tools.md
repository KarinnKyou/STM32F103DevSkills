# 工具发现

工具链随 STM32Cube 的 bundle 分发，默认不写入 PATH。先按下面的位置定位，再用真实命令确认版本。

| 工具 | 位置 | 备注 |
| --- | --- | --- |
| `arm-none-eabi-gcc` 14.3.1 | `%LOCALAPPDATA%\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin` | 含 gcc/g++/size/objcopy/readelf |
| `arm-none-eabi-gdb` 14.3.1 | `%LOCALAPPDATA%\stm32cube\bundles\gnu-gdb-for-stm32\14.3.1+st.2\bin` | 与 gcc 版本号一致但目录不同 |
| `cmake` 4.4 | `%LOCALAPPDATA%\stm32cube\bundles\cmake\4.4.0+st.1\bin` | 也可用 `cube cmake` 转发 |
| `ninja` 1.13.2 | `%LOCALAPPDATA%\stm32cube\bundles\ninja\1.13.2+st.1\bin` | 也可用 `cube ninja` 转发 |
| `pyocd` 0.46.1 | 随 Python 环境安装，通常在 `Scripts\pyocd.exe` | 在 PATH；内置 `stm32f103c8` 目标 |
| `cube` 0.10.3 | `%USERPROFILE%\.local\stm32cube\bin\cube.exe` | 在用户级 PATH |

DSH 进程可能继承旧的 PATH 快照，`Get-Command` 失败时先用绝对路径或临时前置目录，不要修改用户全局环境。

```powershell
$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
$env:PATH = "$gcc;$env:PATH"
Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue

arm-none-eabi-gcc --version
pyocd list --targets | Select-String stm32f103c8
```

`pyocd list` 在无板时输出 `No available debug probes`，这表示探针未连接，不表示 pyOCD 不可用。

## 探针

调试器是 CMSIS-DAP（如 `FIRE FireDAP CMSIS-DAP`），不是 ST-Link。因此：

- 不使用 STM32CubeProgrammer 处理本板
- 烧录、擦除、复位、GDB 统一走 pyOCD，命令见[硬件技能](../../f103-hw/references/flash-debug.md)

## 能力边界

| 事项 | 结论 |
| --- | --- |
| CubeMX2 处理 F1 | 不支持：其设备库只含 C5 器件，F1 查询返回未找到 |
| 传统 CubeMX 处理 F1 | 可安装，但只导出 Makefile/MDK/IAR，且需要额外的 Java 运行时 |
| 手写 CMake + 厂商 HAL1 源码 | 本路线采用，见[工程技能](../../f103-project/SKILL.md) |
