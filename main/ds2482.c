#include <stdio.h>
#include "ds2482.h"


uint8_t short_detected; //short detected on 1-wire net
uint8_t ROM_NO[8];
uint8_t crc8;
uint8_t crc_tbl[] = {
	0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
};





/* getbits: получает n бит,  начиная с p позиции*/
uint8_t getbits(uint8_t x, int p, int n)
{
	return (x >> (p+1-n)) & ~(~0 << n);
}



/*
* Calculate the CRC8.
* Returns current global crc8 value
* See Application Note 27
*/
uint8_t calc_crc8(uint8_t value)
{
	crc8 = crc_tbl[crc8 ^ value];
	return crc8;
}






/* i2c command *
* S - start
* AD,0 - select ds2482 for write access
* [A] - Acknowledged
* DRST - Command "device reset" F0h
* WCFG - Command "write configuration" D2h
* SRP - Command "set read pointer" E1h
* 1WRS - Command "1-wire reset" B4h
* 1WWB - Command "1-wire write byte" A5h
* 1WRB - Command "1-wire read byte" 96h
* 1WT - Command "1-wire tripled" 78h
* 1WSB - Command "1-wire single bit" 87h
* AD,1 - select ds2482 for read access
* [SS] - status byte to read to verify state
* A\ - not Acknowledged
* P - stop condition
* [] indicates from slave
*/



uint8_t DS2482_reset(void)
{
	/*
	 *S AD,0 [A] DRST [A] P S AD,1 [A] [SS] A\ P 
	*/
	uint8_t status;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_DRST, ACK_CHECK_EN); //DRST - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[DS2482_reset()] - CMD_DRST = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_reset()] - Parameter error \n");
			return 0;
		case ESP_FAIL:
			printf("[DS2482_reset()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_reset()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[DS2482_reset()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[DS2482_reset()] - default block");
			return 0;
	}
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	i2c_master_read_byte(cmd, &status, NACK_VAL); // [SS] - notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[DS2482_reset()] - status = %d \n", status);
			/* check for failure due to incorrect read back of status */
			return ((status & 0xF7) == 0x10); //return 1
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_reset()] - Parameter error \n");
			return 0;
		case ESP_FAIL:
			printf("[DS2482_reset()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_reset()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[DS2482_reset()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[DS2482_reset()] - default block");
			return 0;
	}
}






uint8_t DS2482_write_config(uint8_t config)
{
	/*
	 * S AD,0 [A] WCFG [A] CF [A] P S AD,1 [A] [CF] A\ P
	*/
	uint8_t read_config;
	uint8_t tmp;
	uint8_t reg_config;
	tmp = config;
	reg_config = config | (~tmp << 4);//is the one`s complement of the lower nibble
	tmp = 0;
	//printf("reg_config = %d \n", reg_config);
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_WCFG, ACK_CHECK_EN); //WCFG - [A]
	i2c_master_write_byte(cmd, reg_config, ACK_CHECK_EN); //CF - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[DS2482_write_config()] - WCFG and CF = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_write_config()] - Parameter error (1) \n");
			return 0;
		case ESP_FAIL:
			printf("[DS2482_write_config()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_write_config()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[DS2482_write_config()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[DS2482_write_config()] - default block");
			return 0;
	}
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	i2c_master_read_byte(cmd, &read_config, NACK_VAL); // [CF] - notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[DS2482_write_config()] - read_config = %d \n", read_config);
			tmp = getbits(reg_config, 3, 4);
			if(tmp != read_config)
			{
				//handle error
				//printf("[DS2482_write_config()] - tmp = %d \n", tmp);
				DS2482_reset();
				return 0;
			}
			else if(tmp == read_config)
			{
				return 1;
			}
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_write_config()] - Parameter error (2) \n");
			return 0;
		case ESP_FAIL:
			printf("[DS2482_write_config()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_write_config()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[DS2482_write_config()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[DS2482_write_config()] - default block");
			return 0;
	}
}




