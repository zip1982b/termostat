/**
 * original author:  Tilen Majerle<tilen@majerle.eu>
 * modification for STM32f10x: Alexander Lutsai<s.lyra@ya.ru>

   ----------------------------------------------------------------------
   	Copyright (C) Alexander Lutsai, 2016
    Copyright (C) Tilen Majerle, 2015

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
 */
 
//#include <stdio.h>

#include "ssd1306_i2c.h"
#include "fonts.h"
#include "ssd1306.h"


/* Write command */
#define SSD1306_WRITECOMMAND(command)      ssd1306_I2C_Write(0x00, (command))
/* Write data */
#define SSD1306_WRITEDATA(data)            ssd1306_I2C_Write(0x40, (data))
/* Absolute value */
#define ABS(x)   ((x) > 0 ? (x) : -(x))






extern uint8_t contrast;





/* SSD1306 data buffer */
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* Private SSD1306 structure */
typedef struct {
	uint16_t CurrentX;
	uint16_t CurrentY;
	uint8_t Inverted;
	uint8_t Initialized;
} SSD1306_t;

/* Private variable */
static SSD1306_t SSD1306;





uint8_t SSD1306_Init(void) {

	/* Init I2C */
	//i2c_master_init();
	
	/* Check if LCD connected to I2C */
	if (!ssd1306_I2C_IsDeviceConnected()) {
		/* Return false */
		return 0;
	}
	
	/* Init LCD */
	SSD1306_WRITECOMMAND(OLED_CMD_DISPLAY_OFF); //display off
	SSD1306_WRITECOMMAND(OLED_CMD_SET_MEMORY_ADDR_MODE); //Set Memory Addressing Mode   
	SSD1306_WRITECOMMAND(0x10); //00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	SSD1306_WRITECOMMAND(0xB0); //Set Page Start Address for Page Addressing Mode,0-7
	SSD1306_WRITECOMMAND(OLED_CMD_SET_COM_SCAN_DIRECTION); //Set COM Output Scan Direction
	SSD1306_WRITECOMMAND(0x00); //---set low column address
	SSD1306_WRITECOMMAND(0x10); //---set high column address
	SSD1306_WRITECOMMAND(0x40); //--set start line address
	SSD1306_WRITECOMMAND(0x81); //--set contrast control register
	SSD1306_WRITECOMMAND(0xFF);
	SSD1306_WRITECOMMAND(0xA1); //--set segment re-map 0 to 127
	SSD1306_WRITECOMMAND(0xA6); //--set normal display
	SSD1306_WRITECOMMAND(0xA8); //--set multiplex ratio(1 to 64)
	SSD1306_WRITECOMMAND(0x3F); //
	SSD1306_WRITECOMMAND(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	SSD1306_WRITECOMMAND(0xD3); //-set display offset
	SSD1306_WRITECOMMAND(0x00); //-not offset
	SSD1306_WRITECOMMAND(0xD5); //--set display clock divide ratio/oscillator frequency
	SSD1306_WRITECOMMAND(0xF0); //--set divide ratio
	SSD1306_WRITECOMMAND(0xD9); //--set pre-charge period
	SSD1306_WRITECOMMAND(0x22); //
	SSD1306_WRITECOMMAND(0xDA); //--set com pins hardware configuration
	SSD1306_WRITECOMMAND(0x12);
	SSD1306_WRITECOMMAND(0xDB); //--set vcomh
	SSD1306_WRITECOMMAND(0x20); //0x20,0.77xVcc
	SSD1306_WRITECOMMAND(0x8D); //--set DC-DC enable
	SSD1306_WRITECOMMAND(0x14); //
	SSD1306_WRITECOMMAND(0xAF); //--turn on SSD1306 panel
	
	/* Clear screen */
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	
	/* Update screen */
	SSD1306_UpdateScreen();
	
	/* Set default values */
	SSD1306.CurrentX = 0;
	SSD1306.CurrentY = 0;
	
	/* Initialized OK */
	SSD1306.Initialized = 1;
	
	/* Return OK */
	return 1;
}

void SSD1306_UpdateScreen(void) {
	uint8_t m;
	
	for (m = 0; m < 8; m++) {
		SSD1306_WRITECOMMAND(0xB0 + m);
		SSD1306_WRITECOMMAND(0x00);
		SSD1306_WRITECOMMAND(0x10);
		
		/* Write multi data */
		ssd1306_I2C_WriteMulti(0x40, &SSD1306_Buffer[SSD1306_WIDTH * m], SSD1306_WIDTH);
	}
}

void SSD1306_ToggleInvert(void) {
	uint16_t i;
	
	/* Toggle invert */
	SSD1306.Inverted = !SSD1306.Inverted;
	
	/* Do memory toggle */
	for (i = 0; i < sizeof(SSD1306_Buffer); i++) {
		SSD1306_Buffer[i] = ~SSD1306_Buffer[i];
	}
}

void SSD1306_Fill(SSD1306_COLOR_t color) {
	/* Set memory */
	memset(SSD1306_Buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}


void Del_str(char *s) {
	printf("Del_str %s", s);
	int len = strlen(s);
	int i;
	for(i = 0; i <= len; i++)
	{
		s[i] = ' ';
	}
}


void DisplayMode(uint8_t mode){
	if(mode == 0xAE){ SSD1306_WRITECOMMAND(0xAE); }
	else if(mode == 0xAF) { SSD1306_WRITECOMMAND(0xAF); }
}
 


void SSD1306_DrawPixel(uint16_t x, uint16_t y, SSD1306_COLOR_t color) {
	if (
		x >= SSD1306_WIDTH ||
		y >= SSD1306_HEIGHT
	) {
		/* Error */
		return;
	}
	
	/* Check if pixels are inverted */
	if (SSD1306.Inverted) {
		color = (SSD1306_COLOR_t)!color;
	}
	
	/* Set color */
	if (color == SSD1306_COLOR_WHITE) {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
	} else {
		SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
	}
}

void SSD1306_GotoXY(uint16_t x, uint16_t y) {
	/* Set write pointers */
	SSD1306.CurrentX = x;
	SSD1306.CurrentY = y;
}

char SSD1306_Putc(char ch, FontDef_t* Font, SSD1306_COLOR_t color) {
	uint32_t i, b, j;
	
	/* Check available space in LCD */
	if (
		SSD1306_WIDTH <= (SSD1306.CurrentX + Font->FontWidth) ||
		SSD1306_HEIGHT <= (SSD1306.CurrentY + Font->FontHeight)
	) {
		/* Error */
		return 0;
	}
	
	/* Go through font */
	for (i = 0; i < Font->FontHeight; i++) {
		b = Font->data[(ch - 32) * Font->FontHeight + i];
		for (j = 0; j < Font->FontWidth; j++) {
			if ((b << j) & 0x8000) {
				SSD1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR_t) color);
			} else {
				SSD1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR_t)!color);
			}
		}
	}
	
	/* Increase pointer */
	SSD1306.CurrentX += Font->FontWidth;
	
	/* Return character written */
	return ch;
}

