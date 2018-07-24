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
 */
#include "ssd1306_i2c.h"







void ssd1306_I2C_WriteMulti(uint8_t reg, uint8_t *data_or_command, size_t size) {
	//uint8_t i;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_write(cmd, data_or_command, size, ACK_CHECK_EN);
	
	/*
	for (i = 0; i < count; i++) {
		i2c_master_write_byte(cmd, data[i], ACK_CHECK_EN);
	}
	*/
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[ssd1306_I2C_WriteMulti()] - OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[ssd1306_I2C_WriteMulti()] - Parameter error \n");
			break;
		case ESP_FAIL:
			printf("[ssd1306_I2C_WriteMulti()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[ssd1306_I2C_WriteMulti()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[ssd1306_I2C_WriteMulti()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[ssd1306_I2C_WriteMulti()] - default block\n");
	}
}








void ssd1306_I2C_Write(uint8_t reg, uint8_t data_or_command) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, data_or_command, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[ssd1306_I2C_Write()] - OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[ssd1306_I2C_Write()] - Parameter error \n");
			break;
		case ESP_FAIL:
			printf("[ssd1306_I2C_Write()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[ssd1306_I2C_Write()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[ssd1306_I2C_Write()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[ssd1306_I2C_Write()] - default block\n");
			break;
	}
}




uint8_t ssd1306_I2C_IsDeviceConnected(void) {
	uint8_t connected = 0;
	/* Try to start, function will return 0 in case device will send ACK */
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, OLED_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
	//i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, ACK_CHECK_EN); //OLED_CMD_DISPLAY_OFF
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[ssd1306_I2C_IsDeviceConnected()] - OLED Display address is true = OK \n");
			connected = 1;
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[ssd1306_I2C_IsDeviceConnected()] - Parameter error \n");
			return connected;
		case ESP_FAIL:
			printf("[ssd1306_I2C_IsDeviceConnected()] - Sending command error, slave doesn`t ACK the transfer \n");
			return connected;
		case ESP_ERR_INVALID_STATE:
			printf("[ssd1306_I2C_IsDeviceConnected()] - i2c driver not installed or not in master mode \n");
			return connected;
		case ESP_ERR_TIMEOUT:
			printf("[ssd1306_I2C_IsDeviceConnected()] - Operation timeout because the bus is busy \n");
			return connected;
		default:
			printf("[ssd1306_I2C_IsDeviceConnected()] - default block");
			return connected;
	}
	return connected;
}
