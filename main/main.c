/*
Project name: Termostat
Autor: zip1982b

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <esp_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"
#include "freertos/queue.h"


#include "ssd1306.h"
#include "ds2482.h"


SemaphoreHandle_t i2c_mux = NULL; // access to i2c

/* 	
	cr	- clockwise rotation
	ccr	- counter clockwise rotation
	bp	- button pressed
*/
enum action{cr, ccr, bp}; 

portBASE_TYPE xStatusOLED;
portBASE_TYPE xStatusReadTemp; // status task create

xTaskHandle xDisplay_Handle;
xTaskHandle xRead_Temp_Handle; // identification Read Temp task

static xQueueHandle gpio_evt_queue = NULL;
static xQueueHandle ENC_queue = NULL;









extern uint8_t short_detected; //short detected on 1-wire net
extern uint8_t crc8;
extern uint8_t crc_tbl[];
extern uint8_t ROM_NO[8];







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
	portBASE_TYPE xStatus;
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
				xStatus = xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);
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
				xStatus = xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);
				gpio_set_intr_type(GPIO_ENC_CLK, GPIO_INTR_ANYEDGE);
			}
				
		}
		else if(io_num == GPIO_ENC_SW)
		{
			rotate = bp;
			//printf("[ENC]rotate = %d\n", rotate);
			printf("[ENC]Button is pressed\n");
			vTaskDelay(800 / portTICK_RATE_MS);
			xStatus = xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);
			gpio_set_intr_type(GPIO_ENC_SW, GPIO_INTR_NEGEDGE);//enable
			
		}
    }
}