char SSD1306_Puts(char* str, FontDef_t* Font, SSD1306_COLOR_t color) {
	/* Write characters */
	while (*str) {
		/* Write character by character */
		if (SSD1306_Putc(*str, Font, color) != *str) {
			/* Return error */
			return *str;
		}
		
		/* Increase string pointer */
		str++;
	}
	
	/* Everything OK, zero should be returned */
	return *str;
}
 

void SSD1306_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, SSD1306_COLOR_t c) {
	int16_t dx, dy, sx, sy, err, e2, i, tmp; 
	
	/* Check for overflow */
	if (x0 >= SSD1306_WIDTH) {
		x0 = SSD1306_WIDTH - 1;
	}
	if (x1 >= SSD1306_WIDTH) {
		x1 = SSD1306_WIDTH - 1;
	}
	if (y0 >= SSD1306_HEIGHT) {
		y0 = SSD1306_HEIGHT - 1;
	}
	if (y1 >= SSD1306_HEIGHT) {
		y1 = SSD1306_HEIGHT - 1;
	}
	
	dx = (x0 < x1) ? (x1 - x0) : (x0 - x1); 
	dy = (y0 < y1) ? (y1 - y0) : (y0 - y1); 
	sx = (x0 < x1) ? 1 : -1; 
	sy = (y0 < y1) ? 1 : -1; 
	err = ((dx > dy) ? dx : -dy) / 2; 

	if (dx == 0) {
		if (y1 < y0) {
			tmp = y1;
			y1 = y0;
			y0 = tmp;
		}
		
		if (x1 < x0) {
			tmp = x1;
			x1 = x0;
			x0 = tmp;
		}
		
		/* Vertical line */
		for (i = y0; i <= y1; i++) {
			SSD1306_DrawPixel(x0, i, c);
		}
		
		/* Return from function */
		return;
	}
	
	if (dy == 0) {
		if (y1 < y0) {
			tmp = y1;
			y1 = y0;
			y0 = tmp;
		}
		
		if (x1 < x0) {
			tmp = x1;
			x1 = x0;
			x0 = tmp;
		}
		
		/* Horizontal line */
		for (i = x0; i <= x1; i++) {
			SSD1306_DrawPixel(i, y0, c);
		}
		
		/* Return from function */
		return;
	}
	
	while (1) {
		SSD1306_DrawPixel(x0, y0, c);
		if (x0 == x1 && y0 == y1) {
			break;
		}
		e2 = err; 
		if (e2 > -dx) {
			err -= dy;
			x0 += sx;
		} 
		if (e2 < dy) {
			err += dx;
			y0 += sy;
		} 
	}
}

