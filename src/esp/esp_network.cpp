#include <HardwareSerial.h>

#include "mqtt.h"
#include "server.h"
#include "wifi.h"

void setup()
{
	pinMode(led, OUTPUT);
	Serial.begin(115200);
	init_WIFI(WIFI_MODE_AP);
	init_server();
	init_MQTT();
	mqtt_cli.subscribe("esp8266/command");
}

void loop()
{
	server.handleClient();
	mqtt_cli.loop();
	// delay(500);
	// Serial.print("Out id is:");
	// Serial.println(id());
}
