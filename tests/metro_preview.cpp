// Host pixel check using the production draw functions and real TFT font tables.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <fstream>
#include <array>
#include <ctime>
#include <cassert>
#include "weather.h"
#include "metro_estimate.h"
#include "metro_arrival_glyphs.h"
constexpr uint16_t TFT_BLACK=0, TFT_WHITE=0xffff, TFT_YELLOW=0xffe0,
  TFT_RED=0xf800, TFT_GREEN=0x07e0, TFT_CYAN=0x07ff;
class PreviewDisplay : public TFT_eSPI {
 public:
  void drawBitmap(int x,int y,const uint8_t* data,int w,int h,uint16_t color) {
    for(int py=0;py<h;py++)for(int px=0;px<w;px++)
      if(data[py*((w+7)/8)+px/8]&(128>>(px%8)))drawPixel(x+px,y+py,color);
  }
  void drawRightString(const String& s,int x,int y,int font) {
    const auto previous = getTextDatum();
    setTextDatum(TR_DATUM);
    drawString(s,x,y,font);
    setTextDatum(previous);
  }
  void drawLine(int x,int y,int endX,int endY,uint16_t color) {
    int dx=abs(endX-x), sx=x<endX?1:-1, dy=-abs(endY-y), sy=y<endY?1:-1, error=dx+dy;
    for (;;) {
      drawPixel(x,y,color); if(x==endX&&y==endY) break;
      int twice=2*error; if(twice>=dy){error+=dy;x+=sx;} if(twice<=dx){error+=dx;y+=sy;}
    }
  }
  void drawCircle(int x,int y,int r,uint16_t color) {
    for(int py=-r;py<=r;py++)for(int px=-r;px<=r;px++)
      if(abs(px*px+py*py-r*r)<=r)drawPixel(x+px,y+py,color);
  }
};
PreviewDisplay panel;
PreviewDisplay& clockDisplay(){return panel;}
time_t fakeNow=1700000000, received=fakeNow, lastDraw=0;
uint32_t fakeMillis=10000, nextFetch=1000000000;
uint8_t operatorFailures=0;
uint32_t millis(){return fakeMillis;}
void delay(unsigned long){}
time_t metroTime(time_t*){return fakeNow;}
struct Train { String number, order, station; int eta=-1,code=99,operatorMinutes=-1;time_t stamp=0,expires=0;bool estimated=false,operatorTime=true; } trains[8];
constexpr int MAX_TRAINS=8;
int trainCount=2;
JsonDocument settings;
int routeIndex(const char* target) {
  JsonArray route = settings["route"].as<JsonArray>();
  for (unsigned i=0; i<route.size(); ++i)
    if (strcmp(route[i] | "", target) == 0) return int(i);
  return -1;
}
bool dirty=true;
bool koreanDisplay=true;
Weather wx;
constexpr int WL_CONNECTED=3;
struct { int status(){return WL_CONNECTED;} } WiFi;
unsigned pollSeconds(bool,const tm&){return 20;}
void fetchTrains(const tm&,time_t){}
int boardingRemaining(const Train& train,time_t now){return now<=train.expires&&train.eta>=0?std::max(0,train.eta-int(now-received)):-1;}
int position(const Train& train){return metroSynchronizedGauge(&train==&trains[0]?150:30,210,train.eta,std::max(0,train.eta-int(fakeNow-received)));}
constexpr size_t LABEL_BYTES = 7200;
std::array<uint8_t,LABEL_BYTES> labels;
bool hasStoredLabels = true;
class File {
  size_t offset = 0;
 public:
  explicit operator bool() const { return hasStoredLabels; }
  size_t size() const { return labels.size(); }
  size_t read(uint8_t* target, size_t count) {
    if (offset + count > labels.size()) return 0;
    memcpy(target, labels.data() + offset, count);
    offset += count;
    return count;
  }
};
File openLabels() { return {}; }
void yield() {}
#define time metroTime
#include "metro_render.inc"
#undef time

