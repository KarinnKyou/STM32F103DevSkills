# f103_rd03 — 安信可 Rd-03 雷达最小验证

把厂商的**标准库**例程移植到 HAL：串口层重写，`bsp_rd03.c` 的解析代码原样复用。

## 接线（扩展板 通讯接口2）

Rd-03 是 5 针模块，直插 `通讯接口2` 正好一一对应：

| 模块引脚 | 通讯接口2 针位 | 核心板引脚 | 说明 |
| --- | --- | --- | --- |
| 3V3 | 1（VCC4） | — | **VCC4 跳线必须拨到 3.3V** |
| GND | 2 | — | |
| TX | 3 | **PB11** | MCU 的 USART3_RX |
| RX | 4 | **PB10** | MCU 的 USART3_TX |
| OT2 | 5 | B8 | 检测结果输出，本例程不用 |

调试串口走核心板 CH340（USART1，PA9/PA10，115200），与雷达互不干扰。

### 引脚冲突

`PB10/PB11` 同时是 OLED 的"核心板排针"接法。所以：

- OLED 插在**核心板排针** → 与雷达冲突，不能同时用
- OLED 插在**扩展板专用座（PB6/PB7）** → 与雷达共存，互不影响

## 编译

```powershell
# 取完整 Drivers（不能用模块例程里那份，缺 22 个 LL 头）
$full = 'D:\DSH_Workspace\04_Embedded\f103_doc\野火小智STM32F103C8T6核心板双USB款\野火STM32F103C8T6核心板双USB款_20241018\2-参考例程\0-综合测试-HAL\Drivers'
Copy-Item "$full\*" .\drivers -Recurse

$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
$env:PATH = "$gcc;" + "$env:LOCALAPPDATA\stm32cube\bundles\ninja\1.13.2+st.1\bin;" +
            "$env:LOCALAPPDATA\stm32cube\bundles\cmake\4.4.0+st.1\bin;" + $env:PATH
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_ASM_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_SIZE="$gcc\arm-none-eabi-size.exe"
cmake --build build

pyocd flash -t stm32f103c8 --erase sector build\f103_app.elf
```

## 预期输出（CH340，115200）

启动横幅：

```
== Rd-03 test (USART3 PB10/PB11 @115200) ==
waiting for radar frames...
```

之后每收到一行，先打原始字节再打文本：

```
RAW[12]: 4F 4E 20 52 61 6E 67 65 20 30 30 0A
TXT: ON Range 00
```

- 有人：`ON ...`，无人：`OFF`
- 若 `RAW` 里出现 `FD FC FB FA` 开头，说明模块处于**二进制协议**模式，
  需要用 `Rd03_Debug_ModeData()` 那条路径解析，而不是文本比较

## 移植要点（三个坑）

1. `#include "stm32f10x.h"` → `"stm32f1xx.h"`
2. `uint8_t buf[...] = {NULL};` → `{0}`（GCC 14 把 void* 转整数当错误）
3. 厂商的 RX 是轮询读取；这里改成 HAL 中断逐字节收，**收到 `\n` 置 `flag`**，
   主循环看到 `flag` 就解析并复位缓冲

## 验证状态：已实机验证（2026-11）

上板实测（通讯接口2，VCC4 = 3.3V）：

```
RAW[4]: 4F 4E 0D 0A 52     ← "ON\r\nR"
TXT: ON
Range 35
```

- 能稳定收到检测事件：15 秒内 143 条 `ON`（人一直在检测范围内）
- 距离读数读到 `Range 35`（35 cm）
- **距离只在数值变化时才发**：人不动时只周期发 `ON`，不会重复 `Range`

模块的实际输出是逐行 ASCII，一条事件两行：

```
ON\r\n
Range 35\r\n
```

所以按行收、按行处理即可；`Range <n>` 用单独的分支提取数值。
若 `RAW` 里出现 `FD FC FB FA` 帧头，说明模块切到了二进制协议模式，
需要走另一条解析路径（厂商的 `Rd03_Debug_ModeData`）。