void vDisplay(void *pvParameter)
{
	/* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("[vDisplay]This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("[vDisplay]silicon revision %d, \n", chip_info.revision);

    printf("[vDisplay]%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
	
	portBASE_TYPE xStatusReceive;
	enum action rotate;
	
	uint8_t state = 10; // default state
	uint8_t frame = 1;
	uint8_t menuitem = 1; // default item - Contrast
	
	int selectRelay = 0;
	uint8_t contrast = 100; // default contrast value
	uint8_t temp = 22; // default temp value

	uint8_t change = 1;
	
	xSemaphoreTake(i2c_mux, portMAX_DELAY); 
	SSD1306_Init();
	xSemaphoreGive(i2c_mux);
	
	uint8_t down_cw;
	uint8_t up_ccw;
	uint8_t press_button;
	printf("state = 10\n");
    while(1) {
		down_cw = 0;
		up_ccw = 0;
		press_button = 0;	
		if(change)
		{
			xSemaphoreTake(i2c_mux, portMAX_DELAY);
			vDrawMenu(menuitem, state, selectRelay, temp, contrast, chip_info);
			change = 0;
			xSemaphoreGive(i2c_mux);
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
		
		
		switch(state){
			/*** Frame 1 - State 10 ************************************************/
			case 10:
				//printf("state = 10\n");
				frame = 1;
				if(down_cw) { menuitem++;}
				else if(up_ccw) { menuitem--; }
				
				// go to Set relay
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
			
			/*** Set relay ***/
			case 1:
				if(down_cw){ 
					selectRelay++;
					if(selectRelay >= 2) { selectRelay = 0; }
				}
				else if(up_ccw){ 
					selectRelay--;
					if(selectRelay <= -1) { selectRelay = 1; }
				}
				else if(press_button && frame == 1) {
					state = 10;
					printf("state = 10\n");
				}
				break;
				
			/*** Set temp ***/
			case 2:
				//printf("State = 2\n");
				if(down_cw){ 
					temp++;
					if(temp >= 25) { temp = 25; }
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





static void vReadTemp(void* arg)
{
	/* Find Devices */
	uint8_t rslt = 0;
	static uint8_t LastDiscrepancy = 0;
	static uint8_t LastFamilyDiscrepancy = 0; 
	static uint8_t LastDeviceFlag = 0;
	
	uint8_t *pROM_NO[3]; // 4 address = pROM_NO[0], pROM_NO[1], pROM_NO[2], pROM_NO[3].
	
	uint8_t i = 0;
	int j;
	int k;
	int n;
	int l;
	
	uint8_t sensors = 0;
	
	uint8_t get[9]; //get scratch pad
	int temp;
	float temperatura;
	
	

	vTaskDelay(5000 / portTICK_RATE_MS);
	
	xSemaphoreTake(i2c_mux, portMAX_DELAY);
	if(DS2482_detect())
	{
		/*ds2482 i2c/1-wire bridge detected*/
		if(OWReset() && !short_detected)
		{
			/*1-wire device detected*/
			// find address ALL devices
			printf("\nFIND ALL ******** \n");
			do{
				//printf("i = %d\n", i);
				pROM_NO[i] = (uint8_t*) malloc(8); //memory for address
				rslt = OWSearch(&LastDiscrepancy, &LastFamilyDiscrepancy, &LastDeviceFlag); //pROM_NO[i]
				if(rslt)
				{
					sensors++;
					for(j = 7; j >= 0; j--)
					{
						*(pROM_NO[i] + j) = ROM_NO[j];
						printf("%02X", *(pROM_NO[i] + j));
					}
					printf("\nSensor# %d\n", i + 1);
				}
				else{
					printf("1-wire device end find\n");
					free(pROM_NO[i]);
					printf("sensors = %d\n", sensors);
				}
				i++;
				//vTaskDelay(250 / portTICK_RATE_MS);
			}
			while(i <= 3 && rslt); // maximum 4 address
			printf("1-wire device end find\n");
		}
		else
			printf("1-wire device not detected (1) or short_detected = %d\n", short_detected);
		
		if(OWReset() && !short_detected){
			vTaskDelay(100 / portTICK_RATE_MS);
			OWWriteByte(SkipROM); //0xCC
			printf("SkipROM\n");
			OWWriteByte(WriteScratchpad); //0x4E
			printf("WriteScratchpad\n");
			OWWriteByte(0x4B); //TH
			printf("TH = 0x4B\n");
			OWWriteByte(0x46); //TL
			printf("TL = 0x46\n");
			OWWriteByte(0x7F); //Config register
			printf("Config = 0x7F - 12 bit\n");
		}
		else
			printf("1-wire device not detected (2) or short_detected = %d\n", short_detected);
		
	}
	else
		printf("ds2482 i2c/1-wire bridge not detected\n");
	xSemaphoreGive(i2c_mux);
	
	while(1)
	{
		vTaskDelay(3000 / portTICK_RATE_MS);
		xSemaphoreTake(i2c_mux, portMAX_DELAY);
		printf("**********************Cycle**********************************\n");
		if(OWReset() && !short_detected && sensors > 0)
		{
			vTaskDelay(50 / portTICK_RATE_MS);
			OWWriteByte(SkipROM); //0xCC - пропуск проверки адресов
			printf("SkipROM\n");
			OWWriteByte(ConvertT); //0x44 - все датчики измеряют свою температуру
			printf("ConvertT\n");
			
			
			for(l = 0; l < sensors; l++)
			{
				crc8 = 0;
				if(OWReset() && !short_detected)
				{
					vTaskDelay(50 / portTICK_RATE_MS);
					OWWriteByte(MatchROM); //0x55 - соответствие адреса
					printf("send MatchROM command\n");
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
						if(n < 8)
						{
							calc_crc8(get[n]); // accumulate the CRC
							//printf("crc8 = %X\n", crc8);
						}
						else if(get[8] == crc8)
							printf("crc = OK\n");
						else
						{
							printf("crc = NOK\n");
						}
					}
					printf("ScratchPAD data = %X %X %X %X %X %X %X %X %X\n", get[8], get[7], get[6], get[5], get[4], get[3], get[2], get[1], get[0]);
					// расчёт температуры
					//temp_msb = get[1]; 
					//temp_lsb = get[0];
					
					// -
					if(getbits(get[1], 7, 1))
					{
						temp = get[1] << 8 | get[0];
						temp = (~temp) + 1;
						temperatura = (temp * 0.0625) * (-1);
						printf("temp = %f *C\n", temperatura);
					}
					// +
					else 
					{
						temp = get[1] << 8 | get[0];
						temperatura = temp * 0.0625;
						printf("temp = %f *C\n", temperatura);
					}
						
				}
				else
					printf("1-wire device not detected(2) or short_detected = %d\n", short_detected);
			}
		}
		else
			printf("1-wire device not detected(1) or sensors = 0 or short_detected = %d\n", short_detected);
		xSemaphoreGive(i2c_mux);
	}
	vTaskDelete(NULL);
}


void app_main()
{
	printf("Project - Termostat\n");
	
	i2c_mux = xSemaphoreCreateMutex();
	
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
    gpio_evt_queue = xQueueCreate(5, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(ENC, "ENC", 1548, NULL, 10, NULL);
	
	ENC_queue = xQueueCreate(10, sizeof(uint32_t));
	
	//install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_CLK, gpio_isr_handler, (void*) GPIO_ENC_CLK);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_DT, gpio_isr_handler, (void*) GPIO_ENC_DT);
	//hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_ENC_SW, gpio_isr_handler, (void*) GPIO_ENC_SW);
	
	
	i2c_master_init();
	
	if(i2c_mux){
		xStatusOLED = xTaskCreate(vDisplay, "vDisplay", 1024 * 2, NULL, 10, &xDisplay_Handle);
		if(xStatusOLED == pdPASS)
			printf("Task vDisplay is created!\n");
		else
			printf("Task vDisplay is not created\n");
		
		
		xStatusReadTemp = xTaskCreate(vReadTemp, "vReadTemp", 1024 * 2, NULL, 10, &xRead_Temp_Handle);
		if(xStatusReadTemp == pdPASS)
			printf("Task vReadTemp is created!\n");
		else
			printf("Task vReadTemp is not created\n");
	}
	else
		printf("mutex (i2c_mux) is not created\n");
}

