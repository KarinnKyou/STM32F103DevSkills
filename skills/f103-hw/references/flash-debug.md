# 烧录与调试（pyOCD + CMSIS-DAP）

## 连接与识别

```powershell
pyocd list                                   # 探针列表；无板时输出 No available debug probes
pyocd list --targets | Select-String stm32f103c8
pyocd commander -t stm32f103c8 -c "show target" -c "exit"
```

`show target` 返回的 Flash 大小决定链接脚本。国内板常见"标 C8、实为 CBT6（128 KiB）"或兼容片（CS32/GD32/APM32）。容量与 `STM32F103C8Tx_FLASH.ld` 的 64 KiB 不符时，改 `.ld` 的 `LENGTH` 并同步器件宏（`STM32F103xB` / `STM32F103xE`）。

## 烧录

```powershell
pyocd flash -t stm32f103c8 --erase chip build\app.elf
pyocd erase -t stm32f103c8 --chip            # 单独擦除
pyocd flash -t stm32f103c8 app.bin --base-address 0x08000000
```

- 校验默认开启，失败以非零退出
- 优先给 ELF：自带地址与符号
- `--erase sector` 在某些情况下会报告 `identical` 而跳过写入；需要确保重写时用 `--erase chip`

## 复位与核对

```powershell
pyocd commander -t stm32f103c8 -c "reset" -c "exit"
```

`-c "reset"` 复位后直接运行。pyOCD 0.46 的参数语法：

| 写法 | 结果 |
| --- | --- |
| `-c "reset run"` | 报 `unexpected value for reset_type option ('run')` |
| `-c "resume"` | 报 `unrecognized command 'resume'` |
| `-c "reset"` | 正常，复位并运行 |

烧录后核对向量表：

```powershell
pyocd commander -t stm32f103c8 -c "read32 0x08000000" -c "read32 0x08000004" -c "exit"
```

`0x08000000` 应为 RAM 顶（如 `0x20005000`），`0x08000004` 应等于 ELF 的 `Entry`（Thumb 位为 1）。

## GDB 调试

```powershell
# 终端 A
pyocd gdbserver -t stm32f103c8 --port 3333

# 终端 B：ELF 必须作为 gdb 的参数，否则没有符号表
arm-none-eabi-gdb build\app.elf `
  -ex "set confirm off" `
  -ex "target extended-remote localhost:3333" `
  -ex "monitor reset halt" `
  -ex "break main" -ex "continue" `
  -ex "bt" -ex "detach"
```

- 漏掉 ELF 参数时表现为 `No symbol table is loaded`，随后 batch 模式也会停在提示符不退出
- 读取运行期变量：`print <symbol>`，比在串口里加日志更直接
- Fault 现场：读 `x/8xw $sp`、`$lr`，以及 SCB 的 `CFSR`（`0xE000ED28`）、`HFSR`（`0xE000ED2C`）、`BFAR`（`0xE000ED38`）

## 启动相关

- **BOOT0 必须为 0** 才从 Flash 启动；BOOT0=1 进入系统存储器（串口下载模式），表现为"烧录成功但程序不跑"
- 用 DAP 供电时不要同时接外部电源；扩展板有 12 V / 5 V / USB 切换开关
