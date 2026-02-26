#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "config.h"

inline WiFiClient wifiClient;
inline PubSubClient mqtt_cli(wifiClient);

inline void callback(const char *topic, const byte *payload, const unsigned int length)
{
	Serial.print("Message arrived in topic: ");
	Serial.println(topic);
	Serial.print("Message:");
	for (int i = 0; i < length; i++) { Serial.print(static_cast<char>(payload[i])); }
	Serial.println("\n-----------------------\n");
}

inline void init_MQTT()
{
	mqtt_cli.setServer(mqtt_broker, mqtt_port);
	mqtt_cli.setCallback(callback);
	while (!mqtt_cli.connected())
	{
		const String client_id = "esp8266-" + String(WiFi.macAddress());
		Serial.println("The client " + client_id + "connects to the public mqtt-broker\n");
		if (mqtt_cli.connect(client_id.c_str())) { Serial.println("MQTT Connected"); } else
		{
			Serial.print("failed with state ");
			Serial.println(mqtt_cli.state());
			delay(2000);
		}
	}
}
