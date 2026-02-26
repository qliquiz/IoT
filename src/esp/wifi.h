#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WString.h>

#include "config.h"

inline ESP8266WiFiMulti wifiMulti;

inline String id()
{
	uint8_t mac[WL_MAC_ADDR_LENGTH];
	WiFi.softAPmacAddress(mac);

	auto mac_id = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX);
	mac_id += String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);

	return mac_id;
}

inline void start_AP_mode()
{
	const IPAddress AP_IP(192, 168, 4, 1);

	Serial.println("Attempting to connect to WiFi AP");

	WiFi.disconnect();
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(SSID_AP + "_" + id(), PASSWORD_AP);

	Serial.println("WiFi AP is up, look at " + SSID_AP + "_" + id());
}

inline void start_client_mode()
{
	Serial.println("Attempting to connect client to ssid:" + SSID_AP + "_" + id());
	wifiMulti.addAP(SSID_CLI.c_str(), SSID_PASSWORD.c_str());
	while (wifiMulti.run() != WL_CONNECTED) { Serial.print("Successfuly connected to Router"); }
}

inline void init_WIFI(const bool mode)
{
	if (mode == WIFI_MODE_CLIENT)
	{
		start_client_mode();
		const String ip = WiFi.localIP().toString();
		Serial.println("My IP-address is: " + ip);
	} else { start_AP_mode(); }
}
