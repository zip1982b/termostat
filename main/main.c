/*
Project name: Termostat
Autor: zip1982b

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <esp_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_system.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"
#include "freertos/queue.h"


#include "ssd1306.h"
#include "ds2482.h"



#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>



#define SSID CONFIG_WIFI_SSID
#define PASS CONFIG_WIFI_PASSWORD

#define UDP_PORT CONFIG_WiFi_PORT

#define MULTICAST_LOOPBACK WiFi_LOOPBACK

#define MULTICAST_TTL CONFIG_WiFi_MULTICAST_TTL

#define MULTICAST_IPV4_ADDR CONFIG_WiFi_MULTICAST_IPV4_ADDR
#define MULTICAST_IPV6_ADDR CONFIG_WiFi_MULTICAST_IPV6_ADDR

#define LISTEN_DEFAULT_IF CONFIG_WiFi_MULTICAST_LISTEN_DEFAULT_IF

/* 	
	cr	- clockwise rotation
	ccr	- counter clockwise rotation
	bp	- button pressed
*/
enum action{cr, ccr, bp}; 

/*
 * GPIO status:
 *	GPIO14: output (relay1)
 *	GPIO12: output (relay2)
 *	GPIO33:(clk - encoder) input
 *	GPIO25:(dt - encoder) input
 *	GPIO26:(sw - encoder) input
 *
*/
#define GPIO_RELAY1    12
//#define GPIO_RELAY2    14
#define GPIO_OUTPUT_PIN_SEL  (1ULL<<GPIO_RELAY1) //| (1ULL<<GPIO_RELAY2))

#define GPIO_ENC_CLK     35
#define GPIO_ENC_DT		 32
#define GPIO_ENC_SW		 33
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_ENC_CLK) | (1ULL<<GPIO_ENC_DT) | (1ULL<<GPIO_ENC_SW)) 

#define ESP_INTR_FLAG_DEFAULT 0


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   we use two - one for IPv4 "got ip", and
   one for IPv6 "got ip". */
const int IPV4_GOTIP_BIT = BIT0;
const int IPV6_GOTIP_BIT = BIT1;

static const char *TAG = "multicast";
#ifdef CONFIG_WiFi_IPV4
static const char *V4TAG = "mcast-ipv4";
#endif
#ifdef CONFIG_WiFi_IPV6
static const char *V6TAG = "mcast-ipv6";
#endif












portBASE_TYPE xStatusOLED;
portBASE_TYPE xStatusReadTemp; // status task create

xTaskHandle xDisplay_Handle;
xTaskHandle xRead_Temp_Handle; // identification Read Temp task

static xQueueHandle gpio_evt_queue = NULL;
static xQueueHandle ENC_queue = NULL;
static xQueueHandle data_to_display_queue = NULL;
static xQueueHandle dataFromDisplay_queue = NULL;


typedef struct{
	uint8_t sensors;
	uint8_t status_relay;
	float temp_average;
} data_to_display_t;





extern uint8_t short_detected; //short detected on 1-wire net
extern uint8_t crc8;
extern uint8_t crc_tbl[];
extern uint8_t ROM_NO[8];


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IPV4_GOTIP_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, IPV4_GOTIP_BIT);
        xEventGroupClearBits(wifi_event_group, IPV6_GOTIP_BIT);
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        xEventGroupSetBits(wifi_event_group, IPV6_GOTIP_BIT);
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}







static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
	switch(gpio_num){
		case GPIO_ENC_CLK:
				gpio_set_intr_type(GPIO_ENC_CLK, GPIO_INTR_DISABLE);
				break;
		case GPIO_ENC_DT:
				gpio_set_intr_type(GPIO_ENC_DT, GPIO_INTR_DISABLE);
				break;
		case GPIO_ENC_SW:
				gpio_set_intr_type(GPIO_ENC_SW, GPIO_INTR_DISABLE);
				break;
	}
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, 0);
}



