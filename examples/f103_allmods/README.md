> **本示例不含 `drivers/`**（HAL 源码体积大，本包不重复分发）。
> 编译前先按下面的步骤把 Drivers 取回来。

## 如何编译

```powershell
# 1) 取完整 Drivers（不能用模块例程里那份，缺 22 个 LL 头）
$full = 'D:\DSH_Workspace\04_Embedded\f103_doc\野火小智STM32F103C8T6核心板双USB款\野火STM32F103C8T6核心板双USB款_20241018\2-参考例程\0-综合测试-HAL\Drivers'
Copy-Item "$full\*" .\drivers -Recurse

# 2) 构建（路径按本机实际情况改）
$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
$env:PATH = "$gcc;" + "$env:LOCALAPPDATA\stm32cube\bundles\ninja\1.13.2+st.1\bin;" +
            "$env:LOCALAPPDATA\stm32cube\bundles\cmake\4.4.0+st.1\bin;" + $env:PATH
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_ASM_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_SIZE="$gcc\arm-none-eabi-size.exe"
cmake --build build

# 3) 烧录（DAP 探针）
pyocd flash -t stm32f103c8 --erase sector build\f103_allmods.elf
```

烧录后接 CH340 串口（115200 8N1）可看到启动自检与周期状态行，样例输出见 `boot_log.txt`。

---
# f103_allmods — F103C8T6 五模块一次性点亮

野火小智 **STM32F103C8T6 核心板 + 扩展板** 上同时插好 5 个模块后的一份自检/演示固件。
一次烧录，OLED 显示状态、光敏读数、编码器计数、舵机跟随、按键蜂鸣全部工作。

```
                     ┌──────────────────────────────┐
   OLED (I2C)  ──────┤ PB6/PB7  (OLED 座)           │
   光敏 (ADC)  ──────┤ PA5 (AO) / PA12 (DO)         │
   有源蜂鸣器  ──────┤ PB12  (通用IO3)              │── 4 kHz 方波（实测是"无源"特性）
   EC11 编码器 ──────┤ PA6/PA7 正交 + PA8 按键      │
   SG90 舵机   ──────┤ PB0   (TIM3_CH3, 50 Hz)      │
   调试串口    ──────┤ PA9/PA10 (USART1 115200)     │
                     └──────────────────────────────┘
```

## 实测接线（与"建议插法"表的差异）

| 模块 | 插在哪 | 实际占用引脚 | 与建议表是否一致 |
| --- | --- | --- | --- |
| OLED 0.96" I2C | **OLED 底座** | **PB6 = SCL / PB7 = SDA**（I2C1） | ❌ 表里写 B11/B10，实测座子是 **B6/B7** |
| 光敏电阻 | 通用IO1（VCC0 → 3.3 V） | **PA5 = AO（ADC1_IN5）/ PA12 = DO** | ⚠️ 表里只写 A5；A5 是模拟量，A12 是数字量 |
| 有源蜂鸣器 | 通用IO3（固定 3.3 V） | **PB12**，高电平驱动 | ✅ |
| EC11 编码器 | 编码器座（VCC2 → 3.3 V） | **PA6 = A / PA7 = B / PA8 = 按键** | ✅ |
| SG90 舵机 | 舵机接口1（5 V） | **PB0 = TIM3_CH3** | ✅ |

> 舵机口是扩展板固定 5 V，但 **5 V 要按下扩展板自锁开关才带电**。

## 行为

开机自检（约 3 秒）：

1. 舵机 500 µs → 2500 µs → 1500 µs 扫一遍；
2. 蜂鸣器先直流 200 ms、再 4 kHz 方波 600 ms；
3. 扫描两条 I2C 总线，打印每一路 ACK，选中真的那一路；
4. 整屏白 1 秒（面板级自检），再进入界面。

运行中：

- **拨编码器** → 舵机每格 6° 跟随，屏幕 `ENC` / `ANG` 同步；
- **按编码器按键** → 蜂鸣器 60 ms（4 kHz 方波），红灯同步亮；
- **遮光敏** → 屏幕 `LIGHT ADC` 与进度条实时变化；
- 串口每 500 ms 打印一行状态（`adc/min/max`、`enc`、`ab`、`sw/raw`、`ang`、`servo`、`oled`）。

屏幕 8 行布局：