void SSD1306_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c) {
	/* Check input parameters */
	if (
		x >= SSD1306_WIDTH ||
		y >= SSD1306_HEIGHT
	) {
		/* Return error */
		return;
	}
	
	/* Check width and height */
	if ((x + w) >= SSD1306_WIDTH) {
		w = SSD1306_WIDTH - x;
	}
	if ((y + h) >= SSD1306_HEIGHT) {
		h = SSD1306_HEIGHT - y;
	}
	
	/* Draw 4 lines */
	SSD1306_DrawLine(x, y, x + w, y, c);         /* Top line */
	SSD1306_DrawLine(x, y + h, x + w, y + h, c); /* Bottom line */
	SSD1306_DrawLine(x, y, x, y + h, c);         /* Left line */
	SSD1306_DrawLine(x + w, y, x + w, y + h, c); /* Right line */
}

void SSD1306_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, SSD1306_COLOR_t c) {
	uint8_t i;
	
	/* Check input parameters */
	if (
		x >= SSD1306_WIDTH ||
		y >= SSD1306_HEIGHT
	) {
		/* Return error */
		return;
	}
	
	/* Check width and height */
	if ((x + w) >= SSD1306_WIDTH) {
		w = SSD1306_WIDTH - x;
	}
	if ((y + h) >= SSD1306_HEIGHT) {
		h = SSD1306_HEIGHT - y;
	}
	
	/* Draw lines */
	for (i = 0; i <= h; i++) {
		/* Draw lines */
		SSD1306_DrawLine(x, y + i, x + w, y + i, c);
	}
}

void SSD1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color) {
	/* Draw lines */
	SSD1306_DrawLine(x1, y1, x2, y2, color);
	SSD1306_DrawLine(x2, y2, x3, y3, color);
	SSD1306_DrawLine(x3, y3, x1, y1, color);
}


void SSD1306_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, SSD1306_COLOR_t color) {
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0, 
	yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0, 
	curpixel = 0;
	
	deltax = ABS(x2 - x1);
	deltay = ABS(y2 - y1);
	x = x1;
	y = y1;

	if (x2 >= x1) {
		xinc1 = 1;
		xinc2 = 1;
	} else {
		xinc1 = -1;
		xinc2 = -1;
	}

	if (y2 >= y1) {
		yinc1 = 1;
		yinc2 = 1;
	} else {
		yinc1 = -1;
		yinc2 = -1;
	}

	if (deltax >= deltay){
		xinc1 = 0;
		yinc2 = 0;
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;
	} else {
		xinc2 = 0;
		yinc1 = 0;
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;
	}

	for (curpixel = 0; curpixel <= numpixels; curpixel++) {
		SSD1306_DrawLine(x, y, x3, y3, color);

		num += numadd;
		if (num >= den) {
			num -= den;
			x += xinc1;
			y += yinc1;
		}
		x += xinc2;
		y += yinc2;
	}
}

void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    SSD1306_DrawPixel(x0, y0 + r, c);
    SSD1306_DrawPixel(x0, y0 - r, c);
    SSD1306_DrawPixel(x0 + r, y0, c);
    SSD1306_DrawPixel(x0 - r, y0, c);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SSD1306_DrawPixel(x0 + x, y0 + y, c);
        SSD1306_DrawPixel(x0 - x, y0 + y, c);
        SSD1306_DrawPixel(x0 + x, y0 - y, c);
        SSD1306_DrawPixel(x0 - x, y0 - y, c);

        SSD1306_DrawPixel(x0 + y, y0 + x, c);
        SSD1306_DrawPixel(x0 - y, y0 + x, c);
        SSD1306_DrawPixel(x0 + y, y0 - x, c);
        SSD1306_DrawPixel(x0 - y, y0 - x, c);
    }
}

