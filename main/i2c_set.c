#include "i2c_set.h"

void i2c_master_init(int I2C_MASTER_NUM, gpio_num_t I2C_MASTER_SDA_IO, gpio_num_t I2C_MASTER_SCL_IO, uint32_t I2C_MASTER_FREQ_HZ, size_t I2C_MASTER_RX_BUF, size_t I2C_MASTER_TX_BUF)
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
                       I2C_MASTER_RX_BUF,
                       I2C_MASTER_TX_BUF, 0);
}


