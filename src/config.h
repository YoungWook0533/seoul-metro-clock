#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Persistent configuration. Mirrors the stock /config JSON keys so the existing
// web UI (and expectations) line up. Stored at /config.json in LittleFS.
struct Config {
  String ssid;
  String password;
  String city        = "Sydney";
  String ntp         = "pool.ntp.org";
  // Open-Meteo works from coordinates. The city is geocoded once and the result
  // cached here; geoFor records which city string it belongs to.
  float  lat = NAN, lon = NAN;
  String geoName, geoFor;
  int    timezoneMin = 600;          // minutes offset (stock stores minutes)
  // POSIX TZ rule, so daylight saving switches automatically. Takes precedence over
  // timezoneMin; empty means "fixed timezoneMin offset" (stock behaviour).
  String tz          = "AEST-10AEDT,M10.1.0,M4.1.0/3";   // Sydney/Melbourne/Canberra
  bool   celsius     = true;
  bool   hour12      = true;
  bool   mile        = false;
  int    theme       = 0;            // 0=Classic 1=Weather 2=Photo 3=Dial 4=Simple 5=Forecast 6=Flip
  int    themeInterval = 10;         // seconds; 0 = no rotation
  int    faceMask    = 0x7f;         // bit n set = face n takes part in rotation
  int    brightness  = 60;           // 0..100
  bool   nightMode   = true;
  bool   nightAuto   = false;          // dim from sunset to sunrise instead of fixed hours
  int    nightBrightness = 15;       // 0..100
  int    startHour   = 23;           // night start
  int    stopHour    = 7;            // night end
  uint16_t color1 = 0xFD44;          // RGB565 accents: time and hands (amber #FFA826)
  uint16_t color2 = 0x6DBF;          // date and highlights (soft blue #6AB7FF)
  uint16_t color3 = 0x4ED4;          // weather (mint #4CD9A0)
  // Photos: seconds per photo, and names left out of the slideshow ("|a.jpg|b.jpg|").
  int    photoInterval = 60;
  bool   photoClock = true;          // the time pill on the Photo face
  String photoOff;
  String customFace;                 // the custom face shown when theme is FACE_CUSTOM
  // Integrations, all off by default (settings page, Integrations card).
  bool   facesApi = false;           // custom faces over HTTP (and MQTT when that is on)
  bool   haCompat = false;           // stock SD Pro photo API for geekmagic-hacs; no sign-in
  bool   mqttOn   = false;           // MQTT with Home Assistant discovery
  String mqttHost, mqttUser, mqttPass, mqttBase;   // mqttBase "" = the hostname
  int    mqttPort = 1883;
  String apiKey;                     // for scripts calling the faces API; never exported

  bool load() {
    if (!LittleFS.exists("/config.json")) return false;
    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    ssid        = doc["ssid"]        | ssid;
    password    = doc["password"]    | password;
    ntp         = doc["ntp"]         | ntp;
    timezoneMin = doc["timezone"]    | timezoneMin;
    tz          = doc["tz"]          | tz;
    brightness  = doc["brightness"]  | brightness;
    nightMode   = doc["nightmode"]   | nightMode;
    nightAuto   = doc["nightauto"]   | nightAuto;
    nightBrightness = doc["nightbrightness"] | nightBrightness;
    startHour   = doc["starttime"]   | startHour;
    stopHour    = doc["stoptime"]    | stopHour;
    return true;
  }

  bool save() {
    JsonDocument doc;
    doc["ssid"] = ssid;           doc["password"] = password;
    doc["ntp"] = ntp;
    doc["timezone"] = timezoneMin;
    doc["tz"] = tz;
    doc["brightness"] = brightness;
    doc["nightmode"] = nightMode; doc["nightauto"] = nightAuto; doc["nightbrightness"] = nightBrightness;
    doc["starttime"] = startHour; doc["stoptime"] = stopHour;
    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
  }
};

extern Config cfg;   // defined in main.cpp
