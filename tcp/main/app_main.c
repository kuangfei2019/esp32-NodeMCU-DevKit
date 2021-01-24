/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include <freertos/event_groups.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

#define GPIO_OUTPUT_IO_LED    2
#define GPIO_OUTPUT_LED_PIN_SEL  (1ULL << GPIO_OUTPUT_IO_LED)
#define ProjectName "OLED_DHT11"

#define GPIO_OUTPUT_IO_0    23
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_OUTPUT_IO_0)
#define GPIO_INPUT_IO_0     23
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)
#define ESP_INTR_FLAG_DEFAULT 0


#define MQTT_HOST            "mqtt.heclouds.com"         // MQTT服务端域名/IP地>址
#define MQTT_PORT           6002        // 网络连接端口号
#define MQTT_CLIENT_ID       "636325056"    // 官方例程中是"Device_ID"        // 客户端标识符
#define MQTT_USER            "375197"             // MQTT用户名
#define MQTT_PASS            "kf12345678000xy01"     // MQTT密码
#define TOPIC_POST           "$dp"
#define POST_DATA "{\"temp\":%d.%d,\"hum\":%d, \"light\":%d}"

const int MQTT_CONNECTED_BIT = BIT1;
static const char *TAG = "MQTT_EXAMPLE";
uint32_t light_val;
char mqtt_buf[1024] = { '\0' };
static EventGroupHandle_t data_event_group;
esp_mqtt_client_handle_t client; 

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
	    xEventGroupSetBits(data_event_group, MQTT_CONNECTED_BIT);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
	    xEventGroupClearBits(data_event_group, MQTT_CONNECTED_BIT);
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
//		.uri = CONFIG_BROKER_URL,
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

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void mqtt_Task(void *pvParameters)
{
	uint16_t data_len = 0;

	while(1)
	{
		xEventGroupWaitBits(data_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
		memset(mqtt_buf, 0, strlen(mqtt_buf));
		mqtt_buf[0] = 0x03;
		sprintf(mqtt_buf + 3, POST_DATA, DHT11_Data_Array[2], DHT11_Data_Array[3], DHT11_Data_Array[0], light_val);
		data_len = strlen(mqtt_buf + 3);
		mqtt_buf[1] = (data_len >> 8) & 0xff;
		mqtt_buf[2] = data_len & 0xff;
		printf("mqtt_buf = %s\r\n", mqtt_buf + 3);
		data_len += 3;
		esp_mqtt_client_publish(client, (char *)TOPIC_POST, mqtt_buf, data_len, 1, 0);
                vTaskDelay(60000 / portTICK_RATE_MS);
	}
}

static void gpio_task_example(void* arg)
{
        int cnt = 0;
        while(1) {
                //printf("cnt: %d\n", cnt++);
         //       cnt++;
                vTaskDelay(1000 / portTICK_RATE_MS);
                gpio_set_level(GPIO_OUTPUT_IO_LED, cnt % 2);
                //              printf("GPIO test\n");
        }
        vTaskDelete(NULL);
}

static void system_timer_callback(void* arg);
esp_timer_handle_t system_timer;

/****************************************************** */
static void system_timer_callback(void* arg)
{
        char light_buf[10] = { '\0' };

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
                        DHT11_NUM_Char();       // DHT11数据值转成字符串

                        OLED_ShowString(50,2,DHT11_Data_Char[0]);        // DHT11_Data_Char[0] == 【湿度字符串】
                        OLED_ShowString(50,4,DHT11_Data_Char[1]);        // DHT11_Data_Char[1] == 【温度字符串】
                }

                light_val = read_adc_val();
                //      printf("\r\n光照 = %d\r\n", light_val);
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


void app_main(void)
{

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Project: \t%s", ProjectName);
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	data_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	mqtt_app_start();
	mygpio_init();

	//start gpio task
	xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
	// OLED显示初始化
	//--------------------------------------------------------
	OLED_Init();                                                    // OLED>初始化
	OLED_ShowString(0, 0, (uint8_t *)"ENV Monitor");                // 湿度
	OLED_ShowString(0, 2, (uint8_t *)"Hum:");               // 湿度
	OLED_ShowString(0, 4, (uint8_t *)"Temp:");      // 温度
	OLED_ShowString(0, 6, (uint8_t *)"Light:");     // 温度
	//   system_timer_init();
	light_adc_init();
	xTaskCreate(system_timer_callback, "system_task_example", 4096, NULL, 10, NULL);
	xTaskCreate(mqtt_Task, "mqtt_task_example", 2048, NULL, 10, NULL);
}
