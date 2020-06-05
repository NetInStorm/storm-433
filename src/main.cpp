#include <Arduino.h> // I mean... I don't to write everything from scratch ;(

/* Radio Stuff */
#include <RH_NRF905.h>
#include <RHMesh.h>

/* Wi-Fi and WebUI Stuff */
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"

/* TODO: Random generation */
const char *ssid = "InStorm-XFGO4K";
const char *password = "demo_password";

AsyncWebServer server(80); // Setting up API Web-server

RH_NRF905 nrf905; // Initializing NRF905 Radio Driver
RHMesh mesh(nrf905, 20); // ADDR fix

IPAddress local_IP(192,168,1,1); // 192.168.1.x, cuz 10.x.x.x would be later used in modern versions in routing.
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

void setup() {
  delay(1000); // Pre-start delay
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin(); // probably need to replace it with LittleFS, but esp8266 support for it isn't YET rly ready... so... nvm.

  String nrf_state = String("PRE_INIT");
 
  /* Setting up AP */
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed!");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP is: ");
  Serial.println(myIP);

  /* Setting up HTTP server */
  
  server.on("/random", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(random(1000))); // Just testing programmability of a server. DELETE later. TODO
  });

  server.on("/api/v1/send", HTTP_POST, [](AsyncWebServerRequest *request){
    uint8_t addr;
    u_char buf[RH_MESH_MAX_MESSAGE_LEN];
    
    if (request->hasParam("addr", true) && request->hasParam("body", true)) {
        addr = request->getParam("addr", true)->value().toInt();
        request->getParam("body", true)->value().getBytes(buf, RH_MESH_MAX_MESSAGE_LEN);
        if (mesh.sendtoWait(buf, sizeof(buf), addr) == RH_ROUTER_ERROR_NONE) {
          request->send(500, "text/plain", "fail");
        } else {
          request->send(200, "text/plain", "success");
        }
    } else {
       request->send(500, "text/plain", "fail");
    };
  });

  server.on("/api/v1/poll", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(random(1000))); // Just testing programmability of a server. DELETE later. TODO
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html"); // Serving main Web-UI

  server.begin();
  Serial.println("Server started.");

  /* Setting up NRF905 Logic */
  Serial.print("Setting nrf905 ... ");
  if (nrf905.init() && mesh.init()) {
    Serial.println("Ready");
    nrf_state = "INIT";
  } else {
    Serial.println("Failed!\n");
    Serial.println("Resetting now!");
  }

  nrf905.setRF(RH_NRF905::TransmitPower10dBm); // Setting up power to max, cuz: "why not?"
}

void loop() {
  uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from;

  if (mesh.recvfromAck(buf, &len, &from))
  {
    Serial.print("got request from : 0x");
    Serial.print(from, HEX);
    Serial.print(": ");
    Serial.println((char*)buf);
  }
}