static void ENC(void* arg)
{
	
	enum action rotate;
	//portBASE_TYPE xStatus;
    uint32_t io_num;
    for(;;) {
		io_num = 0;
		xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY); // the task is blocked until the data arrives
		if(io_num == GPIO_ENC_CLK)
		{
			xQueueReceive(gpio_evt_queue, &io_num, 100/portTICK_RATE_MS);
			gpio_set_intr_type(GPIO_ENC_CLK, GPIO_INTR_ANYEDGE); //enable
			if (io_num == GPIO_ENC_DT)
			{
				rotate = cr;
				printf("[ENC]clockwise rotation\n");
				xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS); //xStatus = 
				gpio_set_intr_type(GPIO_ENC_DT, GPIO_INTR_ANYEDGE);//enable
				
			}
				
		}	
		else if(io_num == GPIO_ENC_DT)
		{
			//vTaskDelay(10 / portTICK_RATE_MS);
			xQueueReceive(gpio_evt_queue, &io_num, 100/portTICK_RATE_MS);
			gpio_set_intr_type(GPIO_ENC_DT, GPIO_INTR_ANYEDGE);
			
			if (io_num == GPIO_ENC_CLK)
			{
				rotate = ccr;
				//printf("[ENC]rotate = %d\n", rotate);
				printf("[ENC]counter clockwise rotation\n");
				xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);//xStatus = 
				gpio_set_intr_type(GPIO_ENC_CLK, GPIO_INTR_ANYEDGE);
			}
				
		}
		else if(io_num == GPIO_ENC_SW)
		{
			rotate = bp;
			//printf("[ENC]rotate = %d\n", rotate);
			printf("[ENC]Button is pressed\n");
			vTaskDelay(300 / portTICK_RATE_MS);
			xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);//xStatus = 
			gpio_set_intr_type(GPIO_ENC_SW, GPIO_INTR_NEGEDGE);//enable
			// 
		}
    }
}








