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
















/*
 * GPIO status:
 *	GPIO14: output (relay1)
 *	GPIO12: output (relay2)
 *	GPIO33:(clk - encoder) input
 *	GPIO25:(dt - encoder) input
 *	GPIO26:(sw - encoder) input
 *
*/
#define GPIO_RELAY1    14
#define GPIO_RELAY2    12
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_RELAY1) | (1ULL<<GPIO_RELAY2))
#define GPIO_ENC_CLK     33
#define GPIO_ENC_DT		 25
#define GPIO_ENC_SW		 26
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_ENC_CLK) | (1ULL<<GPIO_ENC_DT) | (1ULL<<GPIO_ENC_SW)) 
#define ESP_INTR_FLAG_DEFAULT 0


void vDrawMenu(uint8_t menu, uint8_t state);
void vDisplayMenuItem(char *item, uint8_t position, uint8_t selected);
void vDisplayMenuPage(char *menuItem, uint8_t *value);
void vDisplayCharMenuPage(char *menuitem, char *value);


/* 	
	cr	- clockwise rotation
	ccr	- counter clockwise rotation
	bp	- button pressed
*/
enum action{cr, ccr, bp}; 

portBASE_TYPE xStatusOLED;
xTaskHandle xDisplay_Handle;

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





void vDisplay(void *pvParameter)
{
	portBASE_TYPE xStatusReceive;
	enum action rotate;
	uint8_t state = 10; // default state
	
	uint8_t menuitem = 1;
	
	uint8_t contrast = 100;
	uint8_t volume = 50;
	
	//char *language[3] = { "EN", "ES", "EL" };
	int selectedLanguage = 0;

	//char *difficulty[2] = { "EASY", "HARD" };
	int selectedDifficulty = 0;


	//char *Relay1[2] = { "OFF", "ON" };
	int selectedRelay1 = 0;


	uint8_t change = 1;
	SSD1306_Init();
	
	uint8_t down_cw;
	uint8_t up_ccw;
	uint8_t press_button;
	
	uint8_t frame = 1;
	
	
    while(1) {
		down_cw = 0;
		up_ccw = 0;
		press_button = 0;
		
		
		if(change)
		{
			vDrawMenu(menuitem, state);
			change = 0;
		}
	/***** Read Encoder ***********************/
		xStatusReceive = xQueueReceive(ENC_queue, &rotate, 100/portTICK_RATE_MS); // portMAX_DELAY - (very long time) сколь угодно долго
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
		else
			//printf("[vDisplay]xStatusReceive = %s\n", xStatusReceive);
			printf("[vDisplay]xStatusReceive not pdPass\n");
	/***** End Read Encoder ***********************/	
		
		
		switch(state){
			/*** Frame 1 ************************************************/
			case 10:
				printf("State = 10\n");
				frame = 1;
				if(down_cw) { menuitem++;}
				else if(up_ccw) { menuitem--; }
				
				// go to Contrast
				else if(press_button && menuitem == 1) { state = 1; }
				
				// go to Volume
				else if(press_button && menuitem == 2) { state = 2; }
				
				// go to Language
				else if(press_button && menuitem == 3) { state = 3; }
				
				// go to Difficulty
				else if(press_button && menuitem == 4) { state = 4; }
				
				
				if(menuitem == 0) { menuitem = 1; }
				
				if(menuitem > 4){
					state = 20;
					frame = 2;
				}

				break;
			/*********************************************************/
			
			/*** Frame 2 *********************************************/
			case 20:
				printf("State = 20\n");
				if(down_cw) { menuitem++; }
				else if(up_ccw) { menuitem--; }
				
				// go to Volume
				else if(press_button && menuitem == 2) { state = 2; }
				
				// go to Language
				else if(press_button && menuitem == 3) { state = 3; }
				
				// go to Difficulty
				else if(press_button && menuitem == 4) { state = 4; }
				
				// go to Relay
				else if(press_button && menuitem == 5) { state = 5; }
				
				if(menuitem > 5){ 
					state = 30;
					frame = 3;
				}
				else if(menuitem < 2) { 
					state = 10;
					frame = 1;
				}
				
				
				break;
			/*********************************************************/
			
			/*** Frame 3 *********************************************/
			case 30:
				printf("State = 30\n");
				if(up_ccw) { menuitem--; }
				else if(down_cw) { menuitem++; }
				else if(menuitem == 7) { menuitem = 6; }
				
				// go to Language
				else if(press_button && menuitem == 3) { state = 3; }
				
				// go to Difficulty
				else if(press_button && menuitem == 4) { state = 4; }
				
				// go to Relay
				else if(press_button && menuitem == 5) { state = 5; }
				
				if(menuitem < 3) { 
					state = 20;
					frame = 2;
				}
				
				if(press_button && menuitem == 6) { 
					//resetDefaults();
					contrast = 100;
					volume = 50;
					selectedLanguage = 0;
					selectedDifficulty = 0;
					vSetContrast(contrast);
					selectedRelay1 = 0;
					turnRelay1_Off();
				}
				
				break;
			/********************************************************/
			
			/*** Contrast ***/
			case 1:
				printf("State = 1\n");
				if(down_cw){
					contrast++;
					vSetContrast(contrast);
				}
				else if(up_ccw){
					contrast--;
					vSetContrast(contrast);
				}
				else if(press_button && frame == 1) { state = 10; }
				break;
				
			/*** Volume ***/
			case 2:
				printf("State = 2\n");
				if(down_cw){ volume++; }
				else if(up_ccw){ volume--; }
				else if(press_button && frame == 1) { state = 10; } // go to Frame 1
				else if(press_button && frame == 2) { state = 20; } // go to Frame 2
				break;
			
			/*** Language ***/
			case 3:
				printf("State = 3\n");
				if(down_cw){ selectedLanguage++; }
				else if(up_ccw){ selectedLanguage--; }
				else if(press_button && frame == 1){ state = 10; }
				else if(press_button && frame == 2){ state = 20; }
				else if(press_button && frame == 3){ state = 30; }
				
				if(selectedLanguage == -1) { selectedLanguage = 2; }
				else if(selectedLanguage == 3) { selectedLanguage = 0; }
				break;
			
			/*** Difficulty ***/
			case 4:
				printf("State = 4\n");
				if(down_cw){ selectedDifficulty++; }
				else if(up_ccw){ selectedDifficulty--; }
				else if(press_button && frame == 1){ state = 10; }
				else if(press_button && frame == 2){ state = 20; }
				else if(press_button && frame == 3){ state = 30; }
				
				if(selectedDifficulty == -1) { selectedDifficulty = 1;}
				else if(selectedDifficulty == 2) { selectedDifficulty = 0;}
				break;
			
			/*** Relay ***/
			case 5:
				printf("State = 5\n");
				if(down_cw){ selectedRelay1++; }
				else if(up_ccw){ selectedRelay1--; }
				else if(press_button && frame == 2){ state = 20; }
				else if(press_button && frame == 3){ state = 30; }
				
				if(selectedRelay1 >= 2) { selectedRelay1 = 0; }
				else if(selectedRelay1 <= -1) { selectedRelay1 = 1; }
				break;
		}
    }
	vTaskDelete(NULL);
}


