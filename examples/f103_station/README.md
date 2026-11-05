# f103_station — 桌面环境监测站

野火小智 **STM32F103C8T6 核心板 + 扩展板**上的一个小项目：**九个模块协同工作**，
OLED 四页仪表盘、事件写 Flash 掉电不丢、旋钮调灵敏度、蜂鸣器分级告警。

不是"逐个模块验证"的测试程序，而是一个有状态的系统：**采集 → 判警 → 执行 → 显示 → 记录**。

## 模块与引脚（全部实机验证）

| # | 模块 | 插位 | 引脚 | 说明 |
| --- | --- | --- | --- | --- |
| 1 | **OLED 0.96"** | OLED 专用座 | B6(SCL) / B7(SDA) | 软 I2C，上电自动探测 |
| 2 | **DHT11** | 通讯接口2（直插，3V3 朝 VCC4） | **PB10** | 4 针模块，数据脚落在第 4 针 |
| 3 | **光敏** | 通用IO1 | A5(AO) / A12(DO) | |
| 4 | **热敏** | 通用IO2 | A4(AO) / A11(DO) | |
| 5 | **EC11 编码器** | 编码器座 | A6 / A7 / A8 | 旋转翻页，按键静音 |
| 6 | **SG90 舵机** | 舵机接口1 | PB0 (TIM3_CH3) | 温度指针，5 V |
| 7 | **有源蜂鸣器** | 通用IO3 | B12 | 分级告警音 |
| 8 | **W25Q64** | 飞线 | **A15(CS) B3(DO) B4(DI) B5(CLK)** + 3V3/GND | 软 SPI |
| 9 | **电位器** | 飞线 | **PA0** + 3V3/GND | 调光照阈值 |

### 为什么 W25Q64 用软件 SPI

硬件 SPI1 在 `A4/A5/A6/A7`，与光敏(A5)、热敏(A4)、编码器(A6/A7) 全部冲突。
用软件 SPI 可以把它挪到任意空闲脚，于是按模块丝印顺序 `CS, DO, DI, CLK`
接到了扩展板排针上连续的 `A15, B3, B4, B5`。

其中 A15/B3/B4 是 JTAG 脚，`w25q64_init()` 里关掉 JTAG、保留 SWD：

```c
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_SWJ_NOJTAG();
```

## 行为

**采集**
- DHT11：每 2 s 读温湿度
- 光敏 / 热敏 / 电位器：每 200 ms 采一次 ADC

**判警**（任一成立即 ALARM，按位记录）

| 条件 | 位 | 阈值 |
| --- | --- | --- |
| 温度过高 | 0x01 | ≥ 30 °C |
| 湿度过高 | 0x02 | ≥ 70 % |
| 光照过暗 | 0x08 | < 电位器设定值（200–3800） |

告警时：蜂鸣器每 0.9 s 短鸣、绿灯 5 Hz 闪、OLED 首行显示 `!ALARM!`。

**记录**：每次告警状态**变化**写一条 8 字节记录到 W25Q64（最多 512 条），上电自动恢复计数。

**舵机**：按温度当指针，10 °C → 20°，40 °C → 160°。

**交互**：旋转编码器翻页；按键在非日志页切换静音，在日志页清空日志。

## OLED 四页

```
页0 总览             页1 环境            页2 环境(2)        页3 日志
STATION     OK       ENVIRONMENT        ENVIRONMENT        LOG 14/512
T 28C  H 48%         LIGHT 3220  ok     LIGHT 3220  ok     #12 t=42s
LIGHT 3220 NTC 1920  NTC   1920          NTC   1920             mask 0x04
DARK<1327 (POT)      T 28C  H 48%       T 28C  H 48%       #13 t=51s
ALARM# 0     MU 0    [光照条]           [光照条]                mask 0x08
[光照条]             DARK<1327          DARK<1327          #14 t=63s
SERVO  92 deg        SERVO  92 deg      LOG 14/512 FLASH ok    mask 0x00
PAGE 1/4 ENC=TURN    PAGE 2/4           PAGE 3/4           PAGE 4/4 BTN=CLR
```

串口每秒一行：

```
t=    5s T=28C H=48% light=3262 ntc=1922 near=1327cm page=0 mute=0 alarm=0x00 log=14 oled=ok
```

## 编译与烧录

```powershell
# 1) 取完整 Drivers（本示例不含 drivers/）
$full = 'D:\DSH_Workspace\04_Embedded\f103_doc\野火小智STM32F103C8T6核心板双USB款\野火STM32F103C8T6核心板双USB款_20241018\2-参考例程\0-综合测试-HAL\Drivers'
Copy-Item "$full\*" .\drivers -Recurse

# 2) 构建
$gcc = "$env:LOCALAPPDATA\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\bin"
$env:PATH = "$gcc;" + "$env:LOCALAPPDATA\stm32cube\bundles\ninja\1.13.2+st.1\bin;" +
            "$env:LOCALAPPDATA\stm32cube\bundles\cmake\4.4.0+st.1\bin;" + $env:PATH
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_ASM_COMPILER="$gcc\arm-none-eabi-gcc.exe" `
  -DCMAKE_SIZE="$gcc\arm-none-eabi-size.exe"
cmake --build build

# 3) 烧录（DAP）
pyocd flash -t stm32f103c8 --erase sector build\f103_station.elf
```

## 实测（2026-11）

```
W25Q64: ok (id=0xEF4017)
flash log: 14 record(s)
t=  5s T=28C H=48% light=3262 ntc=1922 near=1327cm page=0 alarm=0x00 log=14 oled=ok
```

- 温湿度、光照、热敏、电位器阈值全部正常
- Flash 掉电后计数从 14 恢复，说明写入与恢复都对
- 温度曾达 31 °C，触发 `alarm=0x01` 并写入日志，降温后自动恢复
- 四页显示、编码器翻页、按键静音均正常

### 关于雷达

Rd-03 原本也在方案里（USART3，PB10/PB11）。但在一次改接后模块**串口不再输出**：
引脚配置正确、TX 空闲高电平（内部下拉也拉不低，证明模块在驱动线路）、波特率无误，
但 0 帧到达、断电重启无效——判定模块串口损坏，已从项目中移除。

## 目录

```
src/
  main.c        项目逻辑
  app.h         引脚与状态声明
  oled.c|.h     SSD1306 + 软 I2C（运行时探测总线）
  dht11/        单总线驱动（HAL，数据脚 PB10）
  w25q64.c|.h   软 SPI Flash 驱动（A15/B3/B4/B5）
  usart.c|.h    调试串口
  retarget.c    GCC printf 重定向
startup/        GCC 启动文件
linker/         64 KiB Flash / 20 KiB RAM
```
