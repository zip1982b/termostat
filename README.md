Project name: Termostat.
This project is based on Espressif IoT Development Framework (esp-idf).

autor: Zhan Beshchanov<zip1982b@gmail.com>


![alt text](img/oled128x64.jpg "my first termostat :)")
Hardware:
* esp32
* OLED ssd1306 (128x64)
* Rotary encoder
* 2 Relay NO


Connections:
* Relay 1,2 - GPIO14, GPIO12
* encoder clk - GPIO33, DT - GPIO25, SW - GPIO26
* SSD1306 VCC - 3.3v 
* SSD1306 GND
* SSD1306 SCL - GPIO19
* SSD1306 SDA - GPIO18