uint8_t DS2482_detect()
{
	/* DS2482 default state */
	uint8_t c1WS = 0; //1-wire speed
	uint8_t cSPU = 0; //Strong pullup
	uint8_t cPPM = 0; //Precense pulse masking
	uint8_t cAPU = 1; //Active pullup
	
	if(!DS2482_reset())
	{
		printf("[DS2482_detect()] - ds2482 not detected or failure to perform reset \n");
		return 0;
	}
	else
	{
		printf("[DS2482_detect()] - ds2482 was reset \n");
	}
	
	// default configuration
	if(!DS2482_write_config(c1WS | cSPU | cPPM |cAPU))
	{
		printf("[DS2482_detect()] - ds2482 failure to write configuration byte \n");
		return 0;
	}
	else
	{
		printf("[DS2482_detect()] - ds2482 was written \n");
	}
	return 1;
}





uint8_t OWReset(void)
{
	/*
	 * S AD,0 [A] 1WRS [A] P S AD,1 [A] [Status] A [Status] A\ P
	 *									\--------/
	 *						Repeat until 1WB bit has changed to 0
	*/
	uint8_t status = 0;
	uint8_t *st;
	uint8_t PPD;
	int poll_count = 0;
	
	st = &status;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WRS, ACK_CHECK_EN); //1WRS - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWReset()] - CMD_1WRS = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWReset()] - Parameter error (1) \n");
			return 0;
		case ESP_FAIL:
			printf("[OWReset()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[OWReset()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[OWReset()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[OWReset()] - default block");
			return 0;
	}
	cmd = i2c_cmd_link_create(); 
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	do
	{
		i2c_master_read_byte(cmd, st, ACK_VAL); //[Status] A
	}
	while ((*st & STATUS_1WB) && (poll_count++ < POLL_LIMIT)); //Repeat untill 1WB bit has changed to 0
	i2c_master_read_byte(cmd, st, NACK_VAL); //[Status] notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			if(*st & STATUS_PPD)
			{
				PPD = 1;
			}
			else
			{
				PPD = 0;
			}
			
			if(*st & STATUS_SD)
			{
				short_detected = 1;
				printf("[OWReset()] - SD = %d \n", short_detected);
			}
			else
			{
				short_detected = 0;
				printf("[OWReset()] - SD = %d \n", short_detected);
			}
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWReset()] - Parameter error (2) \n");
			return 0;
		case ESP_FAIL:
			printf("[OWReset()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[OWReset()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[OWReset()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[OWReset()] - default block");
			return 0;
	}
	
	if(poll_count >= POLL_LIMIT)
	{
		DS2482_reset();
		return 0;
	}
	
	if(PPD)
		return 1;
	else
		return 0;
}




uint8_t OWTouchBit(uint8_t sendbit)
{
	/*
	 * S AD,0 [A] 1WSB [A] BB [A] P S AD,1 [A] [Status] A [Status] A\ P
	 *											\--------/
	 *							Repeat until 1WB bit has changed to 0
	 * BB indicates byte containing bit value in msbit
	*/
	uint8_t status = 0;
	uint8_t *st;
	int poll_count = 0;
	
	st = &status;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WSB, ACK_CHECK_EN); //1WSB - [A]
	i2c_master_write_byte(cmd, sendbit ? 0x80 : 0x00, ACK_CHECK_EN); //BB - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWTouchBit()] - CMD_1WSB = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWTouchBit()] - Parameter error (1) \n");
			return 0;
		case ESP_FAIL:
			printf("[OWTouchBit()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[OWTouchBit()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[OWTouchBit()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[OWTouchBit()] - default block");
			return 0;
	}
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	do
	{
		i2c_master_read_byte(cmd, st, ACK_VAL); //[Status] A
	}
	while ((*st & STATUS_1WB) && (poll_count++ < POLL_LIMIT)); //Repeat until 1WB bit has changed to 0
	i2c_master_read_byte(cmd, st, NACK_VAL); //[Status] notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWTouchBit()] - *st = %d \n", *st);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWTouchBit()] - Parameter error (2) \n");
			return 0;
		case ESP_FAIL:
			printf("[OWTouchBit()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[OWTouchBit()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[OWTouchBit()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[OWTouchBit()] - default block");
			return 0;
	}
	
	if(poll_count >= POLL_LIMIT)
	{
		DS2482_reset();
		return 0;
	}
	
	if(*st & STATUS_SBR)
		return 1;
	else
		return 0;
}




