/**
 * original author:  Tilen Majerle<tilen@majerle.eu>
 * modification for STM32f10x: Alexander Lutsai<s.lyra@ya.ru>
 * modification for ESP32: Zhan Beshchanov<zip1982b@gmail.com>
   ----------------------------------------------------------------------
    Copyright (C) Zhan Beshchanov, 2018
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
   SSD1306    |ESP32        |DESCRIPTION

   VCC        |3.3V         |
   GND        |GND          |
   SCL        |19           |Serial clock line
   SDA        |18           |Serial data line
   
   
   
 */
#ifndef ssd1306_I2C_H
#define ssd1306_I2C_H

#include "driver/i2c.h"


void i2c_master_init();

/**
 * @brief  Writes single byte to slave
 * @param  
 * @param  
 * @param  reg: register to write to
 * @param  data_or_command: data Or Command to be written
 * @retval None
 */
void ssd1306_I2C_Write(uint8_t reg, uint8_t data_or_command);

/**
 * @brief  Writes multi bytes to slave
 * @param  
 * @param  
 * @param  reg: register to write to
 * @param  *data_or_command: pointer to data array to write it to slave
 * @param  size: how many bytes will be written
 * @retval None
 */
void ssd1306_I2C_WriteMulti(uint8_t reg, uint8_t *data_or_command, size_t size);


uint8_t ssd1306_I2C_IsDeviceConnected(void);


#endif

