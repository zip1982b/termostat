#ifndef i2c_set_H
#define i2c_set_H

#include "driver/i2c.h"

/*** i2c settings for DS2482-100 1-wire net***/
#define I2C_MASTER_SCL_DS2482          	19               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_DS2482          	18               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM_DS2482          	I2C_NUM_1        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DS2482		0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DS2482		0                /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ_DS2482		100000           /*!< I2C master clock frequency */


/*** i2c settings for SSD1306 OLED Display***/
#define I2C_MASTER_SCL_SSD1306			26               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_SSD1306          25               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM_SSD1306			I2C_NUM_0        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_SSD1306		0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_SSD1306		0                /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ_SSD1306		400000           /*!< I2C master clock frequency */

#define WRITE_BIT			I2C_MASTER_WRITE 		/* I2C Master write */
#define READ_BIT			I2C_MASTER_READ  		/* I2C_MASTER_READ */
#define ACK_CHECK_EN		0x1				 		/* I2C master will check ack from slave */
#define ACK_CHECK_DIS		0x0				 		/* I2C master will not check ack from slave */
#define ACK_VAL				0x0				 		/* I2C ack value */
#define NACK_VAL			0x1				 		/* I2C nack value */




void i2c_master_init(int I2C_MASTER_NUM, gpio_num_t I2C_MASTER_SDA_IO, gpio_num_t I2C_MASTER_SCL_IO, uint32_t I2C_MASTER_FREQ_HZ, size_t I2C_MASTER_RX_BUF, size_t I2C_MASTER_TX_BUF);






#endif