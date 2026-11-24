#include "metro.h"
#include "config.h"
#include "metro_web.h"
#include "metro_estimate.h"
#include "weather.h"
#include "arex_ca.h"
#include "metro_arrival_glyphs.h"
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TFT_eSPI.h>
#include <algorithm>

extern ESP8266WebServer server;
extern TFT_eSPI& clockDisplay();
bool requireAuth();
extern bool brightnessDirty;
void applyTime();

namespace {
constexpr int MAX_TRAINS=8;
constexpr int FAVORITE_COUNT = 5;
constexpr size_t LABEL_BYTES = 7200;
int activeFavorite = -1;
int baseLabelSlot = 0;
JsonDocument settings;
bool dirty = true;
bool koreanDisplay = true;
uint32_t nextFetch = 0;
uint8_t operatorFailures = 0;
time_t lastDraw = 0, received = 0;
String state = "WAITING";
struct Train { String number, order, station; int eta = -1, code = 99, operatorMinutes = -1, distance = -1, gaugeStart = 30, gaugeEta = 1; time_t stamp = 0, expires = 0; bool estimated = false, operatorTime = false; } trains[MAX_TRAINS];
Train work[MAX_TRAINS];
int trainCount = 0;
void insertTrain(Train* list,int& count,const Train& train,bool etaOrder) {
  int at=0;
  for(int i=0;i<count;i++) if(list[i].number==train.number) return;
  while(at<count && (etaOrder?list[at].eta<=train.eta:list[at].order<train.order)) ++at;
  if(at>=MAX_TRAINS) return;
  count=std::min(MAX_TRAINS,count+1);
  for(int i=count-1;i>at;i--) list[i]=list[i-1];
  list[at]=train;
}
int boardingRemaining(const Train& train,time_t now) {
  return now<=train.expires && train.eta>=0?std::max(0,train.eta-int(now-received)):-1;
}
int quotaDay = 0, reserved = 0, available = 0;
time_t sourceTime(const String& text);

int routeIndex(String name) {
  if (name == "서울") name = "서울역";
  JsonArray route = settings["route"].as<JsonArray>();
  for (size_t i = 0; i < route.size(); ++i)
    if (route[i].as<String>() == name) return i;
  return -1;
}

bool saveJson(const char* path, JsonDocument& doc) {
  String temporary = String(path) + ".tmp";
  File file = LittleFS.open(temporary, "w");
  if (!file) return false;
  size_t written = serializeJson(doc, file);
  file.close();
  if (written != measureJson(doc)) return false;
  return LittleFS.rename(temporary, path);
}

void defaults() {
  settings["lineId"] = "1065";
  settings["lineName"] = "공항철도";
  settings["station"] = "마곡나루";
  settings["updn"] = "상행";
  settings["toward"] = "김포공항 방면";
  settings["direction"] = 1;
  settings["timezone"] = 540;
  settings["ntp"] = "pool.ntp.org";
  settings["brightness"] = 60;
  settings["peakSeconds"] = 20;
  settings["normalSeconds"] = 180;
  settings["stationDwellSeconds"] = 30;
  const char* stations[] = {"서울역","공덕","홍대입구","디지털미디어시티","마곡나루","김포공항","계양","검암","청라국제도시","영종","운서","공항화물청사","인천공항1터미널","인천공항2터미널"};
  for (auto station : stations) settings["route"].add(station);
}

unsigned pollSeconds(bool approaching, const tm& local) {
  if (settings["lineId"].as<String>() == "1065") return 20;
  unsigned interval = settings[approaching ? "peakSeconds" : "normalSeconds"].as<unsigned>();
  int minute = local.tm_hour*60+local.tm_min;
  int elapsed = minute < 30 ? minute : std::max(30,minute-270);
  int used = reserved-available;
  if (used > 900*elapsed/1170+40) interval = std::max(interval, approaching ? 45U : 240U);
  return interval;
}

void fetchOperator(time_t now) {
  static const char* codes[] = {"010","020","030","040","049","050","060","070","075","077","080","090","100","110"};
  int canonical = routeIndex(settings["station"].as<String>());
  String stationCode = settings["stationCodes"][routeIndex(settings["station"].as<String>())] | "";
  if (stationCode.length() != 3 && (canonical < 0 || canonical >= 14)) { state = "STATION ERROR"; return; }
  // Schedule backoff before attempting I/O; errors must never cause a tight loop.
  operatorFailures = std::min(5,int(operatorFailures)+1);
  nextFetch = millis() + std::min(600U,20U << operatorFailures)*1000;
  BearSSL::X509List anchors(AREX_ROOT_CA,sizeof(AREX_ROOT_CA));
  BearSSL::WiFiClientSecure client;
  client.setTrustAnchors(&anchors);
  client.setBufferSizes(16384,512);
  client.setTimeout(4000);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(4000);
  if (stationCode.length() != 3) stationCode = codes[canonical];
  String url = String("https://www.airportrailroad.com/train/normal/live/realTimeStation?startStation=")+stationCode;
  if (!http.begin(client,url)) { state = "AREX CONNECT ERROR"; return; }
  int response = http.GET();
  if (response != 200) {
    if (response == 403 || response == 429) nextFetch = millis()+1800000UL;
    state = String("AREX HTTPS ")+response+" / "+client.getLastSSLError();
    http.end(); return;
  }
  int direction = settings["direction"] | 1;
  const char* list = direction > 0 ? "realTimeStationDown" : "realTimeStationUp";
  JsonDocument filter, reply;
  for (const char* field : {"formType","trnNo","waitingTime","waitingStn"}) filter[list][0][field] = true;
  auto error = deserializeJson(reply,http.getStream(),DeserializationOption::Filter(filter));
  http.end();
  if (error || !reply[list].is<JsonArray>()) { state = String("AREX DATA ")+error.c_str(); return; }
  now = time(nullptr);
  Train* candidates=work; for(auto& train:work) train=Train();
  int count = 0;
  int target = routeIndex(settings["station"].as<String>());
  for (JsonObject record : reply[list].as<JsonArray>()) {
    if (record["formType"].as<String>() != "B" || record["waitingTime"].isNull()) continue;
    String waiting = record["waitingTime"].as<String>();
    bool numeric = waiting.length() > 0;
    for (char c : waiting) if (!isdigit(c)) numeric = false;
    if (!numeric) continue;
    Train train;
    train.number = record["trnNo"].as<String>();
    train.operatorMinutes = waiting.toInt();
    train.eta = train.operatorMinutes*60;
    train.operatorTime = true; train.stamp = now; train.expires = now+75;
    int distance = record["waitingStn"].as<int>();
    train.distance = distance;
    train.gaugeStart = 210-60*std::min(3,std::max(1,distance));
    train.gaugeEta = std::max(1,train.eta);
    for (int i=0;i<trainCount;i++) if (trains[i].number == train.number && trains[i].distance == distance) {
      train.gaugeStart = trains[i].gaugeStart;
      train.gaugeEta = trains[i].gaugeEta;
      if (trains[i].operatorMinutes == train.operatorMinutes)
        train.eta = std::max(0,trains[i].eta-int(now-received));
    }
    int station = target-distance*direction;
    if (distance > 0 && station >= 0 && station < int(settings["route"].size())) train.station = settings["route"][station].as<String>();
    else train.station = "출발 전";
    insertTrain(candidates,count,train,true);
  }
  trainCount = count; std::copy(candidates,candidates+count,trains); for(auto& train:work) train=Train();
  received = now; state = count ? "AREX LIVE" : "NO DATA";
  operatorFailures = 0;
  nextFetch = millis()+20000UL;
}

String encoded(const String& input) {
  String output;
  const char* digits = "0123456789ABCDEF";
  for (unsigned char value : input) {
    if (isalnum(value)) output += char(value);
    else { output += '%'; output += digits[value >> 4]; output += digits[value & 15]; }
  }
  return output;
}

bool takeQuota(const tm& local) {
  int day = (local.tm_year + 1900) * 1000 + local.tm_yday;
  if (day != quotaDay) { quotaDay = day; reserved = available = 0; }
  if (!available) {
    if (reserved >= 900) return false;
    JsonDocument budget;
    budget["day"] = quotaDay;
    budget["reserved"] = reserved + 25;
    // Reserve before a network call. A restart forfeits unused calls in this block.
    if (!saveJson("/metro-budget.json", budget)) return false;
    reserved += 25;
    available = 25;
  }
  --available;
  return true;
}

time_t sourceTime(const String& text) {
  tm parsed = {};
  if (text.length() != 19 || text[4] != '-' || text[7] != '-' || text[13] != ':') return 0;
  parsed.tm_year = text.substring(0,4).toInt() - 1900;
  parsed.tm_mon = text.substring(5,7).toInt() - 1;
  parsed.tm_mday = text.substring(8,10).toInt();
  parsed.tm_hour = text.substring(11,13).toInt();
  parsed.tm_min = text.substring(14,16).toInt();
  parsed.tm_sec = text.substring(17,19).toInt();
  parsed.tm_isdst = 0;
  return mktime(&parsed) - (540 - cfg.timezoneMin) * 60;
}

void fetchTrains(const tm& local, time_t now) {
  int minute = local.tm_hour * 60 + local.tm_min;
  bool approaching = trainCount && trains[0].eta >= 0 && now <= trains[0].expires &&
                     trains[0].eta - int(now-received) <= 90;
  uint32_t seconds = pollSeconds(approaching, local);
  nextFetch = millis() + seconds * 1000;
  if (minute >= 30 && minute < 300) { state = "NIGHT"; trainCount = 0; return; }
  if (settings["lineId"].as<String>() == "1065") { fetchOperator(now); return; }
  String key = settings["apiKey"] | "";
  if (key.isEmpty()) { state = "SET API KEY"; return; }
  if (!takeQuota(local)) { state = "DAILY LIMIT"; return; }
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(2500);
  http.useHTTP10(true);
  String url = "http://swopenapi.seoul.go.kr/api/subway/" + encoded(key) +
               "/json/realtimeStationArrival/0/30/" + encoded(settings["station"].as<String>());
  if (!http.begin(client, url)) { state = "CONNECT ERROR"; return; }
  if (http.GET() != 200) { state = "HTTP ERROR"; http.end(); return; }
  JsonDocument filter, reply;
  filter["errorMessage"]["code"] = true;
  const char* fields[] = {"subwayId","updnLine","btrainNo","ordkey","barvlDt","arvlCd","arvlMsg3","arvlMsg2","recptnDt","btrainSttus","trainLineNm"};
  const char* listName = "realtimeArrivalList";
  for (auto field : fields) filter[listName][0][field] = true;
  auto error = deserializeJson(reply, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (error) { state = "DATA ERROR"; return; }
  String code = reply["errorMessage"]["code"] | "";
  if (code != "INFO-000") { state = code.length() ? code : "API ERROR"; return; }
  Train* candidates=work; for(auto& train:work) train=Train();
  int count = 0;
  for (JsonObject record : reply[listName].as<JsonArray>()) {
    if (record["subwayId"].as<String>() != settings["lineId"].as<String>()) continue;
    time_t stamp = sourceTime(record["recptnDt"].as<String>());
    if (record["updnLine"].as<String>() != settings["updn"].as<String>()) continue;
    if (record["subwayId"].as<String>() == "1065" &&
        (record["btrainSttus"].as<String>() == "급행" || record["trainLineNm"].as<String>().indexOf("급행") >= 0)) continue;
    String next=settings["nextStation"]|"";
    if (next.length() && record["trainLineNm"].as<String>().indexOf(next+"방면")<0) continue;
    Train train;
    train.code = record["arvlCd"].as<int>();
    if (train.code == 2) continue;
    train.number = record["btrainNo"].as<String>();
    train.station = record["arvlMsg3"].as<String>();
    bool arex = record["subwayId"].as<String>() == "1065";
    if (!stamp || now - stamp > (arex ? 900 : 180) || stamp > now + 30) continue;
    train.stamp = stamp;
    train.expires = stamp + 180;
    int eta = record["barvlDt"].as<int>();
    // AREX frequently reports 0 even for trains several stations away.
    train.eta = eta > 0 ? std::max(0, eta - int(std::max(time_t(0), now - stamp))) : -1;
    train.order = record["ordkey"].as<String>();
    if (eta<=0) train.eta=(train.code==0 || train.code==1)?0:-1;
    int at=routeIndex(train.station),target=routeIndex(settings["station"].as<String>());
    train.distance=at>=0?abs(target-at):3;
    train.gaugeStart=210-60*std::min(3,std::max(1,train.distance));
    train.gaugeEta=std::max(1,train.eta);
    for(int i=0;i<trainCount;i++) if(trains[i].number==train.number && trains[i].station==train.station) {
      train.gaugeStart=trains[i].gaugeStart;train.gaugeEta=trains[i].gaugeEta;
    }
    insertTrain(candidates,count,train,false);
  }
  trainCount = count;
  std::copy(candidates,candidates+count,trains); for(auto& train:work) train=Train();
  received = now;
  state = count ? "LIVE" : "NO DATA";
  if (count && trains[0].eta <= 90) nextFetch = millis() + pollSeconds(true,local) * 1000;
}

String favoritePath(int slot) { return String("/metro-favorite-") + slot; }

File openLabels() {
  if (activeFavorite >= 0) return LittleFS.open(favoritePath(activeFavorite), "r");
  return LittleFS.open(String("/metro-labels-") + baseLabelSlot + ".bin", "r");
}

// Keep feed identifiers in Korean; romanize only when drawing English labels.
String latinName(const char* source) {
  static const char* const initial[] = {"g","kk","n","d","tt","r","m","b","pp","s","ss","","j","jj","ch","k","t","p","h"};
  static const char* const vowel[] = {"a","ae","ya","yae","eo","e","yeo","ye","o","wa","wae","oe","yo","u","wo","we","wi","yu","eu","ui","i"};
  static const char* const final[] = {"","k","k","ks","n","nj","nh","t","l","lk","lm","lb","ls","lt","lp","lh","m","p","ps","t","t","ng","t","t","k","t","p","h"};
  auto bytes = reinterpret_cast<const uint8_t*>(source);
  String result;
  while (*bytes && *bytes != '(') {
    if (*bytes < 128) { result += char(*bytes++); continue; }
    if ((*bytes & 0xf0) == 0xe0 && bytes[1] && bytes[2]) {
      int syllable = ((*bytes & 15) << 12 | (bytes[1] & 63) << 6 | (bytes[2] & 63)) - 0xac00;
      bytes += 3;
      if (syllable >= 0 && syllable < 11172) {
        result += initial[syllable / 588]; result += vowel[syllable / 28 % 21]; result += final[syllable % 28];
      }
    } else ++bytes;
  }
  if (result.length()) result[0] = toupper(static_cast<unsigned char>(result[0]));
  return result;
}

// Fit rendered pixels, since Romanized station names vary greatly in length.
void latinLabel(String text, int x, int y, int width, int rows, bool centered, uint16_t color) {
  auto& display = clockDisplay();
  display.setTextColor(color, TFT_BLACK);
  for (int row = 0; row < rows && text.length(); ++row) {
    String line = text;
    const bool last = row + 1 == rows;
    const bool clipped = display.textWidth(text, 2) > width;
    const int limit = width - (clipped && last ? display.textWidth("...", 2) : 0);
    while (display.textWidth(line, 2) > limit) line.remove(line.length() - 1);
    text.remove(0, line.length());
    if (clipped && last) line += "...";
    display.drawString(line, centered ? x - display.textWidth(line, 2) / 2 : x, y + 16 * row, 2);
  }
}

void drawEnglishLabels() {
  const char* station = settings["station"] | "";
  latinLabel(latinName(station), 10, 7, 138, 2, false, TFT_WHITE);
  String id = settings["lineId"] | "";
  String line = id == "1065" ? String("Airport Railroad") : latinName(settings["lineName"] | "");
  if (id >= "1001" && id <= "1009") line = String("Line ") + id.substring(3);
  latinLabel(line, 10, 40, 218, 1, false, 0xbdf7);
  int target = routeIndex(station);
  int direction = settings["direction"] | 1;
  JsonArray route = settings["route"].as<JsonArray>();
  for (int i = 0; i < 4; ++i) {
    int index = target - (3 - i) * direction;
    String label = target >= 0 && index >= 0 && index < int(route.size())
        ? latinName(route[index] | "") : String();
    latinLabel(label.length() ? label : String("--"), 30 + 60 * i, 97, 54, 2, true, TFT_WHITE);
  }
}

void drawLabels() {
  if (!koreanDisplay) { drawEnglishLabels(); return; }
  File file = openLabels();
  if (!file || file.size() < LABEL_BYTES) {
    clockDisplay().drawBitmap(10, 38, SETUP_STATION, 48, 12, TFT_WHITE);
    return;
  }
  uint8_t row[30];
  for (int y = 0; y < 240; ++y) {
    if (file.read(row, sizeof(row)) != sizeof(row)) break;
    // Stored labels predate the per-region layout. Reserve the upper right
    // for the time and keep all dynamic captions out of the stored bitmap.
    if (y < 55 || (y >= 98 && y < 124))
      clockDisplay().drawBitmap(0, y, row, y < 38 ? 152 : 240, 1, TFT_WHITE);
    yield();
  }
}

int position(const Train& train) {
  if (train.operatorTime && train.distance<=0) return 30;
  if (train.code==0 || train.code==1) return 210;
  int remaining=std::max(0,train.eta-int(time(nullptr)-received));
  if (train.eta<0) return train.gaugeStart;
  int end=210;
  return metroSynchronizedGauge(train.gaugeStart,end,train.gaugeEta,remaining);
}

bool validSettings(JsonDocument& value) {
  int walking=value["walkSeconds"]|0;
  if(walking<0 || walking>10800) return false;
  JsonArray route = value["route"].as<JsonArray>();
  int target = -1;
  if (route.size() < 3 || route.size() > 150) return false;
  JsonArray minutes = value["segmentMinutes"].as<JsonArray>();
  if (!minutes.isNull()) {
    if (minutes.size() + 1 != route.size()) return false;
    for (JsonVariant minute : minutes)
      if (!minute.is<float>() || minute.as<float>() <= 0 || minute.as<float>() > 60) return false;
  }
  for (size_t i = 0; i < route.size(); ++i) {
    if (!route[i].is<String>() || route[i].as<String>().length() > 80) return false;
    if (route[i].as<String>() == value["station"].as<String>()) target = i;
  }
  int direction = value["direction"] | 0;
  int previous = target - 2 * direction;
  String line = value["lineId"] | "";
  if (line.length() != 4) return false;
  int dwell = value["stationDwellSeconds"] | 30;
  if (dwell < 0 || dwell > 180) return false;
  for (char c : line) if (!isdigit(c)) return false;
  String updn = value["updn"] | "";
  return (updn == "상행" || updn == "하행" || updn == "내선" || updn == "외선") &&
    (direction == 1 || direction == -1) && target >= 0 && previous >= 0 && previous < int(route.size()) &&
    value["peakSeconds"].as<int>() >= 20 && value["peakSeconds"].as<int>() <= 600 &&
    value["normalSeconds"].as<int>() >= 120 && value["normalSeconds"].as<int>() <= 1800 &&
    value["timezone"].as<int>() >= -720 && value["timezone"].as<int>() <= 840 &&
    value["brightness"].as<int>() >= 1 && value["brightness"].as<int>() <= 100 &&
    value["ntp"].as<String>().length() > 0 && value["ntp"].as<String>().length() <= 80;
}
bool readFavorite(int slot, JsonDocument& value) {
  File file = LittleFS.open(favoritePath(slot), "r");
  return file && file.size() > LABEL_BYTES && file.seek(LABEL_BYTES, SeekSet) &&
         !deserializeJson(value, file);
}

bool saveFavorite(int slot) {
  File labels = openLabels();
  if (!labels || labels.size() < LABEL_BYTES) return false;

  // A favorite contains its bitmap and settings in one atomically replaced file.
  const char* temporary = "/metro-favorite.tmp";
  File file = LittleFS.open(temporary, "w");
  if (!file) return false;
  uint8_t buffer[240];
  for (size_t offset = 0; offset < LABEL_BYTES; offset += sizeof(buffer)) {
    if (labels.read(buffer, sizeof(buffer)) != sizeof(buffer) ||
        file.write(buffer, sizeof(buffer)) != sizeof(buffer)) return false;
    yield();
  }
  labels.close();
  JsonDocument copy = settings;
  copy.remove("apiKey");
  copy.remove("labelSlot");
  size_t written = serializeJson(copy, file);
  file.close();
  return written == measureJson(copy) && LittleFS.rename(temporary, favoritePath(slot));
}

void loadBase(JsonDocument& value) {
  File file = LittleFS.open("/metro.json", "r");
  if (!file || deserializeJson(value, file) || !validSettings(value)) value.clear();
}

void applySettings(JsonDocument&& value, int favorite) {
  bool timeChanged = cfg.timezoneMin != (value["timezone"] | 540) ||
                     cfg.ntp != value["ntp"].as<String>();
  settings = std::move(value);
  activeFavorite = favorite;
  cfg.tz = "";
  cfg.timezoneMin = settings["timezone"] | 540;
  cfg.ntp = settings["ntp"].as<String>();
  cfg.brightness = settings["brightness"] | 60;
  brightnessDirty = true;
  if (timeChanged) applyTime();
  trainCount = 0;
  for (auto& train : trains) train = Train();
  received = operatorFailures = nextFetch = lastDraw = 0;
  state = "WAITING";
  dirty = true;
}

bool activateFavorite(int slot) {
  JsonDocument value;
  if (!readFavorite(slot, value) || !validSettings(value)) return false;
  value["apiKey"] = settings["apiKey"];
  applySettings(std::move(value), slot);
  // Draw the new route before starting a potentially blocking network request.
  nextFetch = millis() + 200;
  return true;
}

void favoriteSummary(JsonObject result, const JsonDocument& value) {
  for (const char* key : {"station", "lineName", "toward", "updn", "walkSeconds"})
    result[key] = value[key];
}

} // namespace

void metroInvalidate() { dirty = true; }

void metroLoad() {
  File language = LittleFS.open("/metro-language", "r");
  koreanDisplay = !language || language.read() != 'e';
  language.close();
  defaults();
  JsonDocument saved;
  loadBase(saved);
  if (!saved.isNull()) settings = std::move(saved);
  baseLabelSlot = settings["labelSlot"] | 0;
  if ((settings["peakSeconds"] | 60) == 60 && (settings["normalSeconds"] | 120) == 120) {
    settings["peakSeconds"] = 20; settings["normalSeconds"] = 180;
  }
  if (settings["stationDwellSeconds"].isNull()) settings["stationDwellSeconds"] = 30;
  File budget = LittleFS.open("/metro-budget.json", "r");
  JsonDocument savedBudget;
  if (budget && !deserializeJson(savedBudget, budget)) {
    quotaDay = savedBudget["day"] | 0;
    reserved = savedBudget["reserved"] | 900;
  }
  cfg.theme = 0; cfg.themeInterval = 0;
  cfg.tz = ""; cfg.timezoneMin = settings["timezone"] | 540;
  cfg.ntp = settings["ntp"].as<String>();
  cfg.brightness = settings["brightness"] | 60;
  cfg.city = "Seoul, KR"; cfg.geoFor = cfg.city; cfg.geoName = "Seoul";
  cfg.lat = 37.5665f; cfg.lon = 126.9780f;
}

void metroNextFavorite() {
  for (int step = 1; step <= FAVORITE_COUNT; ++step) {
    int slot = (activeFavorite + step) % FAVORITE_COUNT;
    if (slot != activeFavorite && activateFavorite(slot)) return;
  }
}

void metroRoutes() {
  server.on("/api/metro/language", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain", koreanDisplay ? "ko" : "en");
  });
  server.on("/api/metro/language", HTTP_POST, [] {
    if (!requireAuth()) return;
    String language = server.arg("language");
    if (language != "en" && language != "ko") { server.send(400); return; }
    bool korean = language == "ko";
    if (korean != koreanDisplay) {
      File file = LittleFS.open("/metro-language.tmp", "w");
      if (!file || file.write(korean ? 'k' : 'e') != 1) { server.send(507); return; }
      file.close();
      if (!LittleFS.rename("/metro-language.tmp", "/metro-language")) { server.send(507); return; }
      koreanDisplay = korean;
      metroInvalidate();
    }
    server.send(204);
  });
  server.on("/api/metro/favorites", HTTP_GET, [] {
    if (!requireAuth()) return;
    JsonDocument result, favorite;
    result["active"] = activeFavorite + 1;
    favoriteSummary(result["current"].to<JsonObject>(), settings);
    JsonArray slots = result["slots"].to<JsonArray>();
    for (int slot = 0; slot < FAVORITE_COUNT; ++slot) {
      if (readFavorite(slot, favorite)) favoriteSummary(slots.add<JsonObject>(), favorite);
      else slots.add(nullptr);
    }
    String body;
    serializeJson(result, body);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", body);
  });
  server.on("/api/metro/favorites", HTTP_POST, [] {
    if (!requireAuth()) return;
    String slotArg = server.arg("slot");
    int slot = slotArg.toInt() - 1;
    if (slotArg.length() != 1 || slot < 0 || slot >= FAVORITE_COUNT) { server.send(400); return; }
    String action = server.arg("action");
    if (action == "save") {
      if (!saveFavorite(slot)) { server.send(409); return; }
      activeFavorite = slot;
    } else if (action == "use") {
      if (!activateFavorite(slot)) { server.send(404); return; }
    } else if (action == "delete") {
      JsonDocument base;
      if (slot == activeFavorite) {
        loadBase(base);
        if (base.isNull()) { server.send(409); return; }
      }
      if (!LittleFS.remove(favoritePath(slot))) { server.send(404); return; }
      if (slot == activeFavorite) applySettings(std::move(base), -1);
    } else { server.send(400); return; }
    server.send(204);
  });
  server.on("/api/metro/catalog", HTTP_GET, [] {
    if (!requireAuth()) return;
    File f=LittleFS.open("/metro-network.json.gz","r");
    if (!f) { server.send(503); return; }
    server.sendHeader("Cache-Control","no-store"); server.streamFile(f,"application/json");
  });
  static File asset;
  static String assetName;
  static bool assetOK=false;
  server.on("/api/metro/asset",HTTP_POST,[] {
    if (!requireAuth()) return;
    if (assetOK && LittleFS.rename("/metro-asset.tmp","/"+assetName)) server.send(200,"text/plain","OK");
    else server.send(400,"text/plain","Asset upload failed");
  },[] {
    auto& u=server.upload();
    if (u.status==UPLOAD_FILE_START) {
      assetOK=false;
      if (!requireAuth()) return;
      assetName=u.filename;
      if (assetName!="metro-ui.html.gz" && assetName!="metro-network.json.gz") return;
      asset=LittleFS.open("/metro-asset.tmp","w"); assetOK=bool(asset);
    } else if (u.status==UPLOAD_FILE_WRITE && assetOK) {
      if (u.totalSize+u.currentSize>65536 || asset.write(u.buf,u.currentSize)!=u.currentSize) assetOK=false;
    } else if (u.status==UPLOAD_FILE_END || u.status==UPLOAD_FILE_ABORTED) {
      asset.close(); if (u.status==UPLOAD_FILE_ABORTED) assetOK=false;
    }
  });
  server.on("/api/metro/key", HTTP_POST, [] {
    if (!requireAuth()) return;
    String key = server.arg("apiKey");
    key.trim();
    if (key.isEmpty() || key.length() > 80) { server.send(400); return; }
    JsonDocument base;
    loadBase(base);
    if (base.isNull()) base = settings;
    base["apiKey"] = key;
    if (!saveJson("/metro.json", base)) { server.send(507); return; }
    settings["apiKey"] = key;
    server.send(204);
  });
  server.on("/api/metro/resolve",HTTP_GET,[] {
    if (!requireAuth()) return;
    String key = settings["apiKey"] | "";
    if (key.isEmpty()) { server.send(428); return; }
    time_t now=time(nullptr); tm local; localtime_r(&now,&local);
    String station=server.arg("station");
    if (station.isEmpty() || station.length()>120) { server.send(400); return; }
    if (!takeQuota(local)) { server.send(429); return; }
    WiFiClient client; HTTPClient http; http.useHTTP10(true); http.setTimeout(4000);
    http.begin(client,"http://swopenapi.seoul.go.kr/api/subway/"+encoded(key)+"/json/realtimeStationArrival/0/100/"+encoded(station));
    if (http.GET()!=200) { http.end(); server.send(502); return; }
    JsonDocument filter,doc;
    filter["errorMessage"]["code"]=true;
    for (const char* k:{"subwayId","updnLine","statnFid","statnTid","trainLineNm"}) filter["realtimeArrivalList"][0][k]=true;
    auto error=deserializeJson(doc,http.getStream(),DeserializationOption::Filter(filter)); http.end();
    if (error) { server.send(502); return; }
    String body; serializeJson(doc,body); server.send(200,"application/json",body);
  });
  server.on("/", [] {
    server.sendHeader("Cache-Control","no-store");
    File f=LittleFS.open("/metro-ui.html.gz","r");
    if (f) server.streamFile(f,"text/html; charset=utf-8");
    else { server.sendHeader("Content-Encoding","gzip"); server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(METRO_WEB), sizeof(METRO_WEB)); }
  });
  server.on("/api/metro", HTTP_GET, [] {
    if (!requireAuth()) return;
    JsonDocument publicSettings = settings;
    publicSettings.remove("apiKey");
    publicSettings["activeFavorite"] = activeFavorite + 1;
    publicSettings["language"] = koreanDisplay ? "ko" : "en";
    publicSettings["hasApiKey"] = !settings["apiKey"].isNull() && settings["apiKey"].as<String>().length() > 0;
    publicSettings["status"] = state;
    publicSettings["callsReservedToday"] = reserved;
    publicSettings["dataAgeSeconds"] = received ? int(time(nullptr) - received) : -1;
    publicSettings["trainSource"] = settings["lineId"].as<String>() == "1065" ? "AREX operator waitingTime" : "realtimeStationArrival";
    publicSettings["weatherAvailable"] = wx.valid;
    if (wx.valid) { publicSettings["seoulTemperature"] = wx.temp; publicSettings["weatherIcon"] = wx.icon; }
    File labels = openLabels();
    publicSettings["labelBytes"] = labels && labels.size() >= LABEL_BYTES ? LABEL_BYTES : 0;
    JsonArray arrivals = publicSettings["arrivals"].to<JsonArray>();
    int boardingEta[MAX_TRAINS]; std::fill(boardingEta,boardingEta+MAX_TRAINS,-1);
    int walking=settings["walkSeconds"]|0;
    publicSettings["walkSeconds"]=walking;
    for (int i = 0; i < trainCount; ++i) {
      JsonObject arrival = arrivals.add<JsonObject>();
      arrival["train"] = trains[i].number;
      arrival["station"] = trains[i].station;
      arrival["stateCode"] = trains[i].code;
      arrival["etaSeconds"] = trains[i].eta >= 0 ? std::max(0, trains[i].eta - int(time(nullptr) - received)) : -1;
      arrival["estimated"] = trains[i].estimated;
      arrival["operatorMinutes"] = trains[i].operatorMinutes;
      arrival["sourceAgeSeconds"] = int(time(nullptr) - trains[i].stamp);
      arrival["gaugeX"] = position(trains[i]);
      if(time(nullptr)<=trains[i].expires && trains[i].eta>=0)
        boardingEta[i]=boardingRemaining(trains[i],time(nullptr));
      arrival["boardingState"]=metroBoardingState(boardingEta[i],walking);
    }
    publicSettings["mainTrainIndex"]=metroCatchableTrain(boardingEta,trainCount,walking);
    String response; serializeJson(publicSettings, response);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", response);
  });
  server.on("/api/metro/labels", HTTP_POST, [] {
    if (!requireAuth()) return;
    if (server.arg("plain").length() != 7200) { server.send(400, "text/plain", "Expected 7200 bitmap bytes"); return; }
    File file = LittleFS.open("/metro-labels.pending", "w");
    if (!file) { server.send(507); return; }
    size_t written = file.write(reinterpret_cast<const uint8_t*>(server.arg("plain").c_str()), 7200);
    file.close();
    server.send(written == 7200 ? 200 : 507, "text/plain", "Labels staged");
  });
  server.on("/api/metro", HTTP_POST, [] {
    if (!requireAuth()) return;
    JsonDocument value;
    if (server.arg("plain").length() > 16000 || deserializeJson(value, server.arg("plain")) || !validSettings(value)) {
      server.send(400, "text/plain", "Invalid settings or route"); return;
    }
    if (!value["apiKey"].is<String>()) value["apiKey"] = settings["apiKey"];
    String key = value["apiKey"] | "";
    if (key.length() > 80) { server.send(400); return; }
    int slot = baseLabelSlot;
    if (LittleFS.exists("/metro-labels.pending")) {
      slot = 1 - slot;
      String path = String("/metro-labels-") + slot + ".bin";
      if (!LittleFS.rename("/metro-labels.pending", path)) {
        server.send(507, "text/plain", "Label activation failed"); return;
      }
    }
    value["labelSlot"] = slot;
    if (!saveJson("/metro.json", value)) { server.send(507); return; }
    baseLabelSlot = slot;
    applySettings(std::move(value), -1);
    cfg.save();
    yield();
    // Apply and fetch inside this request, before acknowledging completion.
    // Waiting for the normal loop can put weather/network work ahead of trains.
    time_t now=time(nullptr); tm local; localtime_r(&now,&local);
    metroDraw(local,now>1600000000);
    JsonDocument result;
    result["saved"]=true; result["refreshed"]=received!=0;
    result["status"]=state; result["arrivals"]=trainCount;
    String body; serializeJson(result,body); server.send(200,"application/json",body);
  });
}

