/*
Project name: Termostat
Autor: zip1982b

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <esp_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"
#include "freertos/queue.h"

#define GPIO_RELAY1    12
//#define GPIO_RELAY2    14
#define GPIO_OUTPUT_PIN_SEL (1ULL<<GPIO_RELAY1) //| (1ULL<<GPIO_RELAY2)
#define GPIO_ENC_CLK     35
#define GPIO_ENC_DT		 32
#define GPIO_ENC_SW		 33
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_ENC_CLK) | (1ULL<<GPIO_ENC_DT) | (1ULL<<GPIO_ENC_SW)) 
#define ESP_INTR_FLAG_DEFAULT 0


/* 	
	cr	- clockwise rotation
	ccr	- counter clockwise rotation
	bp	- button pressed
*/
enum action{cr, ccr, bp}; 

portBASE_TYPE xStatusRelay1;
xTaskHandle xRelay1_Handle;

static xQueueHandle gpio_evt_queue = NULL;
static xQueueHandle ENC_queue = NULL;

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
			vTaskDelay(500 / portTICK_RATE_MS);
			xStatus = xQueueSendToBack(ENC_queue, &rotate, 100/portTICK_RATE_MS);
			gpio_set_intr_type(GPIO_ENC_SW, GPIO_INTR_NEGEDGE);//enable
			
		}
    }
}


void vRelay1(void *pvParameter)
{
	portBASE_TYPE xStatus;
	enum action rotate;
	
	
	
    while(1) {
		uint8_t down = 0;
		uint8_t up = 0;
		uint8_t middle = 0;
		
		
	/***** Read Encoder ***********************/
		xStatus = xQueueReceive(ENC_queue, &rotate, 100/portTICK_RATE_MS); // portMAX_DELAY - (very long time) сколь угодно долго
		if(xStatus == pdPASS)
		{
			printf("[vRelay1]rotate = %d\n", rotate);
			switch(rotate){
				case 0: //down = clockwise
					down = 1;
					break;
				case 1: //up = counter clockwise
					up = 1;
					break;
				case 2: //middle = button pressed
					middle = 1;
					break;
			}
		}
	/***** End Read Encoder ***********************/	
		
		
		
	/*** Relay ***/
		if(down){
			gpio_set_level(GPIO_RELAY1, 1);
		}
		if(up){
			gpio_set_level(GPIO_RELAY1, 0);
		}
		
		
	
    }
	vTaskDelete(NULL);
}



void app_main()
{
	printf("Project - Termostat\n");
	
	/* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
	
	
	/*** GPIO init ***/
	gpio_config_t io_conf;
	//Настройки GPIO для релейного ВЫХОДа
    //disable interrupt - отключитли прерывания
    io_conf.intr_type = GPIO_INTR_DISABLE;
	
    //set as output mode - установка в режим Выход
    io_conf.mode = GPIO_MODE_OUTPUT;
	
    //bit mask of the pins that you want to set,e.g.GPIO14/12
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	
    //disable pull-down mode - отключитли подтяжку к земле
    io_conf.pull_down_en = 0;
	
    //disable pull-up mode - отключили подтяжку к питанию
    io_conf.pull_up_en = 0;
	
    //configure GPIO with the given settings - конфигурирование портов с данными настройками (Выход)
    gpio_config(&io_conf);
	
	//Настройки GPIO Энкодера на ВХОД
	//interrupt of falling edge - прерывание по спадающему фронту и по возрастающему фронту
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO33/25/26 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    
	//enable pull-up mode
    //io_conf.pull_up_en = 1;
	
    gpio_config(&io_conf);
	
	//change gpio interrupt type for one pin
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
	
	
	xStatusRelay1 = xTaskCreate(vRelay1, "vRelay1", 1024 * 2, NULL, 10, &xRelay1_Handle);
		if(xStatusRelay1 == pdPASS)
			printf("Task vRelay1 is created!\n");
		else
			printf("Task vRelay1 is not created\n");
	
	
}

