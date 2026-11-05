/**
  ******************************************************************************
  * @file       bsp_rd03.c
  * @author     embedfire
  * @version     V1.0
  * @date        2024
  * @brief      rd03协议接口
  ******************************************************************************
  * @attention
  *
  * 实验平台  ：野�? STM32F103C8T6-STM32�?发板 
  * 论坛      ：http://www.firebbs.cn
  * 官网      ：https://embedfire.com/
  * 淘宝      ：https://yehuosm.tmall.com/
  *
  ******************************************************************************
  */
  
#include "rd03/bsp_rd03.h"
#include "usart/bsp_usart.h"
#include <string.h>

RD03_DataTypeDef rd03data;

/**
 * @brief  清空接收buf,准备接收反馈数据
 * @param  rx_data_buff：接收数据buff
 * @retval �?
 */
void Rd03_buff_Reset(USART_DataTypeDef* rx_data_buff)
{
    memset(rx_data_buff->data,0,rx_data_buff->len);
    rx_data_buff->flag = 0;
    rx_data_buff->len = 0;
}

/**
 * @brief  RD03的数据处理函�?
 * @param  mode：RD03当前的工作模�?
 * @param  rx_data_buff：接收数据buff
 * @retval  1：检测有�? 0：检测无�? 其它:不合法数�?
 */
uint8_t RD03_DataHandler(uint32_t mode,USART_DataTypeDef* rx_data_buff)
{
    if(mode == RD03_DEBUGGING_MODE)
    {
        return Rd03_Debug_ModeData(rx_data_buff);
    }
    else if(mode == RD03_REPORTING_MODE)
    {
        return Rd03_Report_ModeData(rx_data_buff);
    }
    else if(mode == RD03_RUNNING_MODE)
    {
        return Rd03_Run_ModeData(rx_data_buff);
    }

    return 2;
}

/**
 * @brief  �?查RD03的数据接收包类型
 * @param  rx_data_buff：接收数据buff
 * @retval 数据包类�?( 0xffffffff 是错误的返回�?)
 */
uint32_t Check_RD03_DataMode(USART_DataTypeDef* rx_data_buff)
{
    uint8_t debug_mode_data_head_temp[] = {0xAA,0xBF,0x10,0x14,0x00};
    uint8_t report_mode_data_head_temp[] = {0xF4,0xF3,0xF2,0xF1,0x00};
    char run_mode_data_head_temp0[] = "ON";
    char run_mode_data_head_temp1[] = "OFF";
    char* p_check_rd03_data_mode_temp;
    p_check_rd03_data_mode_temp = (char*)rx_data_buff->data;
    uint32_t return_value = 0xffffffff;
    
    if(0x00 == strstr((char*)rx_data_buff->data,(char*)run_mode_data_head_temp0)- p_check_rd03_data_mode_temp)
    {
        return_value = RD03_RUNNING_MODE;
    }
    else if(0x00 == strstr((char*)rx_data_buff->data,(char*)run_mode_data_head_temp1)- p_check_rd03_data_mode_temp)
    {
        return_value = RD03_RUNNING_MODE;
    }
    else if(0x00 == strstr((char*)rx_data_buff->data,(char*)report_mode_data_head_temp)- p_check_rd03_data_mode_temp)
    {
        return_value = RD03_REPORTING_MODE;
    }
    else if(0x00 == strstr((char*)rx_data_buff->data,(char*)debug_mode_data_head_temp)- p_check_rd03_data_mode_temp)
    {
        return_value = RD03_DEBUGGING_MODE;
    }

    return return_value;
}

/**
 * @brief  调试模式数据
 * @param  rx_data_buff：接收数据buff
 * @retval  1：检测有�? 0：检测无�? 其它:不合法数�?
 */