void vDisplay(void *pvParameter)
{
	i2c_master_init(I2C_MASTER_NUM_SSD1306, I2C_MASTER_SDA_SSD1306, I2C_MASTER_SCL_SSD1306, I2C_MASTER_FREQ_HZ_SSD1306, I2C_MASTER_RX_BUF_SSD1306, I2C_MASTER_TX_BUF_SSD1306);
	
	vTaskDelay(1000 / portTICK_RATE_MS);
	esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

	portBASE_TYPE xStatusReceive;
	portBASE_TYPE xStatusSend;
	enum action rotate;
	data_to_display_t data_to_display;
	
	
	uint8_t state = 10; // default state
	uint8_t frame = 1;
	uint8_t menuitem = 1; // default item - Contrast
	
	
	uint8_t contrast = 100; // default contrast value
	uint8_t temp = 22; // default temp value
	uint8_t ust = 0;

	uint8_t change = 1;
	
	 
	SSD1306_Init();
	
	
	uint8_t down_cw;
	uint8_t up_ccw;
	uint8_t press_button;
	printf("state = 10\n");
	
	
	
	
    while(1) {
		/***** data ***********************/
		xStatusReceive = xQueueReceive(data_to_display_queue, &data_to_display, 100/portTICK_RATE_MS); // portMAX_DELAY - (very long time) сколь угодно долго - 100/portTICK_RATE_MS
		if(xStatusReceive == pdPASS)
		{

			change = 1;
			printf("[vDisplay]Receivied data / sensors = %d / status_relay = %d / temp_averag = %f *C\n", data_to_display.sensors, data_to_display.status_relay, data_to_display.temp_average);
		}
		
		/***********************/
		down_cw = 0;
		up_ccw = 0;
		press_button = 0;	


		if(change)
		{
			
			vDrawMenu(menuitem, state, temp, contrast, chip_info, data_to_display.sensors, data_to_display.status_relay, data_to_display.temp_average);
			change = 0;
		}

		
	/***** Read Encoder ***********************/
		xStatusReceive = xQueueReceive(ENC_queue, &rotate, 300/portTICK_RATE_MS); // portMAX_DELAY - (very long time) сколь угодно долго - 100/portTICK_RATE_MS
		if(xStatusReceive == pdPASS)
		{
			change = 1;
			printf("[vDisplay]rotate = %d\n", rotate);
			switch(rotate){
				case 0: //down_cw = clockwise
					down_cw = 1;
					break;
				case 1: //up_ccw = counter clockwise
					up_ccw = 1;
					break;
				case 2: //press_button = button pressed
					press_button = 1;
					break;
			}
		}
		//else
			//printf("[vDisplay]xStatusReceive = %d\n", xStatusReceive);
			//printf("[vDisplay]xStatusReceive not pdPass\n");
	/***** End Read Encoder ***********************/	
	

	
		if(temp != ust){
			ust = temp;
			xStatusSend = xQueueSendToBack(dataFromDisplay_queue, &ust, 100/portTICK_RATE_MS);
			if(xStatusSend == pdPASS)
			{
				printf("[vDisplay] send ustavka\n");
				printf("[vDisplay] temp = %d\n", temp);
				printf("[vDisplay] ustavka = %d\n", ust);
			}
		}
		
		
		
		switch(state){
			/*** Frame 1 - State 10 ************************************************/
			case 10:
				//printf("state = 10\n");
				frame = 1;
				if(down_cw) { menuitem++;}
				else if(up_ccw) { menuitem--; }
				
				// go to Sensors address
				else if(press_button && menuitem == 1) { 
					state = 1;
					printf("state = 1\n");
				}
				
				// go to Set temp
				else if(press_button && menuitem == 2) { 
					state = 2;
					printf("state = 2\n");
				}
				
				// go to Process view
				else if(press_button && menuitem == 3) { 
					state = 3;
					printf("state = 3\n");
				}
				
				// go to Chip info
				else if(press_button && menuitem == 4) { 
					state = 4;
					printf("state = 4\n");
				}
				
				
				if(menuitem == 0) { menuitem = 1; }
				
				if(menuitem > 4){
					state = 20;
					printf("state = 20\n");
					frame = 2;
				}

				break;
			/*********************************************************/
			
			/*** Frame 2 - State 20 *********************************************/
			case 20:
				//printf("State = 20\n");
				frame = 2;
				if(down_cw) { menuitem++; }
				else if(up_ccw) { menuitem--; }
				
				// go to Set temp
				else if(press_button && menuitem == 2) { 
					state = 2;
					printf("state = 2\n");
				}
				
				// go to Process view
				else if(press_button && menuitem == 3) { 
					state = 3;
					printf("state = 3\n");
				}
				
				// go to Chip info
				else if(press_button && menuitem == 4) { 
					state = 4;
					printf("state = 4\n");
				}
				
				// go to Network info
				else if(press_button && menuitem == 5) { 
					state = 5;
					printf("state = 5\n");
				}
				
				if(menuitem > 5){ 
					state = 30;
					printf("state = 30\n");
					frame = 3;
				}
				else if(menuitem < 2) { 
					state = 10;
					printf("state = 10\n");
					frame = 1;
				}
				break;
			/*********************************************************/
			
			/*** Frame 3 *********************************************/
			case 30:
				//printf("State = 30\n");
				frame = 3;
				if(up_ccw) { menuitem--; }
				else if(down_cw) { menuitem++; }
				
				// go to Process view
				else if(press_button && menuitem == 3) {
					state = 3;
					printf("state = 3\n");
				}
				
				// go to Chip info
				else if(press_button && menuitem == 4) {
					state = 4;
					printf("state = 4\n");
				}
				
				// go to Network info
				else if(press_button && menuitem == 5) {
					state = 5;
					printf("state = 5\n");
				}
				
				// go to Contrast
				else if(press_button && menuitem == 6) {
					state = 6;
					printf("state = 6\n");
				}
				
				if(menuitem < 3) { 
					state = 20;
					printf("state = 20\n");
					frame = 2;
				}
				else if(menuitem > 6) {
					state = 40;
					printf("state = 40\n");
					frame = 4;
				}
				break;
			/********************************************************/
			
			/*** Frame 4 *********************************************/
			case 40:
				//printf("State = 40\n");
				frame = 4;
				
				
				if(up_ccw) { menuitem--; }
				else if(down_cw) { menuitem++; }
				
				// go to Chip info
				else if(press_button && menuitem == 4) {
					state = 4;
					printf("state = 4\n");
				}
				
				// go to Network info
				else if(press_button && menuitem == 5) {
					state = 5;
					printf("state = 4\n");
				}
				
				// go to Contrast
				else if(press_button && menuitem == 6) {
					state = 6;
					printf("state = 6\n");
				}
				
				// go to Display sleep
				else if(press_button && menuitem == 7) {
					state = 7;
					printf("state = 7\n");
				}
				
				if(menuitem < 4) { 
					state = 30;
					printf("state = 30\n");
					frame = 3;
				}
				else if(menuitem > 7) {
					state = 50;
					printf("state = 50\n");
					frame = 5;
				}
				break;
			/********************************************************/
			
			/*** Frame 5 *********************************************/
			case 50:
				//printf("State = 50\n");
				frame = 5;
				if(up_ccw) { menuitem--; }
				else if(down_cw) { menuitem++; }
				else if(menuitem == 9) { menuitem = 8; }
				// go to Network info
				else if(press_button && menuitem == 5) {
					state = 5;
					printf("state = 5\n");
				}
				
				// go to Contrast
				else if(press_button && menuitem == 6) {
					state = 6;
					printf("state = 6\n");					
				}
				
				// go to Display sleep
				else if(press_button && menuitem == 7) {
					state = 7;
					printf("state = 7\n");
				}
				
				// go to Reset CPU
				else if(press_button && menuitem == 8) {
					state = 8;
					printf("State = 8\n");
				}
				
				if(menuitem < 5) { 
					state = 40;
					printf("state = 40\n");
					frame = 4;
				}
				
				break;
			/********************************************************/
			
			/*** Sensors address ***/
			case 1:
				if(press_button && frame == 1) {
					state = 10;
					printf("state = 10\n");
				}
				break;
				
			/*** Set temp ***/
			case 2:
				//printf("State = 2\n");
				if(down_cw){ 
					temp++;
					if(temp >= 30) { temp = 30; }
				}
				else if(up_ccw){ 
					temp--;
					if(temp <= 20) { temp = 20; }
				}
				else if(press_button && frame == 1) {
					state = 10;							// go to Frame 1
					printf("state = 10\n");
				} 
				else if(press_button && frame == 2) {
					state = 20;							// go to Frame 2
					printf("state = 20\n");
				} 
				break;
			
			/*** Process view ***/
			case 3:
				//printf("State = 3\n");
				if(press_button && frame == 1){ state = 10; }
				else if(press_button && frame == 2){
					state = 20;
					printf("state = 20\n");
				}
				else if(press_button && frame == 3){
					state = 30;
					printf("state = 30\n");
				}
				break;
			
			/*** Chip info ***/
			case 4:
				//printf("State = 4\n");
				if(press_button && frame == 1){
					state = 10;
					printf("state = 10\n");
				}
				else if(press_button && frame == 2){
					state = 20;
					printf("state = 20\n");
				}
				else if(press_button && frame == 3){
					state = 30;
					printf("state = 30\n");
				}
				else if(press_button && frame == 4){
					state = 40;
					printf("state = 40\n");
				}
				break;
			
			/*** Network info ***/
			case 5:
				//printf("State = 5\n");
				if(press_button && frame == 2){
					state = 20;
					printf("state = 20\n");
				}
				else if(press_button && frame == 3){
					state = 30;
					printf("state = 30\n");
				}
				else if(press_button && frame == 4){
					state = 40;
					printf("state = 40\n");
				}
				else if(press_button && frame == 5){
					state = 50;
					printf("state = 50\n");
				}
				break;
				
			/*** Contrast ***/
			case 6:
				//printf("State = 6\n");
				if(down_cw){
					contrast++;
					vSetContrast(contrast);
				}
				else if(up_ccw){
					contrast--;
					vSetContrast(contrast);
				}
				else if(press_button && frame == 3) {
					state = 30;
					printf("state = 30\n");
				}
				else if(press_button && frame == 4) {
					state = 40;
					printf("state = 40\n");
				}
				else if(press_button && frame == 5) {
					state = 50;
					printf("state = 50\n");
				}
				break;
				
			/*** Display sleep ***/
			case 7:
				//printf("State = 7\n");
				DisplayMode(OLED_CMD_DISPLAY_OFF);
				if(press_button && frame == 4) {
					state = 40;
					printf("state = 40\n");
					DisplayMode(OLED_CMD_DISPLAY_ON);
				}
				else if(press_button && frame == 5) { 
					state = 50;
					printf("state = 50\n");
					DisplayMode(OLED_CMD_DISPLAY_ON);
				}
				break;
			
			/*** Reset CPU ***/
			case 8:
				//printf("State = 8\n");
				printf("Restarting now.\n");
				fflush(stdout);
				esp_restart();
				break;
		}
    }
	vTaskDelete(NULL);
}