void SSD1306_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, SSD1306_COLOR_t c) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    SSD1306_DrawPixel(x0, y0 + r, c);
    SSD1306_DrawPixel(x0, y0 - r, c);
    SSD1306_DrawPixel(x0 + r, y0, c);
    SSD1306_DrawPixel(x0 - r, y0, c);
    SSD1306_DrawLine(x0 - r, y0, x0 + r, y0, c);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SSD1306_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, c);
        SSD1306_DrawLine(x0 + x, y0 - y, x0 - x, y0 - y, c);

        SSD1306_DrawLine(x0 + y, y0 + x, x0 - y, y0 + x, c);
        SSD1306_DrawLine(x0 + y, y0 - x, x0 - y, y0 - x, c);
    }
}
 
void SSD1306_ON(void) {
	SSD1306_WRITECOMMAND(0x8D);  
	SSD1306_WRITECOMMAND(0x14);  
	SSD1306_WRITECOMMAND(0xAF);  
}
void SSD1306_OFF(void) {
	SSD1306_WRITECOMMAND(0x8D);  
	SSD1306_WRITECOMMAND(0x10);
	SSD1306_WRITECOMMAND(0xAE);  
}


void vSetContrast(uint8_t contrast) {
	SSD1306_WRITECOMMAND(0x81);  
	SSD1306_WRITECOMMAND(contrast);  
	//SSD1306_WRITECOMMAND(0x);  
}

