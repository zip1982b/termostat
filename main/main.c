/*
Project name: Termostat
Autor: zip1982b

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h> //*
#include <stddef.h> //*
#include <string.h> //*


#include <stdlib.h>
#include <esp_types.h>

#include "esp_wifi.h" //*
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"
#include "freertos/queue.h"



#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "ssd1306.h"
#include "ds2482.h"


/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;


/*
 * GPIO status:
 *	GPIO12: output (relay2)
*/
#define GPIO_RELAY1    12
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_RELAY1) //| (1ULL<<GPIO_RELAY2))


#define ESP_INTR_FLAG_DEFAULT 0


static const char *TAG = "MQTT_EXAMPLE";


portBASE_TYPE xStatusOLED;
portBASE_TYPE xStatusReadTemp; // status task create

xTaskHandle xDisplay_Handle;
xTaskHandle xRead_Temp_Handle; // identification Read Temp task

static xQueueHandle data_to_display_queue = NULL;
static xQueueHandle dataFromDisplay_queue = NULL;
static xQueueHandle status_relay_queue = NULL;


typedef struct{
	float temperatura;
} data_to_display_t;





extern uint8_t short_detected; //short detected on 1-wire net
extern uint8_t crc8;
extern uint8_t crc_tbl[];
extern uint8_t ROM_NO[8];


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
	     .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}



/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
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
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
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
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}







void vDisplay(void *pvParameter)
{
	i2c_master_init(I2C_MASTER_NUM_SSD1306, I2C_MASTER_SDA_SSD1306, I2C_MASTER_SCL_SSD1306, I2C_MASTER_FREQ_HZ_SSD1306, I2C_MASTER_RX_BUF_SSD1306, I2C_MASTER_TX_BUF_SSD1306);
	
	vTaskDelay(1000 / portTICK_RATE_MS);

	portBASE_TYPE xStatusReceive;
	portBASE_TYPE xdata_to_display_Receive;
	data_to_display_t data_to_display;
    
	uint8_t status_relay = 0;

	uint8_t change = 1;
	uint8_t change_status_relay = 1;
	
	SSD1306_Init();
    while(1) {
		/***** data from vTemperatureSensor***********************/
		xdata_to_display_Receive = xQueueReceive(data_to_display_queue, &data_to_display, 100/portTICK_RATE_MS); // portMAX_DELAY - very long time
		if(xdata_to_display_Receive == pdPASS)
		{
			change = 1;
		}
		
        /******data from vPump*****************/
		xStatusReceive = xQueueReceive(status_relay_queue, &status_relay, 100/portTICK_RATE_MS); // portMAX_DELAY - very long time
        if(xStatusReceive == pdPASS){
            change_status_relay = 1;
        }

		if(change || change_status_relay)
		{
			
			vDrawMenu(status_relay, data_to_display.temperatura);
			change = change_status_relay = 0;
		}
    }
	vTaskDelete(NULL);
}