/*
void resetDefaults(void)
  {
    contrast = 100;
    volume = 50;
    selectedLanguage = 0;
    selectedDifficulty = 0;
    vSetContrast(contrast);
	selectedRelay1 = 0;
    turnRelay1_Off();
  }
*/
  
  

void vDrawMenu(uint8_t menuitem, uint8_t state, uint8_t contrast, uint8_t volume, int selectedLanguage)
{
	char menuItem1[] = "Contrast";
	char menuItem2[] = "Volume";
	char menuItem3[] = "Language";
	char menuItem4[] = "Difficulty";
	char menuItem5[] = "Relay1";
	char menuItem6[] = "Reset";
	
	char language[3] = { "EN", "ES", "EL" };
	char difficulty[2] = { "EASY", "HARD" };
	char Relay1[2] = { "OFF", "ON" };
	
	
	if(state > 5)
	{
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts("MAIN MENU", &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE);
	
	/*************** state 10 ******************************/
		if(menuitem == 1 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 1); // Contrast
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 2 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 1); // Volume
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 3 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 1); // Language
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		} eds   
		else if(menuitem == 4 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 1); // Difficulty
			SSD1306_UpdateScreen();
		}
	/************ state 20 **********************************/
		else if(menuitem == 2 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 1);
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 3 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 1);
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 4 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 1);
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 5 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 1);
			SSD1306_UpdateScreen();
		}
	/************* state 30 *********************************/
		else if(menuitem == 3 && state == 30)
		{
			vDisplayMenuItem(menuItem3, 15, 1);
			vDisplayMenuItem(menuItem4, 25, 0);
			vDisplayMenuItem(menuItem5, 35, 0);
			vDisplayMenuItem(menuItem6, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 4 && state == 30)
		{
			vDisplayMenuItem(menuItem3, 15, 0);
			vDisplayMenuItem(menuItem4, 25, 1);
			vDisplayMenuItem(menuItem5, 35, 0);
			vDisplayMenuItem(menuItem6, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 5 && state == 30)
		{
			vDisplayMenuItem(menuItem3, 15, 0);
			vDisplayMenuItem(menuItem4, 25, 0);
			vDisplayMenuItem(menuItem5, 35, 1);
			vDisplayMenuItem(menuItem6, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 6 && state == 30)
		{
			vDisplayMenuItem(menuItem3, 15, 0);
			vDisplayMenuItem(menuItem4, 25, 0);
			vDisplayMenuItem(menuItem5, 35, 0);
			vDisplayMenuItem(menuItem6, 45, 1);
			SSD1306_UpdateScreen();
		}
	
	/***************** End state *****************************/
	}
	/***************** Page 1 end ***************************/
	
	/*****************  Page 2 ******************************/
	else if(state < 10 && menuitem == 1){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem1, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		char v[24];
		itoa(contrast, v, 10);
		SSD1306_Puts(v, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
		//vDisplayMenuPage(menuItem1, &contrast);
	
	
	else if(state < 10 && menuitem == 2){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem2, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		char v[24];
		itoa(volume, v, 10);
		SSD1306_Puts(v, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
		//vDisplayMenuPage(menuItem2, &volume);
	
	
	else if(state < 10 && menuitem == 3){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem3, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		SSD1306_Puts(value, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
		//vDisplayCharMenuPage(menuItem3, language[selectedLanguage]);
	
	
	else if(state < 10 && menuitem == 4)
		vDisplayCharMenuPage(menuItem4, difficulty[selectedDifficulty]);
	
	
	else if(state < 10 && menuitem == 5)
		vDisplayCharMenuPage(menuItem5, Relay1[selectedRelay1]);
		if(selectedRelay1 == 1)
			turnRelay1_On();
		else if(selectedRelay1 == 0)
			turnRelay1_Off();	
}



void vDisplayMenuItem(char *item, uint8_t position, uint8_t selected)
{
	if(selected)
	{
		SSD1306_GotoXY(0, position);
		SSD1306_Puts(">", &Font_7x10, SSD1306_COLOR_BLACK); // шрифт Font_7x10, цвет чёрным
		SSD1306_Puts(item, &Font_7x10, SSD1306_COLOR_BLACK); // шрифт Font_7x10, цвет чёрным
	}
	else
	{
		SSD1306_GotoXY(0, position);
		SSD1306_Puts(">", &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, цвет белым
		SSD1306_Puts(item, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, цвет белым
	}
}



void vDisplayMenuPage(char *menuitem, uint8_t *value)
{
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	//SSD1306_UpdateScreen();
	SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
	SSD1306_Puts(menuitem, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
	SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
	SSD1306_GotoXY(5, 15);
	SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
	SSD1306_GotoXY(5, 35);
	char v[24];
	itoa(*value, v, 10);
	//printf("v = %s\n", v);
	SSD1306_Puts(v, &Font_11x18, SSD1306_COLOR_WHITE);
	SSD1306_UpdateScreen();
}




void vDisplayCharMenuPage(char *menuitem, char *value)
{
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	//SSD1306_UpdateScreen();
	SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
	SSD1306_Puts(menuitem, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
	SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
	SSD1306_GotoXY(5, 15);
	SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
	SSD1306_GotoXY(5, 35);
	SSD1306_Puts(value, &Font_11x18, SSD1306_COLOR_WHITE);
	SSD1306_UpdateScreen();
}


void turnRelay1_Off(void)
{
	gpio_set_level(GPIO_RELAY1, 0);
}

void turnRelay1_On(void)
{
	gpio_set_level(GPIO_RELAY1, 1);
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
    //disable interrup_ccwt - отключитли прерывания
    io_conf.intr_type = GPIO_INTR_DISABLE;
	
    //set as output mode - установка в режим Выход
    io_conf.mode = GPIO_MODE_OUTPUT;
	
    //bit mask of the pins that you want to set,e.g.GPIO14/12
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	
    //disable pull-down_cw mode - отключитли подтяжку к земле
    io_conf.pull_down_cw_en = 0;
	
    //disable pull-up_ccw mode - отключили подтяжку к питанию
    io_conf.pull_up_ccw_en = 0;
	
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
	
	
	xStatusOLED = xTaskCreate(vDisplay, "vDisplay", 1024 * 2, NULL, 10, &xDisplay_Handle);
		if(xStatusOLED == pdPASS)
			printf("Task vDisplay is created!\n");
		else
			printf("Task vDisplay is not created\n");
	
	
}

