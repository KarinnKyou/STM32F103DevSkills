# 厂商资料树

野火小智资料在 `D:\DSH_Workspace\04_Embedded\f103_doc`，约 4.7 GB / 1.4 万文件。按下面的结构定位：

```
野火小智【<模块名>】模块资料\<模块>_<日期>\2-配套例程_接线说明\
    野火小智STM32F103C8核心板\{HAL, 标准库}\{Drivers, User, Project}
野火小智STM32F103C8T6核心板双USB款\...\2-参考例程\0-综合测试-HAL\
野火小智STM32F103C8T6扩展板\原理图_尺寸图\
```

- 模块例程在 `User\` 下：`main.c`、`stm32f1xx_it.c`、`bsp_<模块>.c|.h`
- 接线说明在 xlsx / PDF，与例程同目录
- 原理图给出扩展板各插座的引脚复用关系，是判断冲突的唯一可靠来源

## Drivers 必须取完整版

`0-综合测试-HAL\Drivers` 是唯一完整的一份。模块例程自带的 `Drivers\STM32F1xx_HAL_Driver\Inc` 被裁剪，缺少 `stm32f1xx_ll_*.h`（约 22 个头），编译到 `stm32f1xx_ll_utils.c` 会因头文件缺失失败。

所有工程统一从综合测试-HAL 复制 Drivers，再只替换 `src/` 下的应用代码。

## 源码编码

厂商源码是 GBK。按 UTF-8 读取会出现中文注释与字符串乱码，写回时也会损坏文件。

```powershell
[System.IO.File]::ReadAllText($path, [System.Text.Encoding]::GetEncoding('gb18030'))
```

写入同理，使用 `WriteAllText($path, $text, [Text.Encoding]::GetEncoding('gb18030'))`。

不要在 PowerShell 里拼内联 `python -c "...中文..."`：命令行中的中文字面量会被转码破坏，改用脚本文件传参。

## PDF 文本层

规格书与原理图的中文文本层无法提取（字体子集未嵌入 ToUnicode），提取结果为空白或乱码。改为渲染成图片再读：

```powershell
python D:\DSH_Workspace\04_Embedded\STM32F103DevSkills\tools\pdf2png.py <pdf> <outdir> --pages 3 --dpi 200
```

## 定位某个模块的例程

```powershell
Get-ChildItem 'D:\DSH_Workspace\04_Embedded\f103_doc' -Recurse -Filter 'bsp_*.h' -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match 'C8核心板' } |
  Select-Object -ExpandProperty FullName
```

引脚宏在 `bsp_<模块>.h` 顶部的 `#define` 段；不同模块例程会重复占用同一组引脚，组合使用前先读[引脚表](../../f103-hw/references/pinmap.md)。
