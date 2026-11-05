# 从 Keil 到 CMake

厂商例程按 MDK/ARMCC 编写，迁移到 GCC 时下列差异会直接导致编译或运行失败。按现象逐条核查。

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `stm32f1xx_ll_rcc.h: No such file or directory` | 模块例程的 `Drivers/Inc` 被裁剪，缺 LL 头 | 改用核心板"综合测试-HAL"的完整 Drivers |
| `multiple definition of 'HAL_InitTick'` | HAL 源码目录下的 `*_template.c` 被一起编译 | 构建里排除 `.*_template\.c$` |
| printf 编译链接通过但无输出 | 厂商用 `int fputc(int, FILE*)`，newlib 走 `_write` | 实现 `_write`，见 `template/src/retarget.c` |
| `undefined reference to '_write'` | 同上，缺少重定向实现 | 同上 |
| 产物没有 `.elf` 后缀 | 通用平台的 `CMAKE_EXECUTABLE_SUFFIX` 为空 | `set_target_properties(<tgt> PROPERTIES SUFFIX ".elf")` |
| 链接报找不到内存区域 / 地址越界 | Keil 用 `.sct` 分散加载，GCC 需要 `.ld` | 用模板 `.ld`，容量与目标芯片一致 |
| `printf("%f")` 输出空 | `nano.specs` 不含浮点格式化 | 链接加 `-u _printf_float`，Flash 占用约增 9 KB |
| 编译报 `unknown type name 'uint8_t'` | 缺少 CMSIS 设备头或未定义器件宏 | 定义 `STM32F103xB`、`USE_HAL_DRIVER` |
| 启动后不进 `main` | 启动文件选错（用了 arm 版）或 `.ld` 的入口与向量表不匹配 | 用 `Templates\gcc\startup_stm32f103xb.s` |

## 编译选项基线

```
-mcpu=cortex-m3 -mthumb -mfloat-abi=soft
-DUSE_HAL_DRIVER -DSTM32F103xB
--specs=nano.specs          # 需要浮点格式化时另加 -u _printf_float
-Wl,--gc-sections
```

链接阶段可能出现 `RWX LOAD segment` 警告，属内存属性描述问题，不影响运行。

## 定位顺序

1. 保留失败命令与第一条有效错误，日志很长时先检索 `error:`、`undefined reference`、`region .* overflowed`
2. 区分"编译期缺文件/缺符号"与"链接期重复定义/未定义引用"——前者查 include 与宏，后者查源文件集合
3. 只改构建配置或补实现文件，不为消除报错去改全局 PATH、编译器 ABI 或器件宏
