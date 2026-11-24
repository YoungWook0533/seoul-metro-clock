#include "weather.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

Weather wx;
Forecast fc;

bool fetchWeather() {
  wx.lastFetch = millis();
  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(4000);
  if (!http.begin(client,"http://api.open-meteo.com/v1/forecast?latitude=37.5665&longitude=126.9780&current=temperature_2m,weather_code,is_day&timezone=Asia%2FSeoul")) return false;
  if (http.GET() != 200) { wx.error = "Weather unavailable"; http.end(); return false; }
  JsonDocument doc;
  auto error = deserializeJson(doc,http.getStream());
  http.end();
  if (error || !doc["current"]["temperature_2m"].is<float>()) { wx.error = "Weather data error"; return false; }
  wx.temp = doc["current"]["temperature_2m"].as<float>();
  int code = doc["current"]["weather_code"] | 3;
  const char* icon = "04";
  if (code == 0) icon = "01";
  else if (code == 1) icon = "02";
  else if (code == 2) icon = "03";
  else if (code == 45 || code == 48) icon = "50";
  else if (code >= 95) icon = "11";
  else if ((code >= 71 && code <= 77) || code == 85 || code == 86) icon = "13";
  else if (code >= 51) icon = "10";
  wx.icon = String(icon) + ((doc["current"]["is_day"] | 1) ? "d" : "n");
  wx.cityName = "Seoul";
  wx.valid = true; wx.lastSuccess = millis(); ++wx.version; wx.error = "";
  return true;
}
