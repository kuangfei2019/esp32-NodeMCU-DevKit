#ifndef __DHT11_H
#define __DHT11_H

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

// 全局变量
//=====================================================
extern uint8_t DHT11_Data_Array[6];		// DHT11数据数组
extern uint8_t DHT11_Data_Char[2][10];	// DHT11数据字符串
//=====================================================

// 函数声明
void Dht11_delay_ms(uint32_t C_time);			// 毫秒延时函数
void DHT11_Signal_Output(uint8_t Value_Vol);	// DHT11信号线(IO5)输出参数电平
void DHT11_Signal_Input(void);			// DHT11信号线(IO5) 设为输入
uint8_t DHT11_Start_Signal_JX(void);			// DHT11：输出起始信号－＞接收响应信号
uint8_t DHT11_Read_Bit(void);					// 读取DHT11一位数据
uint8_t DHT11_Read_Byte(void);				// 读取DHT11一个字节
uint8_t DHT11_Read_Data_Complete(void);		// 完整的读取DHT11数据操作
void DHT11_NUM_Char(void);				// DHT11数据值转成字符串

#endif /* __DHT11_H */

