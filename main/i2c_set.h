#ifndef i2c_set_H
#define i2c_set_H

#include "driver/i2c.h"

/*** i2c settings ***/
#define I2C_MASTER_SCL_IO          19               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          18               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM             I2C_NUM_1        /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE  0                /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ         100000           /*!< I2C master clock frequency */

#define WRITE_BIT			I2C_MASTER_WRITE 		/* I2C Master write */
#define READ_BIT			I2C_MASTER_READ  		/* I2C_MASTER_READ */
#define ACK_CHECK_EN		0x1				 		/* I2C master will check ack from slave */
#define ACK_CHECK_DIS		0x0				 		/* I2C master will not check ack from slave */
#define ACK_VAL				0x0				 		/* I2C ack value */
#define NACK_VAL			0x1				 		/* I2C nack value */


#define DS2482_ADDR			0x18

void i2c_master_init();


uint8_t read_statusDS2482(void);


#endif