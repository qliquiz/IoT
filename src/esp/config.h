#pragma once

constexpr bool WIFI_MODE_CLIENT = true;
constexpr bool WIFI_MODE_AP = false;

const String SSID_AP = "esp8266";
const String PASSWORD_AP = "12345678";
const String SSID_CLI = "enet";
const String SSID_PASSWORD = "YOUR_PASSWORD";

constexpr int led = LED_BUILTIN;
constexpr int mqtt_port = 1883;
inline char *mqtt_broker = "broker.emqx.io";