int main(int argc,char**argv){
  assert(argc==2);
  std::string out=argv[1];
  std::ifstream(out+"/labels.bin",std::ios::binary).read(reinterpret_cast<char*>(labels.data()),labels.size());
  // Old uploads may contain pixels in regions now owned by dynamic text.
  labels[18*30+30-1] = 255;
  labels[143*30+30-1] = 255;
  labels[235*30+2] = 255;
  tm local={};local.tm_hour=22;local.tm_min=30;
  auto save=[&](const char*name){std::ofstream(out+"/"+name+".raw",std::ios::binary).write(reinterpret_cast<char*>(panel.fb),sizeof(panel.fb));};
  settings["station"]="인천공항2터미널";
  settings["lineId"]="1065";
  settings["direction"]=1;
  for (const char* station : {"디지털미디어시티", "청라국제도시", "공항화물청사", "인천공항2터미널"})
    settings["route"].add(station);
  // The Wi-Fi/IP and password screens leave the panel in middle-center mode.
  panel.setTextDatum(MC_DATUM);
  wx.valid=true;wx.temp=9.3;wx.icon="03n";wx.lastSuccess=fakeMillis;
  trains[0].eta=350;trains[1].eta=850;trains[0].expires=trains[1].expires=fakeNow+75;
  trains[0].operatorMinutes=5;trains[1].operatorMinutes=14;
  metroDraw(local,true);save("two-digits");
  assert(panel.getTextDatum() == TL_DATUM);
  assert(panel.fb[18*240+238]==TFT_BLACK);
  assert(panel.fb[143*240+238]==TFT_BLACK);
  std::array<uint16_t,57600> publishedFrame;
  std::copy(std::begin(panel.fb),std::end(panel.fb),publishedFrame.begin());
  for(int y=228;y<240;y++)for(int x=10;x<92;x++)assert(panel.fb[y*240+x]==TFT_BLACK);
  // Check actual font widths, including a negative two-digit temperature.
  for(const char* value:{"9.3","29.8","-12.4"}){
    int width=panel.textWidth(value,2), iconLeft=std::min(71,30+width);
    assert(18+width+panel.textWidth("C",2)<iconLeft);
    assert(iconLeft+19<=92);
  }
  fakeNow+=22;fakeMillis+=22000;metroDraw(local,true);
  std::array<uint16_t,57600> incremental;std::copy(std::begin(panel.fb),std::end(panel.fb),incremental.begin());
  dirty=true;metroDraw(local,true);
  assert(std::equal(incremental.begin(),incremental.end(),std::begin(panel.fb)));
  save("after-22-seconds");
  fakeNow+=40;fakeMillis+=40000;metroDraw(local,true);
  // Whole-minute source is still 5: local seconds must not change it to 4.
  for(int y=130;y<198;y++)for(int x=93;x<240;x++)
    assert(panel.fb[y*240+x]==publishedFrame[y*240+x]);
  received=fakeNow;trains[1].eta=350;wx.temp=-12.4;dirty=true;metroDraw(local,true);save("negative-temperature");
  for(int eta:{360,300,180,120,60,0}) {
    received=++fakeNow;trains[0].eta=eta;trains[0].expires=fakeNow+75;
    trains[0].operatorMinutes=eta/60;
    metroDraw(local,true);
    uint16_t color=eta<=120?TFT_YELLOW:TFT_CYAN;
    int x=position(trains[0]);assert(panel.fb[66*240+x]==color);
    std::array<uint16_t,57600> before;
    std::copy(std::begin(panel.fb),std::end(panel.fb),before.begin());
    dirty=true;metroDraw(local,true);
    assert(std::equal(before.begin(),before.end(),std::begin(panel.fb)));
    if(eta==120)save("soon-arrival");
    if(eta==240)save("four-minutes");
  }
  received=++fakeNow;settings["walkSeconds"]=300;trainCount=3;trains[2].eta=840;trains[2].operatorMinutes=14;trains[2].expires=fakeNow+75;
  trains[0].eta=120;trains[0].operatorMinutes=2;
  trains[1].eta=600;trains[1].operatorMinutes=10;
  trains[0].expires=trains[1].expires=fakeNow+75;
  dirty=true;metroDraw(local,true);save("walking-promoted");
  assert(panel.fb[66*240+position(trains[0])]==TFT_RED);
  assert(panel.fb[66*240+position(trains[1])]==TFT_CYAN);
  // Seven minutes to arrival and five to walk: icon AND large ETA are yellow.
  for(int margin:{121,120,119,0,-1}) {
    received=++fakeNow;settings["walkSeconds"]=300;
    trains[0].eta=300+margin;trains[0].operatorMinutes=7;
    trains[1].eta=900;trains[1].operatorMinutes=15;
    for(int i=0;i<trainCount;i++)trains[i].expires=fakeNow+75;
    dirty=true;metroDraw(local,true);
    uint16_t icon=margin<0?TFT_RED:margin<=120?TFT_YELLOW:TFT_CYAN;
    assert(panel.fb[66*240+position(trains[0])]==icon);
    int yellow=0,green=0,red=0;
    for(int y=130;y<208;y++)for(int x=93;x<240;x++){
      uint16_t pixel=panel.fb[y*240+x];yellow+=pixel==TFT_YELLOW;green+=pixel==TFT_GREEN;red+=pixel==TFT_RED;
    }
    if(margin>=0&&margin<=120){assert(yellow>0);assert(green==0);}
    else {assert(green>0);assert(yellow==0);}
    assert(red==0);
    if(margin==120)save("boarding-yellow-synced");
  }
  // A language change must repaint even when the time and arrival data are unchanged.
  std::array<uint16_t,57600> korean;
  std::copy(std::begin(panel.fb),std::end(panel.fb),korean.begin());
  koreanDisplay=false; dirty=true;
  panel.setTextDatum(MC_DATUM);
  metroDraw(local,true);
  assert(panel.getTextDatum() == TL_DATUM);
  assert(!std::equal(korean.begin()+130*240,korean.begin()+197*240,std::begin(panel.fb)+130*240));
  assert(panel.fb[143*240+238]==TFT_BLACK);
  save("english-arrivals");
  panel.fillScreen(TFT_BLACK);
  latinLabel(latinName("Incheon International Airport Terminal 2"),10,7,138,2,false,TFT_WHITE);
  latinLabel(latinName("Dongdaemun History and Culture Park"),30,97,54,2,true,TFT_WHITE);
  const auto countInk = [](int left, int top, int right, int bottom) {
    int n = 0;
    for (int y=top; y<bottom; ++y)
      for (int x=left; x<right; ++x) n += panel.fb[y*240+x] != TFT_BLACK;
    return n;
  };
  assert(countInk(10,7,148,22) && countInk(10,22,148,38));
  assert(countInk(3,97,57,112) && countInk(3,112,57,128));
  assert(!countInk(148,7,240,38) && !countInk(57,97,240,128));
  assert(latinName("마곡나루") == "Magoknaru");
  const std::array<Train,3> previous{trains[0],trains[1],trains[2]};
  for (int i=0;i<3;i++) {
    trains[i].eta=7500+i*600;
    trains[i].operatorMinutes=125+i*10;
  }
  received=fakeNow;
  dirty=true; metroDraw(local,true);
  assert(!countInk(90,130,93,198));
  save("english-long-eta");
  std::copy(previous.begin(),previous.end(),std::begin(trains));
  koreanDisplay=true; dirty=true; metroDraw(local,true);
  assert(std::equal(korean.begin(),korean.end(),std::begin(panel.fb)));
  // Exercise incremental changes in both languages and switch route direction.
  // The compact ETA fonts must fit the same boxes even as value lengths change.
  settings["walkSeconds"]=0;
  trainCount=2;
  for (bool korean : {false,true}) {
    koreanDisplay=korean;
    settings["direction"]=-1;
    settings["station"]="디지털미디어시티";
    dirty=true;
    for (int minutes : {9,88,125,999,1200,1,-1,9}) {
      received=++fakeNow;
      for (int i=0; i<trainCount; ++i) {
        trains[i].operatorMinutes=minutes;
        trains[i].eta=minutes<0 ? -1 : minutes*60;
        trains[i].expires=fakeNow+75;
      }
      metroDraw(local,true);
      assert(!countInk(90,130,93,198)); // alley between estimates
      assert(countInk(95,130,207,211));
      assert(countInk(10,159,57,198));
      assert(countInk(12,204,60,221));
      assert(!countInk(0,130,10,228) && !countInk(236,130,240,228));
      std::copy(std::begin(panel.fb),std::end(panel.fb),incremental.begin());
      dirty=true;
      metroDraw(local,true);
      assert(std::equal(incremental.begin(),incremental.end(),std::begin(panel.fb)));
    }
    if (!korean) save("english-reverse-direction");
  }
  hasStoredLabels=false;
  koreanDisplay=true;
  dirty=true;
  metroDraw(local,true);
  assert(countInk(10,38,58,50)); // Korean setup prompt before the first save.
  return 0;
}