static void vTemperatureSensor(void* arg)
{
    data_to_display_t data_to_display;
	i2c_master_init(I2C_MASTER_NUM_DS2482, I2C_MASTER_SDA_DS2482, I2C_MASTER_SCL_DS2482, I2C_MASTER_FREQ_HZ_DS2482, I2C_MASTER_RX_BUF_DS2482, I2C_MASTER_TX_BUF_DS2482);
	uint8_t checksum = 0;
	
	uint8_t get[9]; //get scratch pad
	int temp;
	float temperatura = 0;
	vTaskDelay(50 / portTICK_RATE_MS);
	

	/* Find Devices */
	if(DS2482_detect())
	{
        printf("ds2482 is detected\n");
		/*ds2482 i2c/1-wire bridge detected*/
		if(OWReset() && !short_detected)
		{
			/*1-wire device detected*/
			// find address devices
			printf("\n1-wire divice is detecting - Finding address ..... \n");
            if(OWFirst()){
                printf("1-wire device is find!!! \n");
                for(uint8_t i=0; i<=7; i++){
                    printf("%X", ROM_NO[i]);
                }
                printf("\n");
                vTaskDelay(100 / portTICK_RATE_MS);

			    if(OWReset() && !short_detected){
                    OWWriteByte(SkipROM); //0xCC
                    OWWriteByte(WriteScratchpad); //0x4E
				    OWWriteByte(0x4B); //TH
				    OWWriteByte(0x46); //TL
				    OWWriteByte(0x7F); //Config register
                    if(OWReset() && !short_detected){
                        OWWriteByte(SkipROM); //0xCC
                        OWWriteByte(CopyScratchpad); //0x48
                    } else printf("[4]1-wire device is not detected or short_detected = %d\n", short_detected);
                } else printf("[3]1-wire device in not detected or short_detected = %d\n", short_detected);
            } else printf("[2]1-wire device is not find");
		} else printf("[1]1-wire device is not detected or short_detected = %d\n", short_detected);
	} else printf("ds2482 id not detected\n");
	
	while(1)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
		printf("*****Cycle********\n");
		if(OWReset() && !short_detected)
		{
			
			OWWriteByte(SkipROM); //0xCC - пропуск проверки адресов
			//printf("SkipROM\n");
			OWWriteByte(ConvertT); //0x44 - все датчики измеряют свою температуру
			//printf("ConvertT - delay 1 sec for conversion\n");
			vTaskDelay(1000 / portTICK_RATE_MS);
			
			crc8 = 0;
			if(OWReset() && !short_detected){
				vTaskDelay(50 / portTICK_RATE_MS);
				OWWriteByte(MatchROM); //0x55 - соответствие адреса
				//printf("send MatchROM command\n");
				// send ROM address = 64 bit
				for(uint8_t k = 0; k <= 7; k++){
					//printf(" %X ", *(pROM_NO[l] + k));
					OWWriteByte(ROM_NO[k]);
				}
				OWWriteByte(ReadScratchpad); //0xBE
				//printf("\n send ReadScratchpad command \n");
				for (uint8_t n=0; n<9; n++)
				{
					get[n] = OWReadByte();
					//printf("get[%d] = %X\n", n, get[n]);
								
					//get[8] не надо проверять crc
					if(n < 8){
						calc_crc8(get[n]); // accumulate the CRC
						//printf("crc8 = %X\n", crc8);
					}
					else if(get[8] == crc8){
						printf("crc = OK\n");
						checksum = 1;
					}
					else{
						checksum = 0;
						printf("crc = NOK\n");
					}
				}
				//printf("ScratchPAD data = %X %X %X %X %X %X %X %X %X\n", get[8], get[7], get[6], get[5], get[4], get[3], get[2], get[1], get[0]);
				
				// расчёт температуры
				if(checksum){
						/* - */
					if(getbits(get[1], 7, 1))
					{
						temp = get[1] << 8 | get[0];
						temp = (~temp) + 1;
						temperatura = (temp * 0.0625) * (-1);
						printf("temp = %f *C\n", temperatura);
					}
						/* + */
					else 
					{
						temp = get[1] << 8 | get[0];
						temperatura = temp * 0.0625;
						printf("temp = %f *C\n", temperatura);
					}	
				}
			}
			
			data_to_display.temperatura = temperatura;
			xQueueSendToBack(data_to_display_queue, &data_to_display, 100/portTICK_RATE_MS);
		
        }
	}
	vTaskDelete(NULL);
}




static void vPump(void* arg){
	uint8_t status_relay = 0;
    while(1){
				gpio_set_level(GPIO_RELAY1, 1);
                status_relay = 1;
			    xQueueSendToBack(status_relay_queue, &status_relay, 100/portTICK_RATE_MS);
		        vTaskDelay(5000 / portTICK_RATE_MS);
				gpio_set_level(GPIO_RELAY1, 0);
                status_relay = 0;
			    xQueueSendToBack(status_relay_queue, &status_relay, 100/portTICK_RATE_MS);
		        vTaskDelay(5000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	

    /*** GPIO init ***/
	gpio_config_t io_conf;
	//Настройки GPIO для релейного ВЫХОДа
    //disable interrup_ccwt - отключитли прерывания
    io_conf.intr_type = GPIO_INTR_DISABLE;
	
    //set as output mode - установка в режим Выход
    io_conf.mode = GPIO_MODE_OUTPUT;
	
    //bit mask of the pins that you want to set,e.g.GPIO14/12
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	
    //disable pull-down_cw mode - отключитли подтяжку к земле
    io_conf.pull_down_en = 0;
	
    //disable pull-up_ccw mode - отключили подтяжку к питанию
    io_conf.pull_up_en = 0;
	
    //configure GPIO with the given settings - конфигурирование портов с данными настройками (Выход)
    gpio_config(&io_conf);
	
	
    gpio_config(&io_conf);
	

	data_to_display_queue = xQueueCreate(3, sizeof(data_to_display_t));
	status_relay_queue = xQueueCreate(3, sizeof(uint8_t));
	dataFromDisplay_queue = xQueueCreate(8, sizeof(uint8_t)); 
	
	
	
    xTaskCreate(vPump, "vPump", 1024 * 2, NULL, 11, NULL);	

	xStatusOLED = xTaskCreate(vDisplay, "vDisplay", 1024 * 2, NULL, 11, &xDisplay_Handle);
	if(xStatusOLED == pdPASS)
		printf("Task vDisplay is created!\n");
	else
		printf("Task vDisplay is not created\n");
	
	
	xStatusReadTemp = xTaskCreate(vTemperatureSensor, "vTemperatureSensor", 1024 * 2, NULL, 10, &xRead_Temp_Handle);
	if(xStatusReadTemp == pdPASS)
		printf("Task vTemperatureSensor is created!\n");
	else
		printf("Task vTemperatureSensor is not created\n");
	
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
//    ESP_ERROR_CHECK(example_connect());

    wifi_init_sta();
    mqtt_app_start();
}

