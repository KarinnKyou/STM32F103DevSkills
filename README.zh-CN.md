# STM32F103DevSkills

面向 **STM32F103C8T6**（Cortex-M3 / 64 KiB Flash / 20 KiB RAM，野火小智核心板 + 扩展板）的
AI 辅助开发技能包。

与 [STM32C5DevSkills](https://github.com/keysking/STM32C5DevSkills) 同构，但走的是**完全不同的技术路线**：

| | STM32C5DevSkills | 本包 |
| --- | --- | --- |
| 生成器 | CubeMX2 + `.ioc2` | **不用生成器**，手写 CMake |
| 库 | HAL2 | HAL1 |
| 烧录 | STM32CubeProgrammer（ST-Link） | **pyOCD（CMSIS-DAP）** |
| 调试 | ST-Link GDB server | **pyOCD gdbserver + arm-none-eabi-gdb** |

两条路线不要互相套用：CubeMX2 不认 F103（实测 `Device not found`），
而标准库/HAL1 的工程结构与 C5 差得远。

## 安装

把 `skills/` 下的目录复制到目标工程的 `.agents/skills/`：

```powershell
Copy-Item .\skills\* <你的工程>\.agents\skills\ -Recurse
```

或直接用自带脚本（默认装到本仓库所在工作区）：

```powershell
.\install.ps1 -Target 'D:\path\to\your-project'
```

装好后 DSH 会自动发现，无需重启。

## 技能清单

| 技能 | 用途 |
| --- | --- |
| `f103-env` | 工具链定位与验证（GCC/CMake/Ninja/pyOCD）、资料树结构、GBK 与 PDF 编码坑 |
| `f103-project` | CMake 工程搭建；`template/` 里有链接脚本、GCC 启动文件、printf 重定向、I2C 总线探测 |
| `f103-firmware` | HAL1 应用代码、12 个模块的引脚与例程位置、Keil→GCC 移植坑 |
| `f103-hw` | pyOCD 烧录/调试、GDB、引脚表、扩展板接口表 |

## 验证状态（三级，引用时请带级别）

### A｜实机验证

| 能力 | 证据 |
| --- | --- |
| 编译 → 烧录 → 运行 | ELF 下载后 LED 轮闪 |
| GPIO 输出 | 有源蜂鸣器（PB9）发声 |
| ADC | 电位器（PA1）读数 3 ~ 4094，满量程 |
| 串口 | CH340 @115200，中文正常 |
| I2C | OLED：扩展板专用座走 **PB6/PB7**，核心板排针走 **PB10/PB11**，两处均点亮 |
| SPI | W25Q64（CS=PA4），FlashID `0xEF4017`，写入后读回比对通过 |
| TIM 编码器 | EC11（PA6/PA7），正反转计数正确 |
| TIM PWM | SG90 舵机（PB0）扫动 |
| GDB | 断点命中 `main`，`bt` 调用栈正确 |
| 多模块同时 | 综合自检固件：ADC/编码器/舵机/蜂鸣器/OLED 同时工作正常 |
| UART 模块 | Rd-03 雷达（USART3 PB10/PB11）：检测到人，距离读数 25~35 cm |
| **八模块同时** | OLED(PB6/7) + DHT11(PB10) + 循迹(A4) + 蜂鸣器(B12) + EC11(A6/7/8) + 舵机(PB0) + Rd-03(B10/11)，OLED 八行数值正常 |
| DHT11 | 单总线，通讯接口2 直插，数据脚 PB10，读数 29 °C / 49 % |

### B｜仅编译验证

- 热敏、雨量/土壤的厂商例程（热敏已另行实机验证）
- RD03 已上板验证，见 `examples\f103_rd03`（注意 USART3 与 OLED 排针接法冲突）

### C｜未涉及

- HardFault 现场分析
- SD 卡、CAN、USB 等本套件没有的模块

## 工程约束与已知问题

1. **同一颗 OLED，两个插座引脚不同**
   - 扩展板 OLED 专用座：`SCL=PB6, SDA=PB7`（与丝印、原理图一致）
   - 核心板排针：`SCL=PB10, SDA=PB11`（官方 xlsx）
   - 对策：用 `f103-project/template/src/iic_probe.c` **先探测再初始化**，不要猜。

2. **`printf("%f")` 静默无输出**
   `--specs=nano.specs` 不含浮点格式化；加 `-u _printf_float`，Flash 约增 9 KiB。

3. **HAL 的 `*_template.c` 不能一起编译**
   会造成 `multiple definition of HAL_InitTick`，必须在 CMake 里排除。

4. **厂商 Keil 例程的 printf 重定向是 `fputc`**
   GCC 下无效，需用模板里的 `retarget.c` 实现 `_write`。

5. **厂商例程默认行为常常不是自动循环**
   例如蜂鸣器例程要按 KEY1 才响，别把它当成接线故障。

6. **模块例程的 `Drivers/Inc` 是精简版**
   缺 22 个 `stm32f1xx_ll_*.h`，编译会失败；取核心板「综合测试-HAL」里的完整 `Drivers`。

7. **外设初始化顺序会互相破坏**：OLED 总线探测会把引脚临时切成 GPIO，
   若雷达 USART3 已初始化就会抢走 B10/B11 → 雷达静默。先探测，再初始化串口。

8. **HAL 的 UART 溢出不会自愈**：阻塞操作（DHT11 约 5 ms）会造成 115200 接收溢出，
   HAL 溢出后停止接收，必须实现 `HAL_UART_ErrorCallback` 重新武装。

9. **野火源码是 GBK 编码**
   用 UTF-8 读会乱码；按 GB18030 解码。PDF 的中文文本层也提取不出，需渲染成图。

## 目录

```
skills/            四个技能（复制到 .agents/skills 使用）
  f103-env/        SKILL.md + references/{tools,sources}.md + agents/ + LICENSE
  f103-project/    SKILL.md + references/gcc-port.md + template/ + agents/ + LICENSE
  f103-firmware/   SKILL.md + references/{modules,i2c-probe,uart}.md + agents/ + LICENSE
  f103-hw/         SKILL.md + references/{flash-debug,pinmap,expansion-board}.md + agents/ + LICENSE
examples/
  f103_station/    小项目：桌面环境监测站（九模块 + OLED 四页 + Flash 日志）
  f103_allmods/    八模块并发自检（OLED+DHT11+循迹+蜂鸣器+编码器+舵机+雷达）
  f103_rd03/       雷达最小验证（含原始字节打印）
tools/
  port_modules.ps1 批量把一个模块的例程移植成 CMake 工程并编译
  pdf2png.py       把原理图/规格书渲染成图片（中文文本层提取不出时的替代方案）
  pin_scan.py      从例程源码里抽 GPIO 宏定义
  extract_pins.py  批量抽取所有模块的引脚宏
```

## 环境基线

- `cube` 0.10.3 / CubeMX2 1.1.1（本包不使用生成器，仅用于取工具链）
- `arm-none-eabi-gcc` 14.3.1、CMake 4.4.0、Ninja 1.13.2（cube bundle，不在 PATH）
- `pyocd` 0.46.1 + `FIRE FireDAP CMSIS-DAP` 探针
- 资料树约 4.7 GB，14403 个文件

## 许可

MIT（见 `LICENSE`）。厂商例程、HAL 源码、STMicroelectronics 与野火的商标归各自权利人；
本包只分发指令与配置，不分发其二进制。