void OWWriteBit(uint8_t sendbit)
{
	OWTouchBit(sendbit);
}


uint8_t OWReadBit(void)
{
	return OWTouchBit(0x01);
}





void OWWriteByte(uint8_t sendbyte)
{
	/**
	* S AD,0 [A] 1WWB [A] DD [A] P S AD,1 [A] [Status] A [Status] A\ P
	*										  \---------/
	*								Repeat until 1WB bit has changed to 0
	* DD data to write
	*/
	uint8_t status;
	int poll_count = 0;
	uint8_t *st;
	st = &status;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WWB, ACK_CHECK_EN); //1WWB - [A]
	i2c_master_write_byte(cmd, sendbyte, ACK_CHECK_EN); //DD - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWWriteByte()] - CMD_1WWB = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWWriteByte()] - Parameter error (1) \n");
			break;
		case ESP_FAIL:
			printf("[OWWriteByte()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[OWWriteByte()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[OWWriteByte()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[OWWriteByte()] - default block\n");
	}
	vTaskDelay(30 / portTICK_RATE_MS);
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	do
	{
		i2c_master_read_byte(cmd, st, ACK_VAL); //[Status] A
	}
	while ((*st & STATUS_1WB) && (poll_count++ < POLL_LIMIT)); //Repeat until 1WB bit has changed to 0
	i2c_master_read_byte(cmd, st, NACK_VAL); //[Status] notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[OWWriteByte()] - *st = %d \n", *st);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWWriteByte()] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[OWWriteByte()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[OWWriteByte()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[OWWriteByte()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[OWWriteByte()] - default block");
	}
	
	if(poll_count >= POLL_LIMIT)
	{
		DS2482_reset();
	}
}


//Case A
uint8_t OWReadByte(void)
{
	/*
	* S AD,0 [A] 1WRB [A] P -idle- S AD,0 [A] SRP [A] E1 [A] S AD,1 [A] DD A\ P
	* 								 
	*						
	* DD - read data
	*
	*
	*
	*/
	uint8_t data;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WRB, ACK_CHECK_EN); //1WRB - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWReadByte()] - CMD_1WRB = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWReadByte()] - Parameter error (1) \n");
			break;
		case ESP_FAIL:
			printf("[OWReadByte()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[OWReadByte()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[OWReadByte()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[OWReadByte()] - default block\n");
	}		
	cmd = i2c_cmd_link_create();		
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_SRP, ACK_CHECK_EN); //SRP - [A]
	i2c_master_write_byte(cmd, 0xE1, ACK_CHECK_EN); //E1h - [A]
	
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	i2c_master_read_byte(cmd, &data, NACK_VAL); //[DD] notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[OWReadByte()] - data = %d \n", data);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWReadByte()] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[OWReadByte()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[OWReadByte()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[OWReadByte()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[OWReadByte()] - default block");
	}
	return data;
}




