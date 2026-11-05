# I2C 总线探测（先探测，再定引脚）

## 为什么必须探测

同一颗模块在不同插座上接的是不同引脚。以 OLED 为例：

| 接法 | SCL | SDA |
| --- | --- | --- |
| 核心板排针 | PB10 | PB11 |
| 扩展板 OLED 专用座 | PB6 | PB7 |

靠"烧一版、看屏幕亮不亮、不行再换一版"来试代价最高，且容易在错误的引脚上把时序改坏。正确做法是先扫描总线确认应答，再用命中的那组引脚初始化驱动。

## 用法

模板里的 `src/iic_probe.c` 提供：

```c
iic_probe_result_t iic_probe_pairs(const iic_pair_t *pairs, int count, uint8_t addr7);
```

```c
#include "iic_probe.h"

USARTX_Init();                                  /* 先有串口，才能看报告 */
iic_probe_result_t r = iic_probe_pairs(iic_pairs_f103c8_example,
                                      iic_pairs_f103c8_example_count, 0x3C);
if (r.found) {
    /* r.name 是命中的那组引脚，据此初始化驱动 */
}
```

串口输出形如：

```
  bus OLED socket PB6/PB7    0x3C  (ACK)
  -> found device on OLED socket PB6/PB7 (addr 0x3C)
```

## 三个约束

1. **先校准扫描器**：SDA 被拉低时，"读不到高电平"会被误判成 ACK，于是每个地址都"应答"。必须先用一组确定可用的接法跑一遍，确认扫描器只报出一个地址，再用于未知接线。
2. **只探目标地址**：OLED 通常应答 `0x3C`（写地址 `0x78`）。扫全表既慢又容易把别的器件当成目标。
3. **不用"屏幕亮了"当判据**：面板可能只是上电发光；串口打印的 ACK 才是证据。

## 引脚表

`iic_pairs_f103c8_example[]` 已按"扩展板插座 → 核心板排针"的顺序列出候选。换板子时改这张表，探测逻辑不变。