static void vRegulator(void* arg)
{
	i2c_master_init(I2C_MASTER_NUM_DS2482, I2C_MASTER_SDA_DS2482, I2C_MASTER_SCL_DS2482, I2C_MASTER_FREQ_HZ_DS2482, I2C_MASTER_RX_BUF_DS2482, I2C_MASTER_TX_BUF_DS2482);
	/* Find Devices */
	uint8_t rslt = 0;
	float average = 0;
	float sum = 0;
	uint8_t status_relay = 0;
	uint8_t checksum = 0;
	uint8_t *pROM_NO[3]; // 4 address = pROM_NO[0], pROM_NO[1], pROM_NO[2], pROM_NO[3].
	
	uint8_t i = 0;
	int j;
	int k;
	int n;
	int l;
	data_to_display_t data_to_display;
	
	uint8_t sensors = 0;
	uint8_t ust = 22;
	
	uint8_t get[9]; //get scratch pad
	int temp;
	float temperatura;
	uint8_t delta = 1;
	vTaskDelay(50 / portTICK_RATE_MS);
	

	if(DS2482_detect())
	{
		/*ds2482 i2c/1-wire bridge detected*/
		if(OWReset() && !short_detected)
		{
			/*1-wire device detected*/
			// find address ALL devices
			printf("\nFIND ALL ******** \n");
			rslt = OWFirst();
			while(rslt)
			{
				pROM_NO[i] = (uint8_t*) malloc(8); //memory for address
				sensors++;
				
				for(j = 7; j >= 0; j--)
				{
					*(pROM_NO[i] + j) = ROM_NO[j];
					printf("%02X", *(pROM_NO[i] + j));
				}

				

				printf("\nSensor# %d\n", i + 1);
				i++;
				rslt = OWNext();
				//printf("result OWNext() = %d\n", rslt);
			}
			
			printf("sensors = %d \n", sensors);
			printf("1-wire device end find ************ \n");
			
			
			
			
			vTaskDelay(100 / portTICK_RATE_MS);
			
			if(OWReset() && !short_detected)
			{
				OWWriteByte(SkipROM); //0xCC
				OWWriteByte(WriteScratchpad); //0x4E
				OWWriteByte(0x4B); //TH
				OWWriteByte(0x46); //TL
				OWWriteByte(0x7F); //Config register
				if(OWReset() && !short_detected)
				{
					OWWriteByte(SkipROM); //0xCC
					OWWriteByte(CopyScratchpad); //0x48
				}
				else
					printf("1-wire device not detected or short_detected = %d\n", short_detected);
				
			}
			else
				printf("1-wire device not detected or short_detected = %d\n", short_detected);

			
		}
		else
			printf("1-wire device not detected (1) or short_detected = %d\n", short_detected);
	}
	else
		printf("ds2482 i2c/1-wire bridge not detected\n");
	
	float *pSensor[sensors];
	for(uint8_t p = 0; p < sensors; p++){
		pSensor[p] = (float*) malloc(sizeof(float));
	}
	
	while(1)
	{
		vTaskDelay(1000 / portTICK_RATE_MS);
		xQueueReceive(dataFromDisplay_queue, &ust, 1000/portTICK_RATE_MS); // portMAX_DELAY - (very long time) сколь угодно долго - 100/portTICK_RATE_MS
		printf("**********************Cycle**********************************\n");
		if(OWReset() && !short_detected && sensors > 0)
		{
			
			OWWriteByte(SkipROM); //0xCC - пропуск проверки адресов
			printf("SkipROM\n");
			OWWriteByte(ConvertT); //0x44 - все датчики измеряют свою температуру
			printf("ConvertT - delay 1 sec for conversion\n");
			vTaskDelay(1000 / portTICK_RATE_MS);
			
			for(l = 0; l < sensors; l++)
			{
				crc8 = 0;
				if(OWReset() && !short_detected)
				{
					vTaskDelay(50 / portTICK_RATE_MS);
					OWWriteByte(MatchROM); //0x55 - соответствие адреса
					//printf("send MatchROM command\n");
					// send ROM address = 64 bit
					for(k = 0; k <= 7; k++)
					{
						//printf(" %X ", *(pROM_NO[l] + k));
						OWWriteByte(*(pROM_NO[l] + k));
					}
					OWWriteByte(ReadScratchpad); //0xBE
					printf("\n send ReadScratchpad command \n");
					for (n=0; n<9; n++)
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
					printf("ScratchPAD data = %X %X %X %X %X %X %X %X %X\n", get[8], get[7], get[6], get[5], get[4], get[3], get[2], get[1], get[0]);
					
					// расчёт температуры
					if(checksum){
						// -
						if(getbits(get[1], 7, 1))
						{
							temp = get[1] << 8 | get[0];
							temp = (~temp) + 1;
							temperatura = (temp * 0.0625) * (-1);
							//printf("Sensor# %d, temp = %f *C\n", l + 1, temperatura);
						}
						// +
						else 
						{
							temp = get[1] << 8 | get[0];
							temperatura = temp * 0.0625;
							//printf("Sensor# %d, temp = %f *C\n", l + 1, temperatura);
						}	
						*pSensor[l] = temperatura; 
					}
					else
						*pSensor[l] = 200.00;
					
				}
				else{
					gpio_set_level(GPIO_RELAY1, 0);
					status_relay = 0;
					printf("[vRegulator] Relay off - 1-wire device not detected(2) or short_detected = %d\n", short_detected);
				}
			}
			
			printf("[vRegulator] ustavka = %d*C\n", ust);
			
			
			for(uint8_t v = 0; v < sensors; v++){
				printf("[vRegulator] Sensor#%d = %.4f *C\n", v + 1, *pSensor[v]);
				sum += *pSensor[v];
			}	
			
			average = sum / sensors;
			sum = 0;
			printf("[vRegulator] average = %f *C\n", average);
				
				
			// controller temp
			if(average < ust - delta){
				gpio_set_level(GPIO_RELAY1, 1);
				status_relay = 1;
				printf("[vRegulator] Relay on\n");
			}
						
			if(average > ust + delta){
				gpio_set_level(GPIO_RELAY1, 0);
				status_relay = 0;
				printf("[vRegulator] Relay off\n");
			}
			
			data_to_display.sensors = sensors;
			data_to_display.status_relay =  status_relay;
			data_to_display.temp_average = average;
			xQueueSendToBack(data_to_display_queue, &data_to_display, 100/portTICK_RATE_MS);
			average = 0;
		}
		else
		{
			gpio_set_level(GPIO_RELAY1, 0);
			status_relay = 0;
			printf("[vRegulator] Relay off - 1-wire device not detected(1) or sensors = 0 or short_detected = %d\n", short_detected);
		}
	}
	vTaskDelete(NULL);
}