//Case C
/*
uint8_t OWReadByte(void)
{ */
	/*
	* S AD,0 [A] 1WRB [A] S AD,1 [A] [Status] A [Status] A\ S AD,0 [A] SRP [A] E1 [A] S AD,1 [A] DD A\ P
	* 								  \--------/
	*						Repeat until 1WB bit has changed to 0
	* DD - read data
	*
	*
	*
	*/
	/*
	uint8_t status;
	int poll_count = 0;
	uint8_t *st;
	st = &status;
	uint8_t data;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WRB, ACK_CHECK_EN); //1WRB - [A]
	
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	do
	{
		i2c_master_read_byte(cmd, st, ACK_VAL); //[Status] A
	}
	while ((*st & STATUS_1WB) && (poll_count++ < POLL_LIMIT)); //Repeat until 1WB bit has changed to 0
	i2c_master_read_byte(cmd, st, NACK_VAL); //[Status] notA
	
	if(poll_count >= POLL_LIMIT)
	{
		DS2482_reset();
		return 0;
	}
	
	vTaskDelay(30 / portTICK_RATE_MS);
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_SRP, ACK_CHECK_EN); //SRP - [A]
	i2c_master_write_byte(cmd, 0xE1, ACK_CHECK_EN); //E1h - [A]
	
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	i2c_master_read_byte(cmd, &data, NACK_VAL); //[DD] notA
	
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[OWReadByte()] - data = %d \n", data);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[OWReadByte()] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[OWReadByte()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[OWReadByte()] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[OWReadByte()] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[OWReadByte()] - default block");
	}
	return data;
}
*/


/*
* Send 8 bits of communication to the 1-wire net and return the result
* 8 bits read from the 1-wire net. 
* The parameter 'sendbyte' least significant 8 bits are used and the least significant
*  8 bits of the result are the return byte.
*/
uint8_t OWTouchByte(uint8_t sendbyte)
{
	if(sendbyte == 0xFF)
		return OWReadByte();
	else
	{
		OWWriteByte(sendbyte);
		return sendbyte;
	}
}



/*
* The 'OWBlock' transfers a block of data to and from the 1-wire net.
* The result is returned in the same buffer.
*/
void OWBlock(uint8_t *tran_buf, int tran_len)
{
	int i;
	for(i=0; i < tran_len; i++)
		tran_buf[i] = OWTouchByte(tran_buf[i]);
}



uint8_t DS2482_search_triplet(uint8_t search_direction)
{
	/*
	* S AD,0 [A] 1WT [A] SS [A] P S [A] AD,1 [A] [Status] A [Status] A\ P
	*											 \---------/
	*								Repeat until 1WB bit has changed to 0
	* SS indicates byte containing search direction bit value in msbit
	*/
	uint8_t status;
	int poll_count = 0;
	uint8_t *st;
	st = &status;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, CMD_1WT, ACK_CHECK_EN); //1WT - [A]
	i2c_master_write_byte(cmd, search_direction ? 0x80 : 0x00, ACK_CHECK_EN); //SS - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[DS2482_search_triplet()] - CMD_1WT = OK \n");
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_search_triplet()] - Parameter error (1) \n");
		case ESP_FAIL:
			printf("[DS2482_search_triplet()] - Sending command error, slave doesn`t ACK the transfer \n");
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_search_triplet()] - i2c driver not installed or not in master mode \n");
		case ESP_ERR_TIMEOUT:
			printf("[DS2482_search_triplet()] - Operation timeout because the bus is busy \n");
		default:
			printf("[DS2482_search_triplet()] - default block");
	}
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	do
	{
		i2c_master_read_byte(cmd, st, ACK_VAL); //[Status] A
	}
	while ((*st & STATUS_1WB) && (poll_count++ < POLL_LIMIT)); //Repeat until 1WB bit has changed to 0
	i2c_master_read_byte(cmd, st, NACK_VAL); //[Status] notA
	i2c_master_stop(cmd); // P
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[DS2482_search_triplet()] - *st = %d \n", *st);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[DS2482_search_triplet()] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[DS2482_search_triplet()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[DS2482_search_triplet()] - i2c driver not installed or not in master mode \n");
			break;
			printf("[DS2482_search_triplet()] - Operation timeout because the bus is busy \n");
		default:
			printf("[DS2482_search_triplet()] - default block");
	}
	
	if(poll_count >= POLL_LIMIT)
	{
		printf("[DS2482_search_triplet()] - POLL_LIMIT");
		DS2482_reset();
		return 0;
	}
	return status;
}











