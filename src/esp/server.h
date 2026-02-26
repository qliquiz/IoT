#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include "config.h"

inline ESP8266WebServer server(80);

inline void handleRoot()
{
	server.send(200,
	            "text/html",
	            R"(<form action="/LED" method="POST"><input type="submit" value="Toggle LED"></form>)");
}

inline void handleLED()
{
	digitalWrite(led, !digitalRead(led));
	server.sendHeader("Location", "/"); // redirection to keep button on the screen
	server.send(303);
}

inline void handleSENSOR()
{
	const int data = analogRead(A0);
	//server.sendHeader("Location","/");
	server.send(200, "text/html", String(data));
}

inline void handleNotFound() { server.send(404, "text/plain", "404: Not found"); }

inline void init_server()
{
	server.on("/", HTTP_GET, handleRoot);
	server.on("/LED", HTTP_POST, handleLED);
	server.on("/SENSOR", HTTP_GET, handleSENSOR);
	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("HTTP server started");
}