void drawWeather() {
  auto& display = clockDisplay();
  display.fillRect(10,198,82,30,TFT_BLACK);
  bool valid = wx.valid && millis()-wx.lastSuccess <= 45UL*60*1000;
  display.setTextColor(TFT_WHITE,TFT_BLACK);
  String temperature = "--";
  if (valid) {
    int tenths = lroundf(wx.temp*10), absolute = abs(tenths);
    temperature = (tenths < 0 ? String("-") : String()) + absolute/10 + "." + absolute%10;
  }
  display.drawString(temperature,12,204,2);
  int width = display.textWidth(temperature,2);
  display.drawCircle(15+width,207,1,TFT_WHITE); display.drawString("C",18+width,204,2);
  int center = std::min(71,30+width)+8;
  if (!valid) return;
  String icon = wx.icon.substring(0,2);
  bool night = wx.icon.endsWith("n");
  if (icon == "01" || icon == "02") {
    display.fillCircle(center,209,5,TFT_YELLOW);
    if (night) display.fillCircle(center+3,207,4,TFT_BLACK);
    else for (int i=0;i<8;i++) {
      const int dx[] = {0,5,7,5,0,-5,-7,-5};
      const int dy[] = {-7,-5,0,5,7,5,0,-5};
      display.drawPixel(center+dx[i],209+dy[i],TFT_YELLOW);
    }
  }
  if (icon != "01") {
    display.fillCircle(center-4,211,4,0xbdf7); display.fillCircle(center+2,208,5,0xbdf7);
    display.fillCircle(center+7,212,4,0xbdf7); display.fillRoundRect(center-8,211,19,5,2,0xbdf7);
    if (icon == "09" || icon == "10") for (int x : {center-5,center+2,center+8}) display.drawLine(x,218,x-2,221,TFT_CYAN);
    if (icon == "13") for (int x : {center-4,center+6}) { display.drawLine(x-2,219,x+2,219,TFT_WHITE); display.drawLine(x,217,x,221,TFT_WHITE); }
    if (icon == "11") { display.drawLine(center+3,216,center-1,220,TFT_YELLOW); display.drawLine(center-1,220,center+4,220,TFT_YELLOW); display.drawLine(center+4,220,center,223,TFT_YELLOW); }
    if (icon == "50") { display.drawFastHLine(center-8,218,19,0xbdf7); display.drawFastHLine(center-6,222,15,0xbdf7); }
  }
}