#ifdef CONFIG_WiFi_IPV4
/* Add a socket, either IPV4-only or IPV6 dual mode, to the IPV4
   multicast group */
static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    // Configure source interface
#if USE_DEFAULT_IF
    imreq.imr_interface.s_addr = IPADDR_ANY;
#else
    tcpip_adapter_ip_info_t ip_info = { 0 };
    err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(V4TAG, "Failed to get IP address info. Error 0x%x", err);
        goto err;
    }
    inet_addr_from_ipaddr(&iaddr, &ip_info.ip);
#endif
    // Configure multicast address to listen to
    err = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(V4TAG, "Configured IPV4 multicast address '%s' is invalid.", MULTICAST_IPV4_ADDR);
        goto err;
    }
    ESP_LOGI(TAG, "Configured IPV4 Multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(V4TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if) {
        // Assign the IPv4 multicast source interface, via its IP
        // (only necessary if this socket is IPV4 only)
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(V4TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
            goto err;
        }
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        goto err;
    }

 err:
    return err;
}
#endif /* CONFIG_WiFi_IPV4 */

#ifdef CONFIG_WiFi_IPV4_ONLY
static int create_multicast_ipv4_socket()
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(V4TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(UDP_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }


    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
        goto err;
    }

#if MULTICAST_LOOPBACK
    // select whether multicast traffic should be received by this device, too
    // (if setsockopt() is not called, the default is no)
    uint8_t loopback_val = MULTICAST_LOOPBACK;
    err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                     &loopback_val, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V4TAG, "Failed to set IP_MULTICAST_LOOP. Error %d", errno);
        goto err;
    }
