#ifndef __BSP_RD03_H
#define __BSP_RD03_H

#include "stm32f1xx.h"
#include <stdio.h>
#include "usart/bsp_usart.h"
#include "rd03/bsp_rd03.h"

/** @defgroup 用于命令�? SYSTEM_MODE_CONFIG  �? 命令�? �?,参数值的选项 (用于 SYSTEM_MODE_CONFIG 命令�?)
  * @{
  */

#define RD03_DEBUGGING_MODE      0x00000000  
#define RD03_REPORTING_MODE      0x00000004  
#define RD03_RUNNING_MODE        0x00000064
#define IS_SYSTEM_MODE_CONFIG_CMD_VALUE_PARAMETER_VALUE(PARAMETER_VALUE) (((PARAMETER_VALUE) == RD03_DEBUGGING_MODE) || \
                                    ((PARAMETER_VALUE) == RD03_REPORTING_MODE) || \
                                    ((PARAMETER_VALUE) == RD03_RUNNING_MODE))
/**
  * @}
  */

typedef struct
{
    uint8_t head[4];
    uint16_t data_size;
    uint8_t body_state;
    uint8_t body_range;
    uint8_t range_gate_energy[32];
    uint8_t tail[4];
    uint16_t rd03_firmware_protocol_version;//协议版本�?
    uint16_t rd03_firmware_buffer_size; //缓冲区大�?
}RD03_DataTypeDef;

extern RD03_DataTypeDef rd03data;
void Rd03_buff_Reset(USART_DataTypeDef* rx_data_buff);
uint8_t RD03_DataHandler(uint32_t mode,USART_DataTypeDef* rx_data_buff);
uint8_t Rd03_Debug_ModeData(USART_DataTypeDef* rx_data_buff);
uint8_t Rd03_Report_ModeData(USART_DataTypeDef* rx_data_buff);
uint8_t Rd03_Run_ModeData(USART_DataTypeDef* rx_data_buff);
uint32_t Check_RD03_DataMode(USART_DataTypeDef* rx_data_buff);

#endif /* __BSP_RD03_H  */
