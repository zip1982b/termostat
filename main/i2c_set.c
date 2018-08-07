#include "i2c_set.h"

void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);
}

/* 
* S - 	Start condition
* AD,0 - Select DS2482-100 for Write Access
* A - Acknowledged
* A\ - NOT Acknowledged
* [] -slave(ds2482) answer
* P -Stop condition
 */
 
uint8_t write_command(uint8_t command)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN); //AD,0  - [A]
	i2c_master_write_byte(cmd, command, ACK_CHECK_EN); //command - [A]
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[write_command()] - OK\n");
			return 1;
		case ESP_ERR_INVALID_ARG:
			printf("[write_command()] - Parameter error \n");
			return 0;
		case ESP_FAIL:
			printf("[write_command()] - Sending command error, slave doesn`t ACK the transfer \n");
			return 0;
		case ESP_ERR_INVALID_STATE:
			printf("[write_command()] - i2c driver not installed or not in master mode \n");
			return 0;
		case ESP_ERR_TIMEOUT:
			printf("[write_command()] - Operation timeout because the bus is busy \n");
			return 0;
		default:
			printf("[write_command()] - default block");
			return 0;
	}
}




/* 
* S - 	Start condition
* AD,1 - Select DS2482-100 for Read Access  
* A - Acknowledged
* A\ - NOT Acknowledged
* [] -slave(ds2482) answer
* P -Stop condition
 */

uint8_t read_registerDS2482(void)
{
	uint8_t reg = 0;
	
	i2c_cmd_handle_t cmd = i2c_cmd_link_create(); 
	i2c_master_start(cmd); //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN); //AD,1  - [A]
	i2c_master_read_byte(cmd, &reg, NACK_VAL); //[reg] /A
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			printf("[read_reg] - reg = %d\n", reg);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[read_reg] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[OWReset()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[read_reg] - i2c driver not installed or not in master mode \n");
			break;
		case ESP_ERR_TIMEOUT:
			printf("[read_reg] - Operation timeout because the bus is busy \n");
			break;
		default:
			printf("[read_reg] - default block");
	}
	return reg;	
}


	