void metroDraw(const tm& local, bool valid) {
  static String oldClock, oldText[2], oldWeather;
  static int oldX[MAX_TRAINS] = {}, oldMain=-2;
  static uint16_t oldColor[MAX_TRAINS] = {}, oldTextColor[2] = {};
  time_t now = time(nullptr);
  if (!operatorFailures && trainCount && now <= trains[0].expires && trains[0].eta-int(now-received) <= 90) {
    uint32_t interval = pollSeconds(true,local)*1000;
    if (int32_t(nextFetch-millis()) > int32_t(interval)) nextFetch = millis()+interval;
  }
  if (valid && WiFi.status() == WL_CONNECTED && (!nextFetch || int32_t(millis() - nextFetch) >= 0)) fetchTrains(local, now);
  if (!dirty && lastDraw == now) return;
  auto& display = clockDisplay();
  // Setup, reconnect and password screens center their text on this same panel.
  display.setTextDatum(TL_DATUM);
  bool repaint = dirty;
  if (dirty) { display.fillScreen(TFT_BLACK); drawLabels(); dirty = false; }
  lastDraw = now;
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  char clock[6]; snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
  String clockText = valid ? clock : "--:--";
  if (repaint || clockText != oldClock) {
    display.fillRect(157, 5, 83, 30, TFT_BLACK);
    display.drawRightString(clockText,232,8,4);
    oldClock = clockText;
  }
  int eta[MAX_TRAINS],x[MAX_TRAINS]; uint16_t color[MAX_TRAINS];
  std::fill(eta,eta+MAX_TRAINS,-1); std::fill(x,x+MAX_TRAINS,-1);
  int walking=(settings["walkSeconds"]|0);
  bool gaugeChanged=repaint;
  for(int i=0;i<MAX_TRAINS;i++) {
    if(i<trainCount && now<=trains[i].expires) {
      x[i]=position(trains[i]);
      eta[i]=boardingRemaining(trains[i],now);
    }
    int boarding=metroBoardingState(eta[i],walking);
    color[i]=boarding==0?TFT_RED:boarding==1?TFT_YELLOW:boarding<0?uint16_t(0x7bef):TFT_CYAN;
    gaugeChanged|=x[i]!=oldX[i] || color[i]!=oldColor[i];
    oldX[i]=x[i];oldColor[i]=color[i];
  }
  int main=metroCatchableTrain(eta,trainCount,walking);
  if(repaint || main!=oldMain) {
    display.fillRect(10,135,82,20,TFT_BLACK);
    if (koreanDisplay) display.drawBitmap(12,137,NEXT_TRAIN,64,14,TFT_WHITE);
    else { display.setTextColor(TFT_WHITE, TFT_BLACK); display.drawString("Next train",12,136,2); }
    oldMain=main; repaint=true; gaugeChanged=true;
  }
  if (gaugeChanged) {
  display.fillRect(10, 58, 220, 38, TFT_BLACK);
  display.fillRect(10, 80, 201, 3, 0x39e7);
  for (int x : {30,90,150,210}) display.fillCircle(x,81,4,0x7bef);
  if (main>=0 && x[main]>=0) display.fillRect(10,80,x[main]-10,3,color[main]);
  int drawn[MAX_TRAINS],n=0;
  for(int pass=0;pass<MAX_TRAINS+1;pass++) {
    int i=pass==0?main:pass-1;
    if(i<0 || i>=trainCount || (pass>0 && i==main) || x[i]<0) continue;
    bool overlap=false; for(int j=0;j<n;j++) overlap|=abs(x[i]-drawn[j])<16;
    if(overlap) continue;
    drawn[n++]=x[i]; int at=x[i]; uint16_t ink=color[i];
    display.fillRoundRect(at-7,65,14,17,3,ink);
    display.fillRect(at-5,68,10,6,TFT_BLACK);
    display.fillCircle(at-4,78,1,TFT_WHITE); display.fillCircle(at+4,78,1,TFT_WHITE);
    display.drawLine(at-4,82,at-6,85,ink); display.drawLine(at+4,82,at+6,85,ink);
  }
  }

  for (int i=0;i<2;i++) {
    int selected=main<0?-1:main+i;
    if(selected>=trainCount) selected=-1;
    int remaining=selected>=0?eta[selected]:-1;
    // Operator publishes whole minutes, not a second-accurate live countdown.
    // Hold its latest value until the next response; decrementing inferred
    // seconds can show a smaller minute and then jump back on the next fetch.
    bool soon = remaining >= 0 && remaining <= 60;
    int minutes = remaining < 0 ? -1 : trains[selected].operatorTime
        ? trains[selected].operatorMinutes : (std::min(remaining, 60000) + 59) / 60;
    String text = soon ? "soon" : minutes < 0 ? "--" : minutes > 999 ? "999+" : String(minutes);
    const bool compact = text.length() > 2;
    // Use exactly the same travel-time margin as the selected train's icon.
    // A missed train is excluded by main selection, never shown as a red ETA.
    uint16_t textColor = metroBoardingState(remaining,walking)==1 ? TFT_YELLOW : i ? TFT_WHITE : TFT_GREEN;
    if (!repaint && text == oldText[i] && textColor == oldTextColor[i]) continue;
    oldText[i] = text; oldTextColor[i] = textColor;
    display.setTextColor(textColor,TFT_BLACK);
    if (i == 0) {
      display.fillRect(93,130,147,81,TFT_BLACK);
      if (soon && !koreanDisplay) display.drawString("Soon", 116, 153, 4);
      else if (soon) {
        display.drawBitmap(108,134,SOON,64,66,textColor);
        display.drawBitmap(179,182,ARRIVED,32,18,textColor);
      }
      else {
        display.drawRightString(text,207,compact ? 183 : 132,compact ? 4 : 8);
        if (koreanDisplay) display.drawBitmap(212,190,MINUTE,24,20,textColor);
        else display.drawString("min",211,189,2);
      }
    } else {
      display.fillRect(10,159,82,39,TFT_BLACK);
      if (soon) {
        if (koreanDisplay) display.drawBitmap(12,172,SOON_SMALL,80,20,textColor);
        else display.drawString("Soon",12,172,2);
      } else {
        display.drawRightString(text,57,compact ? 177 : 171,compact ? 2 : 4);
        if (koreanDisplay) display.drawBitmap(62,180,MINUTE_SMALL,24,20,textColor);
        else display.drawString("min",61,177,2);
      }
    }
  }
  bool weatherValid = wx.valid && millis()-wx.lastSuccess <= 45UL*60*1000;
  String weatherKey = weatherValid ? String(wx.temp,1)+wx.icon : "invalid";
  if (repaint || weatherKey != oldWeather) { drawWeather(); oldWeather = weatherKey; }
}

