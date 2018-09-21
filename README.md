Project name: Termostat.
This project is based on Espressif IoT Development Framework (esp-idf).

autor: Zhan Beshchanov<zip1982b@gmail.com>


![alt text](img/oled128x64_1relay.jpg "my first termostat :)")
Hardware:
* esp32
* OLED ssd1306 (128x64)
* Rotary encoder
* 1 Relay NO


Connections:
* Relay - GPIO12 (len on = relay on, led off = relay off)
* encoder clk - GPIO35, DT - GPIO32, SW - GPIO33
* SSD1306 VCC - 3.3v 
* SSD1306 GND

 two i2c mater
* DS2482 SCL - GPIO19
* DS2482 SDA - GPIO18

* SCL_SSD1306 - GPIO26
* SDA_SSD1306 - GPIO25