#endif

    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}
#endif /* CONFIG_WiFi_IPV4_ONLY */

#ifdef CONFIG_WiFi_IPV6
static int create_multicast_ipv6_socket()
{
    struct sockaddr_in6 saddr = { 0 };
    struct in6_addr if_inaddr = { 0 };
    struct ip6_addr if_ipaddr = { 0 };
    struct ip6_mreq v6imreq = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
    if (sock < 0) {
        ESP_LOGE(V6TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(UDP_PORT);
    bzero(&saddr.sin6_addr.un, sizeof(saddr.sin6_addr.un));
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in6));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }

    // Selct the interface to use as multicast source for this socket.
#if USE_DEFAULT_IF
    bzero(&if_inaddr.un, sizeof(if_inaddr.un));
#else
    // Read interface adapter link-local address and use it
    // to bind the multicast IF to this socket.
    //
    // (Note the interface may have other non-LL IPV6 addresses as well,
    // but it doesn't matter in this context as the address is only
    // used to identify the interface.)
    err = tcpip_adapter_get_ip6_linklocal(TCPIP_ADAPTER_IF_STA, &if_ipaddr);
    inet6_addr_from_ip6addr(&if_inaddr, &if_ipaddr);
    if (err != ESP_OK) {
        ESP_LOGE(V6TAG, "Failed to get IPV6 LL address. Error 0x%x", err);
        goto err;
    }
#endif

    // Assign the multicast source interface, via its IP
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if_inaddr,
                     sizeof(struct in6_addr));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to set IPV6_MULTICAST_IF. Error %d", errno);
        goto err;
    }

    // Assign multicast TTL (set separately from normal interface TTL)
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to set IPV6_MULTICAST_HOPS. Error %d", errno);
        goto err;
    }

#if MULTICAST_LOOPBACK
    // select whether multicast traffic should be received by this device, too
    // (if setsockopt() is not called, the default is no)
    uint8_t loopback_val = MULTICAST_LOOPBACK;
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                     &loopback_val, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to set IPV6_MULTICAST_LOOP. Error %d", errno);
        goto err;
    }
#endif

    // this is also a listening socket, so add it to the multicast
    // group for listening...

    // Configure source interface
#if USE_DEFAULT_IF
    v6imreq.imr_interface.s_addr = IPADDR_ANY;
#else
    inet6_addr_from_ip6addr(&v6imreq.ipv6mr_interface, &if_ipaddr);
#endif
#ifdef CONFIG_WiFi_IPV6
    // Configure multicast address to listen to
    err = inet6_aton(MULTICAST_IPV6_ADDR, &v6imreq.ipv6mr_multiaddr);
    if (err != 1) {
        ESP_LOGE(V6TAG, "Configured IPV6 multicast address '%s' is invalid.", MULTICAST_IPV6_ADDR);
        goto err;
    }
    ESP_LOGI(TAG, "Configured IPV6 Multicast address %s", inet6_ntoa(v6imreq.ipv6mr_multiaddr));
    ip6_addr_t multi_addr;
    inet6_addr_to_ip6addr(&multi_addr, &v6imreq.ipv6mr_multiaddr);
    if (!ip6_addr_ismulticast(&multi_addr)) {
        ESP_LOGW(V6TAG, "Configured IPV6 multicast address '%s' is not a valid multicast address. This will probably not work.", MULTICAST_IPV6_ADDR);
    }

    err = setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                     &v6imreq, sizeof(struct ip6_mreq));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to set IPV6_ADD_MEMBERSHIP. Error %d", errno);
        goto err;
    }
