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
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"

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
		//printf("cnt: %d\n", cnt++);
		cnt++;
		vTaskDelay(1000 / portTICK_RATE_MS);
		gpio_set_level(GPIO_OUTPUT_IO_LED, cnt % 2);
//		printf("GPIO test\n");
	}
	vTaskDelete(NULL);
}

static void system_timer_callback(void* arg);
esp_timer_handle_t system_timer;

/****************************************************** */
static void system_timer_callback(void* arg)
{
	char light_buf[10] = { '\0' };
	uint32_t light_val;

	while (1) {

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

#if 0
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
#endif

                // OLED显示温湿度
                //---------------------------------------------------------------------------------
                DHT11_NUM_Char();       // DHT11数据值转成字符串

                OLED_ShowString(50,2,DHT11_Data_Char[0]);        // DHT11_Data_Char[0] == 【湿度字符串】
                OLED_ShowString(50,4,DHT11_Data_Char[1]);        // DHT11_Data_Char[1] == 【温度字符串】
        }

	light_val = read_adc_val();
//	printf("\r\n光照 = %d\r\n", light_val);
	sprintf(light_buf, "%d     ", light_val);
	OLED_ShowString(50, 6, (uint8_t *)light_buf);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
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

#define PROMPT_STR CONFIG_IDF_TARGET

/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        return;
    }
}
#endif // CONFIG_STORE_HISTORY

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_console(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    /* Don't return empty lines */
    linenoiseAllowEmpty(false);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif
}

static void mygpio_init(void)
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
}

static void console_task_example(void* arg)
{
    initialize_nvs();

#if CONFIG_STORE_HISTORY
    initialize_filesystem();
#endif

    initialize_console();

    /* Register commands */
    esp_console_register_help_command();
    register_system();
    register_wifi();
    register_nvs();

    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    const char* prompt = LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;

    printf("\n"
           "This is an example of ESP-IDF console component.\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Press Enter or Ctrl+C will terminate the console environment.\n");

    /* Figure out if the terminal supports escape sequences */
    int probe_status = linenoiseProbe();
    if (probe_status) { /* zero indicates success */
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = PROMPT_STR "> ";
#endif //CONFIG_LOG_COLORS
    }
__start:
    /* Main loop */
    while(true) {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char* line = linenoise(prompt);
        if (line == NULL) { /* Break on EOF or error */
		goto __start;
            break;
        }
        /* Add the command to the history if not empty*/
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
#if CONFIG_STORE_HISTORY
            /* Save command history to filesystem */
            linenoiseHistorySave(HISTORY_PATH);
#endif
        }

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }

    esp_console_deinit();
//    vTaskDelete(NULL);

}

void app_main(void)
{
    mygpio_init();

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
   // xTaskCreate(console_task_example, "console_task_example", 2048, NULL, 10, NULL);
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
 //   system_timer_init();
    light_adc_init();
    xTaskCreate(system_timer_callback, "system_task_example", 4096, NULL, 10, NULL);
    console_task_example(NULL);
}