uint8_t OWSearch(uint8_t *ld, uint8_t *lfd, uint8_t *ldf) //, uint8_t *ROM_NO
{
	int id_bit; // 
	int cmp_id_bit; // the complement of the id_bit. This bit is AND of the complement of all of the id_bit_number
					// bits of the devices that are still participating in the search. 
	
	uint8_t search_direction; // бит, указывающий направление поиска.
							  // Все устройства с этим битом остаются в поиске, а остальные переходят в состояние ожидания для 1-проводного сброса.

	uint8_t status;
	
	// initialize for search
	int id_bit_number = 1; // бит ROM с номером 1 - 64, который в настоящее время выполняется.
	int last_zero = 0; // разрядная позиция последнего нуля, записанная там, где имело место несоответствие
	int rom_byte_number = 0; // 
	uint8_t search_result = 0; //False or True
	uint8_t rom_byte_mask = 1;
	
	crc8 = 0;
	
	//printf("address memory ROM_NO = %p\n", ROM_NO);
	//printf("*ld value = %d\n", *ld);
	//printf("*lfd value = %d\n", *lfd);
	//printf("*ldf value = %d\n", *ldf);
	
	// if the last call was not the last one 
	if (!*ldf)
	{
		// 1-wire reset
		if (!OWReset())
		{
			// reset the search
			*ld = 0;
			*ldf = 0;
			*lfd = 0;
			return 0;
			
		}
		vTaskDelay(250 / portTICK_RATE_MS);
		// issue the search command
		// выдать команду поиска
		OWWriteByte(SearchROM); // 0xF0
		
		// loop to do the search
		// Цикл поиска
		do
		{
			if(id_bit_number < *ld)
			{
				if((ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
					search_direction = 1;
				else
					search_direction = 0;
			}
			else
			{
				// if equal to last pick 1, if not then pick 0
				if(id_bit_number == *ld)
					search_direction = 1;
				else
					search_direction = 0;
			}
			
			
			// Perform a triple operation on the ds2482 which will perform
			// 2 read bits and 1 write bit
			status = DS2482_search_triplet(search_direction);
			
			//check bit results in status byte
			id_bit = ((status & STATUS_SBR) == STATUS_SBR);
			cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
			search_direction = ((status & STATUS_DIR) == STATUS_DIR) ? (uint8_t)1 : (uint8_t)0;
			
			// check for no devices on 1-wire
			if((id_bit) && (cmp_id_bit))
				break;
			else
			{
				if((!id_bit) && (!cmp_id_bit) && (search_direction == 0))
				{
					last_zero = id_bit_number;
					//printf("id_bit_number = %d\n", id_bit_number);
					
					// check for last discrepancy in family
					if(last_zero < 9)
						*lfd = last_zero;
				}
				
				// set or clear  the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if(search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;
				
				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;
				
				// if the mask is 0 then go to new SerialNum byte rom_byte_number
				// and reset mask
				if (rom_byte_mask == 0)
				{
					calc_crc8(ROM_NO[rom_byte_number]); // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while(rom_byte_number < 8);  // loop until through all ROM byte 0-7
		
		// if the search was successful then
		if(!((id_bit_number < 65) || (crc8 != 0)))
		{
			// search successful so set LastDiscrepancy, LastDeviceFlag
			// search_result
			//printf("last_zero = %d\n", last_zero);
			*ld = last_zero;
			
			//check for last device
			if(*ld == 0)
				*ldf = 1;
			
			search_result = 1;
		}	
	}
	
	// if no device found then reset counters so next
	// 'search' will be like a first
	
	if(!search_result || (ROM_NO[0] == 0))
	{
		*ld = 0;
		*ldf = 0;
		*lfd = 0;
		search_result = 0;
	}
	
	return search_result; //False or True
} 



