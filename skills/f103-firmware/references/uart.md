# UART 收发（HAL1）

## 中断逐字节接收

轮询收发会卡住主循环，持续输出的模块（雷达、GPS、串口屏）用中断逐字节收：

```c
HAL_UART_Receive_IT(&huart3, &rx_byte, 1);          /* 启动一次 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    push_byte(buf, rx_byte);                        /* 入缓冲或状态机 */
    HAL_UART_Receive_IT(h, &rx_byte, 1);            /* 必须重新武装 */
}
```

回调里漏掉重新武装，只会收到第一个字节。

## 溢出不会自愈

HAL 在检测到溢出（ORE）后**停止接收**，不会自动恢复。只要主循环里存在毫秒级阻塞操作（单总线读取、长延时、Flash 擦除），115200 下就可能溢出。必须补上：

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART3) {
        linelen = 0;
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}
```

未处理时表现为"运行一段时间后串口静默"，或解析出明显越界的数值（被截断的帧首尾相接）。

## 分帧策略

| 模块输出 | 策略 |
| --- | --- |
| ASCII 行（`ON\r\n`、`Range 35\r\n`） | 按 `\n` 切行，去掉 `\r` 后逐行匹配 |
| 固定头部二进制帧 | 按帧头同步（如 `FD FC FB FA`），读长度字段后整帧拷贝 |
| 连续无分隔数据 | 用空闲中断（IDLE）或定时判定帧结束 |

一条命令可能产生多行、且部分字段只在变化时上报，解析器不能假设"每帧都包含全部字段"，须保留上一有效值。

## 联调手段

1. 先把收到的原始字节按十六进制打印，确认波特率与接线；协议对不上时才看得到模块究竟发了什么。
2. 再用行/帧解析，打印解析结果与计数器。
3. 最后接入业务逻辑。

## 抓启动日志

不少例程只在启动时打印一次，随后进入空循环。若先复位再开串口，抓到的是尾部残字节（表现为乱码）。正确顺序是先开串口、再复位目标：

```powershell
$sp.Open()
pyocd commander -t stm32f103c8 -c "reset" -c "exit"
Start-Sleep -Seconds 3
$sp.ReadExisting()
```

串口设备名会随插拔变化（COM5 / COM6 等），按设备名反查端口，不要写死：

```powershell
(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'CH340.*\((COM\d+)\)' }) |
  ForEach-Object { [regex]::Match($_.Name,'COM\d+').Value }
```

## 与 GPIO 复用的器件共存

软 I2C/软 SPI 探测、GPIO 扫描都会临时改写引脚模式。若同一组引脚上已有串口外设，**先完成探测再初始化串口**，否则探测会把引脚改回 GPIO，串口从此收不到数据。

```c
oled_discover();      /* 可能改写 PB10/PB11 */
rd03_init();          /* 之后再把它们配成 USART3 */
```
