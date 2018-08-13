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


uint8_t read_statusDS2482(void)
{
	uint8_t reg_status = 0;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);  //S
	i2c_master_write_byte(cmd, DS2482_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, &reg_status, NACK_VAL); //[Status] notA
	i2c_master_stop(cmd); // P
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000/portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	switch(ret){
		case ESP_OK:
			//printf("[read_statusDS2482()] - reg_status = %d \n", reg_status);
			break;
		case ESP_ERR_INVALID_ARG:
			printf("[read_statusDS2482()] - Parameter error (2) \n");
			break;
		case ESP_FAIL:
			printf("[read_statusDS2482()] - Sending command error, slave doesn`t ACK the transfer \n");
			break;
		case ESP_ERR_INVALID_STATE:
			printf("[read_statusDS2482()] - i2c driver not installed or not in master mode \n");
			break;
			printf("[read_statusDS2482()] - Operation timeout because the bus is busy \n");
		default:
			printf("[read_statusDS2482()] - default block");
	}
	return reg_status;
}