```
F103C8T6 ALL-MODULES
OLED I2C PB6/PB7
LIGHT ADC 3259   79%
[#################        ]      <- 进度条
ENC     +3  ANG  108
SW 0  BEEP 0  DO 1
SERVO 1700 us
UP    42 s
```

## 编译

```powershell
$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
$cm  = "C:\Users\karin\AppData\Local\stm32cube\bundles\cmake\4.4.0+st.1\bin\cmake.exe"
$ninja = "C:\Users\karin\AppData\Local\stm32cube\bundles\ninja\1.13.2+st.1\bin\ninja.exe"

& $cm -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" `
    -DCMAKE_C_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
    -DCMAKE_ASM_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
    -DCMAKE_SIZE="$gcc\arm-none-eabi-size.exe"
& $cm --build build
```

实测占用：`text 18120 / data 120 / bss 2216`（64 KiB Flash / 20 KiB RAM）。

## 烧录

DAP 调试器，pyOCD，**不要**用 STM32CubeProgrammer：

```powershell
pyocd flash -t stm32f103c8 --erase sector build\f103_allmods.elf
pyocd commander -t stm32f103c8 -c "reset" -c "exit"
```

看日志：CH340 在 **COM5**（换 USB 口会变），115200 8N1，先开串口再复位。

## 四个实测坑（都已在代码里处理）

1. **OLED 座是 PB6/PB7，不是 B11/B10。**
   更坑的是：B10/B11 那排**没有上拉**，而 F1 的开漏输出**不会启用内部上拉**，
   于是总线悬空、每个地址都"ACK"（实测 112 个假地址），固件会以为屏在 B10/B11 上、
   初始化"成功"但屏幕全黑。本驱动改成"释放 = 输入 + 内部上拉 / 拉低 = 开漏输出"，
   并用"应答地址数 > 8 判定为假 ACK"的扫描顺序（先 B6/B7）定位真总线。

2. **EC11 每格只有 1 个正交边沿**（实测拨 10 格 → +10 边沿），
   不是常见的 4 边沿/格，所以 `ENC_EDGES_PER_DETENT = 1`；方向也按顺时针为正做了取反。

3. **"有源蜂鸣器"实际按无源特性驱动才稳定出声**：单给直流高电平不够，
   用 4 kHz 方波（`buzzer_tone()`），按键提示音也走方波。

4. **光敏模块的 AO 在 PA5、DO 在 PA12**；PA5 必须配成模拟输入走 ADC1_IN5，
   PA12 用内部上拉读数字量。混用时先看 `LIGHT ADC` 是否有随光变化的连续值。

## 文件

| 文件 | 作用 |
| --- | --- |
| `src/main.c` | 应用：时钟、GPIO、ADC、TIM2 正交解码、TIM3 舵机、总线发现、UI |
| `src/oled.c/.h` | 软件 I2C（引脚可配）+ SSD1306 128×64 文本/进度条 |
| `src/font6x8.c/.h` | 6×8 ASCII 字库（由 `tools/gen_font6x8.py` 从厂商 GBK 码表提取） |
| `src/usart.c` + `src/retarget.c` | USART1 115200 与 GCC 版 `printf` 重定向 |
| `src/it.c` | 中断向量（SysTick、TIM2 1 kHz） |
| `drivers/` | 厂商完整 HAL1 + CMSIS（取自核心板"综合测试-HAL"） |

## 验证状态

| 能力 | 证据 |
| --- | --- |
| 编译 → 烧录 → 运行 | ✅ `text 18120`，pyOCD 擦写 19456 B 无错误，串口连续输出 |
| OLED（PB6/PB7 座） | ✅ 屏上显示 8 行状态，实测确认 |
| 光敏（PA5 ADC） | ✅ 遮光时数值与进度条明显变化 |
| 蜂鸣器（PB12） | ✅ 按键 60 ms 提示音 |
| EC11（PA6/PA7/PA8） | ✅ 拨 10 格计 +10，方向正确；按键触发 |
| SG90（PB0） | ✅ 开机自检扫动，随编码器转动 |
| TIM3 PWM 寄存器 | ✅ CNT 变化、PSC=71、ARR=19999、CCR3=1500（1.5 ms） |
| TIM2 1 kHz | ✅ PSC=7199、ARR=9 |

以上均为**实机验证**，不是"编译通过"或"命令写好了"。

---

## 扩展：同时驱动七个模块（2026-11 实测）

在原五模块基础上增加 **热敏电阻** 与 **Rd-03 雷达**，七个模块同时工作，OLED 八行全部显示。

### 新增接线

| 模块 | 插哪 | 引脚 | 跳线 |
| --- | --- | --- | --- |
| 热敏电阻 | 通用IO2 | **A4 = AO**（ADC1_IN4），A11 = DO（本示例只读模拟量） | VCC1 → 3.3 V |
| Rd-03 雷达 | 通讯接口2 | **B10 = MCU TX / B11 = MCU RX**（USART3） | VCC4 → 3.3 V |

### 完整引脚分配（无冲突）

```
OLED      专用座    PB6 / PB7
光敏      IO1       A5(AO) / A12(DO)
热敏      IO2       A4(AO) / A11(DO)
蜂鸣器    IO3       B12
EC11      编码器座   A6 / A7 / A8
SG90      舵机1      PB0
Rd-03     通讯2      B10(TX) / B11(RX)
调试串口             PA9 / PA10
```

关键点：**Rd-03 用 B10/B11，OLED 用 B6/B7，互不冲突**，这是能同时跑七路的前提。
若 OLED 插在核心板排针（B10/B11），就会和雷达抢占同一组引脚。

### OLED 八行布局

```
0  F103C8T6 ALL-MODULES
1  OLED I2C PB6/PB7
2  LIGHT 3200  78%
3  NTC   2100  51%
4  ENC     +6  ANG  96
5  SW 0  BEEP 0  DO 1
6  SERVO 1500 us
7  RADAR  25cm ON
```

### 实测结果

上板验证：OLED 八行数值全部正常，雷达显示 `25cm ON`，编码器转动时 `ENC/ANG` 与舵机跟随，
蜂鸣器按键触发，光敏与热敏 ADC 读数随环境变化。串口同时输出整行状态：

```
t=   12s adc=3200 ntc=2100 min=... max=... pct= 78% do=1 enc=    +6 ab=11 sw=0/raw1 ang= 96 servo=1500 us radar=1/25cm oled=ok
```
---

## 再扩展：+DHT11、+循迹（八模块，2026-11 实测）

为了给循迹和 DHT11 腾位置，把**已单独验证过**的光敏和热敏换下：

| 模块 | 插哪 | 引脚 |
| --- | --- | --- |
| **DHT11** | 通用IO1（替换光敏） | **A5 = I/O**（3V3 / IO / NC / GND 四针正好对应） |
| **循迹** | 通用IO2（替换热敏） | **A4 = AO**，A11 = DO（本示例只读模拟量） |

DHT11 官方表就是 4 针 `3V3 / I/O / NC / GND`，插 IO1（`VCC0 / A5 / A12 / GND`）一位对一位，
所以**不需要杜邦线**，直插即可，数据脚是 A5。

### 两个新踩的坑

1. **I2C 探测必须在 USART3 初始化之前**。
   `oled_discover()` 会把候选引脚临时切成 GPIO 扫描，如果雷达的 USART3 已经初始化，
   就会把 B10/B11 抢走，雷达从此静默。现象：`radar=-1/-1cm`。
2. **DHT11 的 5 ms 阻塞读取会造成 USART3 溢出**，而 HAL 在溢出后**不会自动恢复接收**。
   必须实现 `HAL_UART_ErrorCallback` 重新武装，否则雷达会"跑着跑着就没数据"。
   加之前实测到 `radar=7942cm` 这种明显错误的距离——那是被截断的帧重新拼接的结果。

### 实测输出

```
t=   18s ntc=4010( 97%) enc=    +0 ab=11 sw=0/raw1 ang= 90 servo=1500 us dht=29C/50% radar=1/123cm oled=ok
```

OLED 对应显示：

```
0  F103C8T6 ALL-MODULES
1  OLED I2C PB6/PB7
2  TEMP 29C  HUMI 51%
3  TRACK 4010  97%
4  ENC     +0  ANG  90
5  SW 0  BEEP 0  DO 1
6  SERVO 1500 us
7  RADAR 120cm ON
```

### 当前完整引脚分配（八模块无冲突）

```
OLED      专用座    PB6 / PB7      (软 I2C)
DHT11     IO1       A5             (单总线)
循迹      IO2       A4(AO) / A11(DO)
蜂鸣器    IO3       B12
EC11      编码器座   A6 / A7 / A8
SG90      舵机1      PB0
Rd-03     通讯2      B10(TX) / B11(RX)
调试串口             PA9 / PA10
```