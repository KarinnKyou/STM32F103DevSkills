# STM32F103DevSkills

AI-assisted development skills for the **STM32F103C8T6** (Cortex-M3, 64 KiB Flash, 20 KiB RAM)
on a Wildfire "Xiaozhi" core board plus its expansion board.

中文说明见 [README.zh-CN.md](README.zh-CN.md).

Same shape as [STM32C5DevSkills](https://github.com/keysking/STM32C5DevSkills), but a **completely different toolchain**:

| | STM32C5DevSkills | This package |
| --- | --- | --- |
| Configurator | CubeMX2 + `.ioc2` | **No generator** — hand-written CMake |
| HAL | HAL2 | HAL1 |
| Flashing | STM32CubeProgrammer (ST-Link) | **pyOCD (CMSIS-DAP)** |
| Debugging | ST-Link GDB server | **pyOCD gdbserver + arm-none-eabi-gdb** |

Do not mix the two routes. CubeMX2 has no F103 device in its database, and the standard-library /
HAL1 project layout is far from the C5 one.

## Installation

Copy the skill directories into your project's `.agents/skills/`:

```powershell
Copy-Item .\skills\* <your-project>\.agents\skills\ -Recurse
```

Or use the bundled script (installs into the workspace that contains this repo by default):

```powershell
.\install.ps1 -Target 'D:\path\to\your-project'
```

Once copied, the harness discovers the skills automatically — no restart needed.

## Skills

| Skill | Purpose |
| --- | --- |
| `f103-env` | Locate and verify the toolchain (GCC / CMake / Ninja / pyOCD), vendor documentation tree, GBK and PDF encoding traps |
| `f103-project` | Build a CMake project; `template/` ships the linker script, GCC startup file, `printf` retarget and I2C bus probe |
| `f103-firmware` | HAL1 application code, pin map and example locations for the module kit, Keil → GCC porting rules |
| `f103-hw` | pyOCD flashing and debugging, GDB recipes, pin tables, expansion-board socket map |

Each skill follows the standard layout: `SKILL.md` (front matter + numbered steps), `references/`
(details, loaded on demand), `agents/openai.yaml` and a `LICENSE`.

## Examples

Three buildable projects under `examples/`. Each one needs the vendor `Drivers/` tree copied in
(see the per-example README); the HAL sources are deliberately not redistributed here.

| Example | What it does |
| --- | --- |
| `f103_station` | **Desk environment monitor** — nine modules, four OLED pages, Flash event log |
| `f103_allmods` | Concurrent self-test: OLED + DHT11 + tracking sensor + buzzer + encoder + servo + radar |
| `f103_rd03` | Minimal radar bring-up, prints raw bytes before parsing |

`f103_station` uses:

| Module | Role | Pins |
| --- | --- | --- |
| OLED 0.96" | four-page dashboard | B6 / B7 (software I2C) |
| DHT11 | temperature and humidity | PB10 (1-wire) |
| Photoresistor | ambient light | A5 (ADC) |
| Thermistor | temperature trend | A4 (ADC) |
| EC11 encoder | page switching, mute | A6 / A7 / A8 |
| SG90 servo | temperature gauge | PB0 (TIM3_CH3) |
| Active buzzer | alarm | B12 |
| W25Q64 | event log, survives power loss | A15 / B3 / B4 / B5 (software SPI) |
| Potentiometer | light-threshold knob | PA0 (ADC) |

## Verification status

Three levels — quote the level when you rely on a claim.

### A — verified on hardware

| Capability | Evidence |
| --- | --- |
| Build → flash → run | ELF downloaded, LEDs cycle |
| GPIO output | Active buzzer sounds |
| ADC | Potentiometer reads 3–4094 across full travel |
| UART | CH340 at 115200, printf output clean |
| I2C | OLED lights up on both the dedicated socket (PB6/PB7) and the header (PB10/PB11) |
| SPI | W25Q64, JEDEC ID `0xEF4017`, write-then-read verified |
| TIM encoder | EC11 quadrature counts correctly in both directions |
| TIM PWM | SG90 sweeps 500–2500 µs |
| GDB | Breakpoint hits `main`, `bt` shows the expected frames |
| Multi-module | Nine modules running together, OLED pages updating |
| Flash log | 14 records written, counter restored after power cycle |
| DHT11 | 28–29 °C / 47–49 % read back reliably |

### B — compiles only

- Rain and soil-moisture vendor examples
- HardFault analysis flow has not been exercised yet

### C — not covered

- SD card, CAN, USB — not part of this kit

## Engineering constraints worth knowing

These are documented in the skills as rules, not as anecdotes:

1. **One OLED, two sockets, different pins.** Dedicated socket is `SCL=PB6 / SDA=PB7`; the core-board
   header is `SCL=PB10 / SDA=PB11`. Probe the bus instead of guessing.
2. **`printf("%f")` prints nothing** with `--specs=nano.specs`. Add `-u _printf_float`; costs about 9 KiB of Flash.
3. **HAL `*_template.c` files must be excluded** from the build, otherwise `multiple definition of HAL_InitTick`.
4. **Vendor `printf` retargeting uses Keil's `fputc`**, which does nothing under GCC — implement `_write`.
5. **The trimmed `Drivers/Inc` shipped with module examples lacks 22 LL headers.** Take the complete
   `Drivers` from the core board's "comprehensive test" example.
6. **Peripheral init order matters.** A software-I2C probe reconfigures pins as GPIO; if USART3 was
   initialized first, the probe steals PB10/PB11 and the UART goes silent. Probe the bus first.
7. **HAL does not recover from UART overrun.** A blocking read (DHT11 takes ~5 ms) can overflow the
   receiver; implement `HAL_UART_ErrorCallback` to re-arm, or the port dies silently.
8. **Vendor sources are GBK encoded.** Read them as GB18030; PDF text layers have no usable Chinese
   text, so render pages to images instead.

## Repository layout

```
skills/
  f103-env/        SKILL.md + references/{tools,sources}.md + agents/ + LICENSE
  f103-project/    SKILL.md + references/gcc-port.md + template/ + agents/ + LICENSE
  f103-firmware/   SKILL.md + references/{modules,i2c-probe,uart}.md + agents/ + LICENSE
  f103-hw/         SKILL.md + references/{flash-debug,pinmap,expansion-board}.md + agents/ + LICENSE
examples/
  f103_station/    nine-module monitor: OLED dashboard + Flash event log
  f103_allmods/    concurrent multi-module self-test
  f103_rd03/       minimal radar bring-up with raw byte dump
tools/
  port_modules.ps1  Port a vendor module example to a CMake project and build it
  pdf2png.py        Render schematics or datasheets to images
  pin_scan.py       Extract GPIO macro definitions from example sources
  extract_pins.py   Batch-extract pin macros for every module
```

## Environment baseline

- `cube` 0.10.3 / CubeMX2 1.1.1 (not used as a generator here, only as the toolchain bundle source)
- `arm-none-eabi-gcc` 14.3.1, CMake 4.4.0, Ninja 1.13.2 (from the cube bundle, not on `PATH`)
- `pyocd` 0.46.1 with a `FIRE FireDAP CMSIS-DAP` probe
- Vendor documentation tree: about 4.7 GB, 14 403 files

## License

MIT, see `LICENSE`. Vendor example code, HAL sources and the STMicroelectronics / Wildfire trademarks
belong to their respective owners. This package distributes instructions and configuration only,
not those binaries.