uint8_t Rd03_Debug_ModeData(USART_DataTypeDef* rx_data_buff)
{
    char * p_rd03_debug_mode_temp;
    p_rd03_debug_mode_temp = (char*)rx_data_buff->data;
    {
        printf("\r\nHead: ");
        {
            memcpy((uint8_t*)rd03data.head,rx_data_buff->data,4);//获取包头
            for (int i = 0; i < 4; ++i) 
            {
                printf("0x%02x ", rd03data.head[i]);
            }
        }
        
        printf("\r\nData: ");
        {
            uint8_t data_temp[20*16*4] = {0};
            memcpy((uint8_t*)data_temp,(uint8_t*)(p_rd03_debug_mode_temp+4),20*16*4);//获取数据
            for (int i = 0; i < 20*16; i++) 
            {
                printf("0x%08x ",(data_temp[4*i])|(data_temp[4*i+1]<<8)|(data_temp[4*i+2]<<16)|(data_temp[4*i+3]<<24));
            }
        }
        printf("\r\nTail: ");
        {
            memcpy((uint8_t*)rd03data.tail,(uint8_t*)(p_rd03_debug_mode_temp+20*16*4+4),4);//获取�?
            for (int i = 0; i < 4; ++i) 
            {
                printf("0x%02x ", rd03data.tail[i]);
            }
        }

        printf("\n");
    }

    return 2;//用于填充数据，无实际作用
}

/**
 * @brief  上报模式数据
 * @param  rx_data_buff：接收数据buff
 * @retval  1：检测有�? 0：检测无�? 其它:不合法数�?
 */
uint8_t Rd03_Report_ModeData(USART_DataTypeDef* rx_data_buff)
{
    char * p_rd03_report_mode_temp;
    p_rd03_report_mode_temp = (char*)rx_data_buff->data; 
    rd03data.body_state = 2;
    {
        printf("\r\nHead: ");
        {
            memcpy((uint8_t*)rd03data.head,rx_data_buff->data,4);//获取包头
            for (int i = 0; i < 4; ++i) 
            {
                printf("0x%02x ", rd03data.head[i]);
            }
        }
        
        printf("\r\nData size: ");
        {
            rd03data.data_size = *(p_rd03_report_mode_temp+4)|((*(p_rd03_report_mode_temp+5))<<8);//获取包长�?
            printf("0x%04x ",rd03data.data_size);
        }
        
        printf("\r\nBody state: ");
        {
            rd03data.body_state = *(p_rd03_report_mode_temp+6);//获取人体�?测结�?
            if(rd03data.body_state)
            {
                printf("有人");
            }
            else
            {
                printf("无人");
            
            }
        }
        
        printf("\r\nBody range: ");
        {
            rd03data.body_range = *(p_rd03_report_mode_temp+7)|((*(p_rd03_report_mode_temp+8))<<8);//获取目标距离
            printf("0x%04x(%d cm) ",rd03data.body_range,rd03data.body_range);
        }
        
        printf("\r\nRange_gate_energy: ");
        {
            memcpy((uint8_t*)rd03data.range_gate_energy,(uint8_t*)(p_rd03_report_mode_temp+9),32);//获取各距离门能量
            for (int i = 0; i < 16; ++i) 
            {
                printf("0x%04x ",rd03data.range_gate_energy[2*i]|(rd03data.range_gate_energy[2*i+1]<<8));
            }
        }
        
        printf("\r\nTail: ");
        {
            memcpy((uint8_t*)rd03data.tail,(uint8_t*)(p_rd03_report_mode_temp+41),4);//获取�?
            for (int i = 0; i < 4; i++) 
            {
                printf("0x%02x ", rd03data.tail[i]);
            }
        }
    }
    return rd03data.body_state;
}

/**
 * @brief  运行模式数据
 * @param  rx_data_buff：接收数据buff
 * @retval  1：检测有�? 0：检测无�? 其它:不合法数�?
 */
uint8_t Rd03_Run_ModeData(USART_DataTypeDef* rx_data_buff)
{
    if(0x00 == (memcmp(rx_data_buff->data,"ON",strlen("ON"))))//字符串比�?,雷达�?测到有人
    {
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)"ON ");
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)rx_data_buff->data+strlen("ON")+2);
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)"有人--距离(:cm) 0x");
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)rx_data_buff->data+strlen("ON Range")+2);
        return 1;
    }
    else if(0x00 == (memcmp(rx_data_buff->data,"OFF",strlen("OFF"))))//字符串比�?,雷达�?测到无人
    {
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)"OFF ");
        USARTX_SendString(DEBUG_USARTX,(uint8_t*)"无人 ");
        return 0 ;
    }
    
    return 2;//用于填充数据，无实际作用
}

/*****************************END OF FILE***************************************/

