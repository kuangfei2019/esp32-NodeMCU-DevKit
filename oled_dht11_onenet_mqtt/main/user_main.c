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

#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"


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

#define MQTT_HOST            "mqtt.heclouds.com"         // MQTT服务端域名/IP地址
#define MQTT_PORT           6002        // 网络连接端口号
#define MQTT_CLIENT_ID       "636325056"    // 官方例程中是"Device_ID"        // 客户端标识符
#define MQTT_USER            "375197"             // MQTT用户名
#define MQTT_PASS            "kf12345678000xy01"     // MQTT密码

//#define STA_SSID             "Spring"        // WIFI名称
//#define STA_PASS             "SRAMx7x9"     // WIFI密码

static const char *TAG = "MQTT_EXAMPLE";

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
	esp_mqtt_client_handle_t client = event->client;
	int msg_id;
	// your_context_t *context = event->context;
	switch (event->event_id) {
		case MQTT_EVENT_CONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

			msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

			msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
			ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

			msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
			ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
			break;
		case MQTT_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			break;

		case MQTT_EVENT_SUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
			ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			break;
		case MQTT_EVENT_UNSUBSCRIBED:
			ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_PUBLISHED:
			ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			break;
		case MQTT_EVENT_DATA:
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
			printf("DATA=%.*s\r\n", event->data_len, event->data);
			break;
		case MQTT_EVENT_ERROR:
			ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			break;
		default:
			ESP_LOGI(TAG, "Other event id:%d", event->event_id);
			break;
	}
	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
	esp_mqtt_client_config_t mqtt_cfg = {
		.event_handle = mqtt_event_handler,
		//.host = "139.196.135.135",
		.host = MQTT_HOST,
		.port = MQTT_PORT,
		.client_id = MQTT_CLIENT_ID,
		.username = MQTT_USER,
		.password = MQTT_PASS,
		.buffer_size = 4096
	};
#if CONFIG_BROKER_URL_FROM_STDIN
	char line[128];

	if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
		int count = 0;
		printf("Please enter url of mqtt broker\n");
		while (count < 128) {
			int c = fgetc(stdin);
			if (c == '\n') {
				line[count] = '\0';
				break;
			} else if (c > 0 && c < 127) {
				line[count] = c;
				++count;
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		mqtt_cfg.uri = line;
		printf("Broker url: %s\n", line);
	} else {
		ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
		abort();
	}
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	esp_mqtt_client_start(client);
}


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
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	mqtt_app_start();
	console_task_example(NULL);
}

