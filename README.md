To install on Wemos D1 mini:

# Install arduino >=1.8.8:
Note the version in the Ubuntu repositories is outdated, use the version
directly from the website.

# Add ESP boards via arduino ide:
In the arduino ide, open file->preferences
in Additional Boards Manager URLs: paste the following link:
http://arduino.esp8266.com/stable/package_esp8266com_index.json

Open Tools->'Boards Manager' and install ESP8266 using the search box.

# Add arduino libraries via arduino ide:
The OneWire and DallasTemperatureSensor libraries are required, both can
be installed using the Tools->'Manage Libraries' menu of the Arduino IDE,
using the search box.

# Programming WiFi access point into the device
To program a WiFi access SSID and Password into the device, hold the button
while resetting the device, the serial monitor should show 'waiting for credentials'
and the LED should light up. Send the credentials in the following format:
wifi-[SSID]-[Password]

# Setting up the server
The server logs messages in the database, the database file must be created using create_db.py

store_vals.py must run continuously to log messages, consider adding a line to start it in

/etc/init.d/rc.local, an example file may be:

#!/bin/sh
/home/pi/store_vals.py & >> output.log

# Registering sensors
Before sensors values are stored in the database, the sensors must be registered in the database,
messages from unregistered sensors are kept in a special table of the database. After a sensor has
began sending messages to the server, run the manage_database.py script, it is safest to kill the
store_vals.py script first, so there are not concurrent programs accessing the database. Sensors
must belong to a 'property', so if the property the sensor belongs to has not been created that must
be done first.
