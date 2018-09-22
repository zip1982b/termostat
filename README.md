Project name: Termostat.
This project is based on Espressif IoT Development Framework (esp-idf).

autor: Zhan Beshchanov<zip1982b@gmail.com>


![alt text](img/oled128x64_1relay.jpg "my first termostat :)")
Hardware:
* esp32
* OLED ssd1306 (128x64) slave 1 (task - vDisplay)
* ds2482-100 slave 2 (task - vRegulator)
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

The I2C APIs are not thread-safe(esp-idf doc).

It was originally intended to use one i2c master, but as a result, to achieve parallel use of the master (two tasks) failed.
Perhaps my experience in programming is not enough.
I tried to do one common task for communication on the i2c bus and transfer the data for the slaves through it, but I could not achieve stable work.

I had to use two ports (i2c masters) to communicate with slaves. At the moment the connection is reliable.