#endif

#if CONFIG_WiFi_IPV4_V6
    // Add the common IPV4 config options
    err = socket_add_ipv4_multicast_group(sock, false);
    if (err < 0) {
        goto err;
    }
#endif

#if CONFIG_WiFi_IPV4_V6
    int only = 0;
#else
    int only = 1; /* IPV6-only socket */
#endif
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &only, sizeof(int));
    if (err < 0) {
        ESP_LOGE(V6TAG, "Failed to set IPV6_V6ONLY. Error %d", errno);
        goto err;
    }
    ESP_LOGI(TAG, "Socket set IPV6-only");

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}
#endif

static void mcast_example_task(void *pvParameters)
{
    while (1) {
        /* Wait for all the IPs we care about to be set
        */
        uint32_t bits = 0;
#ifdef CONFIG_WiFi_IPV4
        bits |= IPV4_GOTIP_BIT;
#endif
#ifdef CONFIG_WiFi_IPV6
        bits |= IPV6_GOTIP_BIT;
#endif
        ESP_LOGI(TAG, "Waiting for AP connection...");
        xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        int sock;

#ifdef CONFIG_WiFi_IPV4_ONLY
        sock = create_multicast_ipv4_socket(); ///////////////////////////////////////////////************************** create_multicast_ipv4_socket() ***************** ///////////////////////////////
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create IPv4 multicast socket");
        }
#else
        sock = create_multicast_ipv6_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create IPv6 multicast socket");
        }
