/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "oled.h"
#include "dht11.h"
#include "light.h"

/**
 * Brief:
 * This test code shows how to configure gpio.
 *
 * GPIO status:
 * GPIO2: output
 *
 * Test:
 * Make led blink
 *
 */

#define GPIO_OUTPUT_IO_LED    2
#define GPIO_OUTPUT_LED_PIN_SEL  (1ULL << GPIO_OUTPUT_IO_LED)
#define ProjectName "OLED_DHT11"

#define GPIO_OUTPUT_IO_0    23
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)
#define GPIO_INPUT_IO_0     23
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)
#define ESP_INTR_FLAG_DEFAULT 0

static void gpio_task_example(void* arg)
{
	int cnt = 0;
	while(1) {
		printf("cnt: %d\n", cnt++);
		vTaskDelay(1000 / portTICK_RATE_MS);
		gpio_set_level(GPIO_OUTPUT_IO_LED, cnt % 2);
//		printf("GPIO test\n");
	}
}

static void system_timer_callback(void* arg);
esp_timer_handle_t system_timer;

/****************************************************** */
static void system_timer_callback(void* arg)
{
	char light_buf[10] = { '\0' };
	uint32_t light_val;

	if(DHT11_Read_Data_Complete() == 0)             // 读取DHT11温湿度值
        {
                //-------------------------------------------------
                // DHT11_Data_Array[0] == 湿度_整数_部分
                // DHT11_Data_Array[1] == 湿度_小数_部分
                // DHT11_Data_Array[2] == 温度_整数_部分
                // DHT11_Data_Array[3] == 温度_小数_部分
                // DHT11_Data_Array[4] == 校验字节
                // DHT11_Data_Array[5] == 【1:温度>=0】【0:温度<0】
                //-------------------------------------------------


                // 温度超过30℃，LED亮
                //----------------------------------------------------
                if(DHT11_Data_Array[5]==1 && DHT11_Data_Array[2]>=30)
                        gpio_set_level(GPIO_OUTPUT_IO_0, 0);
                else
                        gpio_set_level(GPIO_OUTPUT_IO_0, 1);


                // 串口输出温湿度
                //---------------------------------------------------------------------------------
                if(DHT11_Data_Array[5] == 1)                    // 温度 >= 0℃
                {
                        printf("\r\n湿度 = %d.%d %%RH\r\n",DHT11_Data_Array[0],DHT11_Data_Array[1]);
                        printf("\r\n温度 = %d.%d ℃\r\n", DHT11_Data_Array[2],DHT11_Data_Array[3]);
                }
                else // if(DHT11_Data_Array[5] == 0)    // 温度 < 0℃
                {
                        printf("\r\n湿度 = %d.%d %%RH\r\n",DHT11_Data_Array[0],DHT11_Data_Array[1]);
                        printf("\r\n温度 = -%d.%d ℃\r\n",DHT11_Data_Array[2],DHT11_Data_Array[3]);
                }

                // OLED显示温湿度
                //---------------------------------------------------------------------------------
                DHT11_NUM_Char();       // DHT11数据值转成字符串

                OLED_ShowString(50,2,DHT11_Data_Char[0]);        // DHT11_Data_Char[0] == 【湿度字符串】
                OLED_ShowString(50,4,DHT11_Data_Char[1]);        // DHT11_Data_Char[1] == 【温度字符串】
        }

	light_val = read_adc_val();
	printf("\r\n光照 = %d\r\n", light_val);
	sprintf(light_buf, "%d     ", light_val);
	OLED_ShowString(50, 6, (uint8_t *)light_buf);
}

void system_timer_start(void)
{
 // 开启一个周期性定时器-----------------------------------------------------/
  ESP_ERROR_CHECK(esp_timer_start_periodic(system_timer, 1000000)); // 1s  -----单位是us
}

void system_timer_init(void)
{

    const esp_timer_create_args_t system_timer_args =
    {
        .callback = &system_timer_callback,
        .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&system_timer_args, &system_timer));
    system_timer_start();

}

void app_main(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_LED_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
    printf("\r\n=================================================\r\n");
    printf("\t Project:\t%s\r\n", ProjectName);
    printf("\t SDK version:\t%s", esp_get_idf_version());
    printf("\r\n=================================================\r\n");

    // OLED显示初始化
    //--------------------------------------------------------
    OLED_Init();							// OLED初始化
    OLED_ShowString(0, 0, (uint8_t *)"ENV Monitor");		// 湿度
    OLED_ShowString(0, 2, (uint8_t *)"Hum:");		// 湿度
    OLED_ShowString(0, 4, (uint8_t *)"Temp:");	// 温度
    OLED_ShowString(0, 6, (uint8_t *)"Light:");	// 温度
    system_timer_init();
    light_adc_init();

}