void vDrawMenu(uint8_t menuitem, uint8_t state, uint8_t temp, uint8_t contrast, esp_chip_info_t chip_info, uint8_t sensors, uint8_t status_relay, float temp_average)
{
	char menuItem1[] = "Found sensors";
	char menuItem2[] = "Set temp";
	char menuItem3[] = "Process view";
	char menuItem4[] = "Chip info";
	char menuItem5[] = "Network info";
	char menuItem6[] = "Contrast";
	char menuItem7[] = "Display sleep";
	char menuItem8[] = "Reset CPU";
	
	
	
	
	if(state > 6)
	{
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts("MAIN MENU", &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE);
	
	/*************** state 10 - Frame 1 ******************************/
		if(menuitem == 1 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 1); // Set relay
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 2 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 1); // Set temp
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 3 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 1); // Process view
			vDisplayMenuItem(menuItem4, 45, 0);
			SSD1306_UpdateScreen();
		}   
		else if(menuitem == 4 && state == 10)
		{
			vDisplayMenuItem(menuItem1, 15, 0);
			vDisplayMenuItem(menuItem2, 25, 0);
			vDisplayMenuItem(menuItem3, 35, 0);
			vDisplayMenuItem(menuItem4, 45, 1); // Chip info
			SSD1306_UpdateScreen();
		}
	/************ state 20 - Frame 2 **********************************/
		else if(menuitem == 2 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 1); // Set temp
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 3 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 1); // Process view
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 4 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 1); // Chip info
			vDisplayMenuItem(menuItem5, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 5 && state == 20)
		{
			vDisplayMenuItem(menuItem2, 15, 0);
			vDisplayMenuItem(menuItem3, 25, 0);
			vDisplayMenuItem(menuItem4, 35, 0);
			vDisplayMenuItem(menuItem5, 45, 1); // Network info
			SSD1306_UpdateScreen();
		}
	/************* state 30 - Frame 3 *********************************/
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
		/************* state 40 - Frame 4 *********************************/
		else if(menuitem == 4 && state == 40)
		{
			vDisplayMenuItem(menuItem4, 15, 1);
			vDisplayMenuItem(menuItem5, 25, 0);
			vDisplayMenuItem(menuItem6, 35, 0);
			vDisplayMenuItem(menuItem7, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 5 && state == 40)
		{
			vDisplayMenuItem(menuItem4, 15, 0);
			vDisplayMenuItem(menuItem5, 25, 1);
			vDisplayMenuItem(menuItem6, 35, 0);
			vDisplayMenuItem(menuItem7, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 6 && state == 40)
		{
			vDisplayMenuItem(menuItem4, 15, 0);
			vDisplayMenuItem(menuItem5, 25, 0);
			vDisplayMenuItem(menuItem6, 35, 1);
			vDisplayMenuItem(menuItem7, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 7 && state == 40)
		{
			vDisplayMenuItem(menuItem4, 15, 0);
			vDisplayMenuItem(menuItem5, 25, 0);
			vDisplayMenuItem(menuItem6, 35, 0);
			vDisplayMenuItem(menuItem7, 45, 1);
			SSD1306_UpdateScreen();
		}
		/************* state 50 - Frame 5 *********************************/
		else if(menuitem == 5 && state == 50)
		{
			vDisplayMenuItem(menuItem5, 15, 1);
			vDisplayMenuItem(menuItem6, 25, 0);
			vDisplayMenuItem(menuItem7, 35, 0);
			vDisplayMenuItem(menuItem8, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 6 && state == 50)
		{
			vDisplayMenuItem(menuItem5, 15, 0);
			vDisplayMenuItem(menuItem6, 25, 1);
			vDisplayMenuItem(menuItem7, 35, 0);
			vDisplayMenuItem(menuItem8, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 7 && state == 50)
		{
			vDisplayMenuItem(menuItem5, 15, 0);
			vDisplayMenuItem(menuItem6, 25, 0);
			vDisplayMenuItem(menuItem7, 35, 1);
			vDisplayMenuItem(menuItem8, 45, 0);
			SSD1306_UpdateScreen();
		}
		else if(menuitem == 8 && state == 50)
		{
			vDisplayMenuItem(menuItem5, 15, 0);
			vDisplayMenuItem(menuItem6, 25, 0);
			vDisplayMenuItem(menuItem7, 35, 0);
			vDisplayMenuItem(menuItem8, 45, 1);
			SSD1306_UpdateScreen();
		}
	/***************** End state *****************************/
	}
	/***************** Page 1 end ***************************/
	
	/*****************  Page 2 ******************************/
	/*** found sensors view ***/
	else if(state == 1){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(20, 0); // установить курсор в позицию 20 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem1, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
	

		SSD1306_GotoXY(5, 20);
		SSD1306_Puts("Quantity:", &Font_11x18, SSD1306_COLOR_WHITE);
		char s[5];
		itoa(sensors, s, 10);
		SSD1306_Puts(s, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
	
	/*** set temp view ***/
	else if(state == 2){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem2, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		char t[24];
		itoa(temp, t, 10);
		SSD1306_Puts(t, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
	
	/*** Process view ***/
	else if(state == 3){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem3, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		if(status_relay)
			SSD1306_Puts("Relay ON", &Font_7x10, SSD1306_COLOR_WHITE);
		else
			SSD1306_Puts("Relay OFF", &Font_7x10, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		SSD1306_Puts("t = ", &Font_7x10, SSD1306_COLOR_WHITE);
		
		char buf[18];
		sprintf(buf, "%5.5f", temp_average);
		SSD1306_Puts(buf, &Font_7x10, SSD1306_COLOR_WHITE);
		SSD1306_Puts("*C", &Font_7x10, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
	
	/*** chip info view ***/
	else if(state == 4){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem4, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("model: ESP32", &Font_7x10, SSD1306_COLOR_WHITE);
		
		
		SSD1306_GotoXY(5, 25);
		SSD1306_Puts("cores: ", &Font_7x10, SSD1306_COLOR_WHITE);
		char core[5];
		itoa(chip_info.cores, core, 10);
		SSD1306_Puts(core, &Font_7x10, SSD1306_COLOR_WHITE);
		
		
		SSD1306_GotoXY(5, 35);
		SSD1306_Puts("revision: ", &Font_7x10, SSD1306_COLOR_WHITE);
		char rev[3];
		itoa(chip_info.revision, rev, 10);
		SSD1306_Puts(rev, &Font_7x10, SSD1306_COLOR_WHITE);
		
		SSD1306_UpdateScreen();
	}
	
	/*** network info view ***/
	else if(state == 5){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem5, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("IP address: ", &Font_7x10, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		SSD1306_Puts("192.168.1.12", &Font_7x10, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
	
	/*** Contrast view ***/
	else if(state == 6){
		SSD1306_Fill(SSD1306_COLOR_BLACK);
		SSD1306_GotoXY(25, 0); // установить курсор в позицию 15 - горизонталь, 0 - вертикаль
		SSD1306_Puts(menuItem6, &Font_7x10, SSD1306_COLOR_WHITE); // шрифт Font_7x10, белым цветом
		SSD1306_DrawLine(10, 12, 110, 12, SSD1306_COLOR_WHITE); // draw line
		SSD1306_GotoXY(5, 15);
		SSD1306_Puts("Value", &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_GotoXY(5, 35);
		char cont[24];
		itoa(contrast, cont, 10);
		SSD1306_Puts(cont, &Font_11x18, SSD1306_COLOR_WHITE);
		SSD1306_UpdateScreen();
	}
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