#endif

        if (sock < 0) {
            // Nothing to do!
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

#ifdef CONFIG_WiFi_IPV4
        // set destination multicast addresses for sending from these sockets
        struct sockaddr_in sdestv4 = {
            .sin_family = PF_INET,
            .sin_port = htons(UDP_PORT),
        };
        // We know this inet_aton will pass because we did it above already
        inet_aton(MULTICAST_IPV4_ADDR, &sdestv4.sin_addr.s_addr);
#endif

#ifdef CONFIG_WiFi_IPV6
        struct sockaddr_in6 sdestv6 = {
            .sin6_family = PF_INET6,
            .sin6_port = htons(UDP_PORT),
        };
        // We know this inet_aton will pass because we did it above already
        inet6_aton(MULTICAST_IPV6_ADDR, &sdestv6.sin6_addr);
#endif

        // Loop waiting for UDP received, and sending UDP packets if we don't
        // see any.
        int err = 1;
        while (err > 0) {
            struct timeval tv = {
                .tv_sec = 2,
                .tv_usec = 0,
            };
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int s = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (s < 0) {
                ESP_LOGE(TAG, "Select failed: errno %d", errno);
                err = -1;
                break;
            }
            else if (s > 0) {
                if (FD_ISSET(sock, &rfds)) {
                    // Incoming datagram received
                    char recvbuf[48];
                    char raddr_name[32] = { 0 };

                    struct sockaddr_in6 raddr; // Large enough for both IPv4 or IPv6
                    socklen_t socklen = sizeof(raddr);
                    int len = recvfrom(sock, recvbuf, sizeof(recvbuf)-1, 0,
                                       (struct sockaddr *)&raddr, &socklen);
                    if (len < 0) {
                        ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
                        err = -1;
                        break;
                    }

                    // Get the sender's address as a string
#ifdef CONFIG_WiFi_IPV4
                    if (raddr.sin6_family == PF_INET) {
                        inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr.s_addr,
                                    raddr_name, sizeof(raddr_name)-1);
                    }
#endif
#ifdef CONFIG_WiFi_IPV6
                    if (raddr.sin6_family == PF_INET6) {
                        inet6_ntoa_r(raddr.sin6_addr, raddr_name, sizeof(raddr_name)-1);
                    }
#endif
                    ESP_LOGI(TAG, "received %d bytes from %s:", len, raddr_name);

                    recvbuf[len] = 0; // Null-terminate whatever we received and treat like a string...
                    ESP_LOGI(TAG, "%s", recvbuf);
                }
            }
            else { // s == 0
                // Timeout passed with no incoming data, so send something!
                static int send_count;
                const char sendfmt[] = "Multicast #%d sent by ESP32\n";
                char sendbuf[48];
                char addrbuf[32] = { 0 };
                int len = snprintf(sendbuf, sizeof(sendbuf), sendfmt, send_count++);
                if (len > sizeof(sendbuf)) {
                    ESP_LOGE(TAG, "Overflowed multicast sendfmt buffer!!");
                    send_count = 0;
                    err = -1;
                    break;
                }

                struct addrinfo hints = {
                    .ai_flags = AI_PASSIVE,
                    .ai_socktype = SOCK_DGRAM,
                };
                struct addrinfo *res;

#ifdef CONFIG_WiFi_IPV4 // Send an IPv4 multicast packet

#ifdef CONFIG_WiFi_IPV4_ONLY
                hints.ai_family = AF_INET; // For an IPv4 socket
#else
                hints.ai_family = AF_INET6; // For an IPv4 socket with V4 mapped addresses
                hints.ai_flags |= AI_V4MAPPED;
#endif
                int err = getaddrinfo(CONFIG_WiFi_MULTICAST_IPV4_ADDR,
                                      NULL,
                                      &hints,
                                      &res);
                if (err < 0) {
                    ESP_LOGE(TAG, "getaddrinfo() failed for IPV4 destination address. error: %d", err);
                    break;
                }
#ifdef CONFIG_WiFi_IPV4_ONLY
                ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(UDP_PORT);
                inet_ntoa_r(((struct sockaddr_in *)res->ai_addr)->sin_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "Sending to IPV4 multicast address %s:%d...",  addrbuf, UDP_PORT);
#else
                ((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons(UDP_PORT);
                inet6_ntoa_r(((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "Sending to IPV6 (V4 mapped) multicast address %s port %d (%s)...",  addrbuf, UDP_PORT, CONFIG_WiFi_MULTICAST_IPV4_ADDR);
#endif
                err = sendto(sock, sendbuf, len, 0, res->ai_addr, res->ai_addrlen);
                freeaddrinfo(res);
                if (err < 0) {
                    ESP_LOGE(TAG, "IPV4 sendto failed. errno: %d", errno);
                    break;
                }
#endif
#ifdef CONFIG_WiFi_IPV6
                hints.ai_family = AF_INET6;
                hints.ai_protocol = 0;
                err = getaddrinfo(CONFIG_EXAMPLE_MULTICAST_IPV6_ADDR,
                                  NULL,
                                  &hints,
                                  &res);
                if (err < 0) {
                    ESP_LOGE(TAG, "getaddrinfo() failed for IPV6 destination address. error: %d", err);
                    break;
                }

                struct sockaddr_in6 *s6addr = (struct sockaddr_in6 *)res->ai_addr;
                s6addr->sin6_port = htons(UDP_PORT);
                inet6_ntoa_r(s6addr->sin6_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "Sending to IPV6 multicast address %s port %d...",  addrbuf, UDP_PORT);
                err = sendto(sock, sendbuf, len, 0, res->ai_addr, res->ai_addrlen);
                freeaddrinfo(res);
                if (err < 0) {
                    ESP_LOGE(TAG, "IPV6 sendto failed. errno: %d", errno);
                    break;
                }
#endif
            }
        }

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

}



void app_main()
{
	ESP_ERROR_CHECK(nvs_flash_init());
    initialise_wifi();
    xTaskCreate(&mcast_example_task, "mcast_task", 4096, NULL, 5, NULL);
	
	
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
	
	//Настройки GPIO Энкодера на ВХОД
	//interrup_ccwt of falling edge - прерывание по спадающему фронту и по возрастающему фронту
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO33/25/26 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    
	//enable pull-up_ccw mode
    //io_conf.pull_up_ccw_en = 1;
	
    gpio_config(&io_conf);
	
	//change gpio interrup_ccwt type for one pin
    gpio_set_intr_type(GPIO_ENC_SW, GPIO_INTR_NEGEDGE); //Button pressed in gpio26
	
	
	//create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(7, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(ENC, "ENC", 1548, NULL, 12, NULL);
	
	ENC_queue = xQueueCreate(10, sizeof(uint32_t));

	data_to_display_queue = xQueueCreate(3, sizeof(data_to_display_t));
	
	dataFromDisplay_queue = xQueueCreate(8, sizeof(uint8_t)); // ustavka
	
	//install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_CLK, gpio_isr_handler, (void*) GPIO_ENC_CLK);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_DT, gpio_isr_handler, (void*) GPIO_ENC_DT);
	//hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_SW, gpio_isr_handler, (void*) GPIO_ENC_SW);
	
	
	
	
	
	xStatusOLED = xTaskCreate(vDisplay, "vDisplay", 1024 * 2, NULL, 11, &xDisplay_Handle);
	if(xStatusOLED == pdPASS)
		printf("Task vDisplay is created!\n");
	else
		printf("Task vDisplay is not created\n");
	
	
	xStatusReadTemp = xTaskCreate(vRegulator, "vRegulator", 1024 * 2, NULL, 10, &xRead_Temp_Handle);
	if(xStatusReadTemp == pdPASS)
		printf("Task vRegulator is created!\n");
	else
		printf("Task vRegulator is not created\n");
	
	
	
	
	
}

