// Arduino 1.8.15 , board manager:M5stack 1.0.9 M5stack-CoreInk , library:M5stack coreink 0.0.3
#include <Arduino.h>
#include "M5CoreInk.h"
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "icon.h"
#include "esp_adc_cal.h"

#include <HTTPClient.h>
#include <ArduinoJson.h> //need version 5.13.5
#include <string>

#include "efontEnableJa.h"        // 日本語     368kB
#include "efont.h"                // この前にfont範囲を書く
#include "efontM5StackCoreInk.h"  // https://github.com/tanakamasayuki/efont

#define WIFI_RETRY_CONNECTION 5

Ink_Sprite TimePageSprite(&M5.M5Ink);
Ink_Sprite PopPageSprite(&M5.M5Ink);
RTC_TimeTypeDef RTCtime,RTCTimeSave;
RTC_DateTypeDef RTCDate;
uint8_t second = 0 ,minutes = 0;

Preferences preferences;

const char* NTP_SERVER = "ntp.nict.jp";
const char* TZ_INFO    = "JST-9";
const char* WIFI_SSID = "*************";  // ルーターのSSIDを記入
const char* WIFI_PASS = "*************";  // ルーターのPASSWORDを記入
const String key = "******.json";  // 地域コードを記入 東京都の場合、130000.json
const String endpoint = "https://www.jma.go.jp/bosai/forecast/data/forecast/";
char *wd[7] = {"日","月","火","水","木","金","土"};
int gwt[] = {5,11,17}; // 天気予報を取得する時刻をきさいする。 この場合は、5時と11時と17時
int size = sizeof(gwt) / sizeof(int);

tm timeinfo;
time_t now;
long unsigned lastNTPtime;
unsigned long lastEntryTime;

String a[] = {"" , "" , "" , ""};
String stelop = "";
String fdate = "";

void drawImageToSprite(int posX,int posY,image_t* imagePtr,Ink_Sprite* sprite)
{
    sprite->drawBuff(   posX, posY,
                        imagePtr->width, imagePtr->height, imagePtr->ptr);
}

void drawTime( RTC_TimeTypeDef *time )
{
    drawImageToSprite(10 ,40,&num55[time->Hours/10],&TimePageSprite);
    drawImageToSprite(50 ,40,&num55[time->Hours%10],&TimePageSprite);
    drawImageToSprite(90 ,40,&num55[10],&TimePageSprite);
    drawImageToSprite(110,40,&num55[time->Minutes/10],&TimePageSprite);
    drawImageToSprite(150,40,&num55[time->Minutes%10],&TimePageSprite);
}

void darwDate( RTC_DateTypeDef *date)
{
    int posX = 10;
    char c0[10], c1[10], c2[10] , c3[10];
    itoa(RTCDate.Month / 10 % 10 ,c0 ,10);
    itoa(RTCDate.Month % 10 ,c1 ,10);   
    itoa(RTCDate.Date / 10 % 10 ,c2 ,10);
    itoa(RTCDate.Date % 10 ,c3 ,10);
    if (RTCDate.Month / 10 % 10 >0)
    {printEfont(&TimePageSprite, c0 ,posX,0,2);posX += 17;}
    else
    {printEfont(&TimePageSprite, "0" ,posX,0,2);posX += 17;}
    printEfont(&TimePageSprite, c1 ,posX,0,2);posX += 17;    
    printEfont(&TimePageSprite, "月" ,posX,0,2);posX += 34;   
    if (RTCDate.Date / 10 % 10 >0)
    {printEfont(&TimePageSprite, c2 ,posX,0,2);posX += 17;}
    else
    {printEfont(&TimePageSprite, "0" ,posX,0,2);posX += 17;}
    printEfont(&TimePageSprite, c3 ,posX,0,2);posX += 17;    
    printEfont(&TimePageSprite, "日" ,posX,0,2);posX += 42;
    printEfont(&TimePageSprite, wd[RTCDate.WeekDay] ,posX,0,2);posX += 17;   
}

float getBatVoltage()
{
    analogSetPinAttenuation(35,ADC_11db);
    esp_adc_cal_characteristics_t *adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 3600, adc_chars);
    uint16_t ADCValue = analogRead(35);
    
    uint32_t BatVolmV  = esp_adc_cal_raw_to_voltage(ADCValue,adc_chars);
    float BatVol = float(BatVolmV) * 25.1 / 5.1 / 1000;
    return BatVol;
}

void drawScanWifi()
{
    M5.M5Ink.clear();
    TimePageSprite.clear();
}

void drawWarning(const char *str)
{
    M5.M5Ink.clear();
    TimePageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
    drawImageToSprite(76,40,&warningImage,&TimePageSprite);
    int length = 0;
    while(*( str + length ) != '\0') length++;
    TimePageSprite.drawString((200 - length * 8) / 2,100,str,&AsciiFont8x16);
    TimePageSprite.pushSprite();
}

void drawTimePage( )
{
    M5.rtc.GetTime(&RTCtime);
    drawTime(&RTCtime);
    minutes = RTCtime.Minutes;
    M5.rtc.GetDate(&RTCDate);
    darwDate(&RTCDate);
    TimePageSprite.pushSprite();
}

void saveBool(String key, bool value){
    preferences.begin("M5CoreInk", false);
    preferences.putBool(key.c_str(), value);
    preferences.end();
}

bool loadBool(String key) {
    preferences.begin("M5CoreInk", false);
    bool keyvalue =preferences.getBool(key.c_str(),false);
    preferences.end();
    return keyvalue;
}

void flushTimePage()
{
    while(1)
    {
        M5.rtc.GetTime(&RTCtime);
        if( minutes != RTCtime.Minutes )
        {
            M5.rtc.GetTime(&RTCtime);
            M5.rtc.GetDate(&RTCDate);
            drawTime(&RTCtime);
            darwDate(&RTCDate);
            TimePageSprite.pushSprite();
            minutes = RTCtime.Minutes;         
            TimePageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
            PopPageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );

            for ( int i = 0; i <size ; i++){
              if( RTCtime.Hours == gwt[i] && RTCtime.Minutes == 0 )  //天気情報取得
              {
		wifiInit();
                getweather();
                PopPageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
                PopPageSprite.pushSprite();
		M5.M5Ink.clear();
                setCpuFrequencyMhz(10);
                drawpop();
              }
            }
            Serial.println ("ShutDown");
            M5.shutdown(48); //original 58 -> 53
            //it only is reached when the USB is connected
            Serial.println ("USB Power");
            delay(45*1000);  // original 55 ->50
            Serial.println ("Restart");
            ESP.restart();
        }

        delay(10);
        M5.update();
        if( M5.BtnPWR.wasPressed())
        {
            digitalWrite(LED_EXT_PIN,LOW);
            saveBool("clock_suspend",false);
            M5.shutdown();
        }
        if( M5.BtnDOWN.wasPressed() || M5.BtnUP.wasPressed()) break;
    }
    M5.M5Ink.clear();
    TimePageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
    PopPageSprite.clear( CLEAR_DRAWBUFF | CLEAR_LASTBUFF );
}

void checkBatteryVoltage( bool powerDownFlag )
{
    float batVol = getBatVoltage();
    Serial.printf("Bat Voltage %.2f\r\n",batVol);
    if( batVol > 3.2 ) return;

    drawWarning("Battery voltage is low");
    if( powerDownFlag == true )
    {
         M5.shutdown();
    }
    while( 1 )
    {
        batVol = getBatVoltage();
        if( batVol > 3.2 ) return;
    }
}

void checkRTC()
{
    M5.rtc.GetTime(&RTCtime);
    if( RTCtime.Seconds == RTCTimeSave.Seconds )
    {
        drawWarning("RTC Error");
        while( 1 )
        {
            if( M5.BtnMID.wasPressed()) return;
            delay(10);
            M5.update();
        }
    }
}

void showTime(tm localTime) {
    Serial.print("[NTP] ");
    Serial.print(localTime.tm_mday);
    Serial.print('/');
    Serial.print(localTime.tm_mon + 1);
    Serial.print('/');
    Serial.print(localTime.tm_year - 100);
    Serial.print('-');
    Serial.print(localTime.tm_hour);
    Serial.print(':');
    Serial.print(localTime.tm_min);
    Serial.print(':');
    Serial.print(localTime.tm_sec);
    Serial.print(" Day of Week ");
    if (localTime.tm_wday == 0)
        Serial.println(7);
    else
        Serial.println(localTime.tm_wday);
}

void saveRtcData() {
    RTCtime.Minutes = timeinfo.tm_min;
    RTCtime.Seconds = timeinfo.tm_sec;
    RTCtime.Hours = timeinfo.tm_hour;
    RTCDate.Year = timeinfo.tm_year+1900;
    RTCDate.Month = timeinfo.tm_mon+1;
    RTCDate.Date = timeinfo.tm_mday;
    RTCDate.WeekDay = timeinfo.tm_wday;

    char timeStrbuff[64];
    sprintf(timeStrbuff, "%d/%02d/%02d %02d:%02d:%02d",
            RTCDate.Year, RTCDate.Month, RTCDate.Date,
            RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);

    Serial.println("[NTP] in: " + String(timeStrbuff));

    M5.rtc.SetTime(&RTCtime);
    M5.rtc.SetDate(&RTCDate);
}

bool getNTPtime(int sec) {
    {
        Serial.print("[NTP] sync.");
        uint32_t start = millis();
        do {
            time(&now);
            localtime_r(&now, &timeinfo);
            Serial.print(".");
            delay(10);
        } while (((millis() - start) <= (1000 * sec)) && (timeinfo.tm_year < (2016 - 1900)));
        if (timeinfo.tm_year <= (2016 - 1900)) return false;

        Serial.print("now ");
        Serial.println(now);
        saveRtcData();
        char time_output[30];
        strftime(time_output, 30, "%a  %d-%m-%y %T", localtime(&now));
        Serial.print("[NTP] ");
        Serial.println(time_output);
    }
    return true;
}

void ntpInit() {
    if (WiFi.isConnected()) {
        configTime(0, 0, NTP_SERVER);
        setenv("TZ", TZ_INFO, 1);
        if (getNTPtime(10)) {  // wait up to 10sec to sync
        } else {
            Serial.println("[NTP] Time not set");
            ESP.restart();
        }
        showTime(timeinfo);
        lastNTPtime = time(&now);
        lastEntryTime = millis();
    }
}

void wifiInit() {
    setCpuFrequencyMhz(80);
    Serial.print("[WiFi] connecting to "+String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int wifi_retry = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_retry++ < WIFI_RETRY_CONNECTION) { 
        Serial.print(".");
        delay(500);
    }
    if (wifi_retry >= WIFI_RETRY_CONNECTION)
        Serial.println(" failed!");
    else
        Serial.println(" connected!");
}

void getweather() {
  if ((WiFi.status() == WL_CONNECTED)) {  
    Serial.println("getweather");
    HTTPClient http;
    http.begin(endpoint + key);
    int httpCode = http.GET();
    delay(100);
    if (httpCode > 0) {
        String payload = http.getString();
        DynamicJsonBuffer jsonBuffer;
        int len=payload.length();
        payload = payload.substring(1,len-1);
        String json = payload;
        JsonObject& weatherdata = jsonBuffer.parseObject(json);
        Serial.println(payload);
        if(!weatherdata.success()){
          Serial.println("parseObject() failed");
        }
    WiFi.mode(WIFI_OFF);
        const char* datedef = weatherdata["reportDatetime"].as<char*>();
        int k = 3 ;
    for (int i = weatherdata["timeSeries"][1]["areas"][0]["pops"].size() - 5; i>=0 ; i--){
        const char* timedef = weatherdata["timeSeries"][1]["timeDefines"][i].as<char*>();
        const char* pop = weatherdata["timeSeries"][1]["areas"][0]["pops"][i].as<char*>();
        String strtimedef = timedef;
        String ftime = strtimedef.substring(11,13);
        String strdatedef = datedef;
        fdate = strdatedef.substring(5,13);

        a[k] = String(pop);
        k -= 1 ;

        for ( int j = 0 ;j <4 ; j++){
            if (a[j] == ""){
              a[j] = "-" ;
            }
        }
       }
       
    const char* weathercodes = weatherdata["timeSeries"][0]["areas"][0]["weatherCodes"][0].as<char*>(); 
    
    DynamicJsonBuffer wtelopbuffer;
    char wtelops[] = "{\"100\":\"晴\",\"101\":\"晴時々曇\",\"102\":\"晴一時雨\",\"103\":\"晴時々雨\",\"104\":\"晴一時雪\",\"105\":\"晴時々雪\",\"106\":\"晴一時雨か雪\",\"107\":\"晴時々雨か雪\",\"108\":\"晴一時雨か雷雨\",\"110\":\"晴後時々曇\",\"111\":\"晴後曇\",\"112\":\"晴後一時雨\",\"113\":\"晴後時々雨\",\"114\":\"晴後雨\",\"115\":\"晴後一時雪\",\"116\":\"晴後時々雪\",\"117\":\"晴後雪\",\"118\":\"晴後雨か雪\",\"119\":\"晴後雨か雷雨\",\"120\":\"晴朝夕一時雨\",\"121\":\"晴朝の内一時雨\",\"122\":\"晴夕方一時雨\",\"123\":\"晴山沿い雷雨\",\"124\":\"晴山沿い雪\",\"125\":\"晴午後は雷雨\",\"126\":\"晴昼頃から雨\",\"127\":\"晴夕方から雨\",\"128\":\"晴夜は雨\",\"130\":\"朝の内霧後晴\",\"131\":\"晴明け方霧\",\"132\":\"晴朝夕曇\",\"140\":\"晴時々雨で雷を伴う\",\"160\":\"晴一時雪か雨\",\"170\":\"晴時々雪か雨\",\"181\":\"晴後雪か雨\",\"200\":\"曇\",\"201\":\"曇時々晴\",\"202\":\"曇一時雨\",\"203\":\"曇時々雨\",\"204\":\"曇一時雪\",\"205\":\"曇時々雪\",\"206\":\"曇一時雨か雪\",\"207\":\"曇時々雨か雪\",\"208\":\"曇一時雨か雷雨\",\"209\":\"霧\",\"210\":\"曇後時々晴\",\"211\":\"曇後晴\",\"212\":\"曇後一時雨\",\"213\":\"曇後時々雨\",\"214\":\"曇後雨\",\"215\":\"曇後一時雪\",\"216\":\"曇後時々雪\",\"217\":\"曇後雪\",\"218\":\"曇後雨か雪\",\"219\":\"曇後雨か雷雨\",\"220\":\"曇朝夕一時雨\",\"221\":\"曇朝の内一時雨\",\"222\":\"曇夕方一時雨\",\"223\":\"曇日中時々晴\",\"224\":\"曇昼頃から雨\",\"225\":\"曇夕方から雨\",\"226\":\"曇夜は雨\",\"228\":\"曇昼頃から雪\",\"229\":\"曇夕方から雪\",\"230\":\"曇夜は雪\",\"231\":\"曇海上海岸は霧か霧雨\",\"240\":\"曇時々雨で雷を伴う\",\"250\":\"曇時々雪で雷を伴う\",\"260\":\"曇一時雪か雨\",\"270\":\"曇時々雪か雨\",\"281\":\"曇後雪か雨\",\"300\":\"雨\",\"301\":\"雨時々晴\",\"302\":\"雨時々止む\",\"303\":\"雨時々雪\",\"304\":\"雨か雪\",\"306\":\"大雨\",\"308\":\"雨で暴風を伴う\",\"309\":\"雨一時雪\",\"311\":\"雨後晴\",\"313\":\"雨後曇\",\"314\":\"雨後時々雪\",\"315\":\"雨後雪\",\"316\":\"雨か雪後晴\",\"317\":\"雨か雪後曇\",\"320\":\"朝の内雨後晴\",\"321\":\"朝の内雨後曇\",\"322\":\"雨朝晩一時雪\",\"323\":\"雨昼頃から晴\",\"324\":\"雨夕方から晴\",\"325\":\"雨夜は晴\",\"326\":\"雨夕方から雪\",\"327\":\"雨夜は雪\",\"328\":\"雨一時強く降る\",\"329\":\"雨一時みぞれ\",\"340\":\"雪か雨\",\"350\":\"雨で雷を伴う\",\"361\":\"雪か雨後晴\",\"371\":\"雪か雨後曇\",\"400\":\"雪\",\"401\":\"雪時々晴\",\"402\":\"雪時々止む\",\"403\":\"雪時々雨\",\"405\":\"大雪\",\"406\":\"風雪強い\",\"407\":\"暴風雪\",\"409\":\"雪一時雨\",\"411\":\"雪後晴\",\"413\":\"雪後曇\",\"414\":\"雪後雨\",\"420\":\"朝の内雪後晴\",\"421\":\"朝の内雪後曇\",\"422\":\"雪昼頃から雨\",\"423\":\"雪夕方から雨\",\"425\":\"雪一時強く降る\",\"426\":\"雪後みぞれ\",\"427\":\"雪一時みぞれ\",\"450\":\"雪で雷を伴う\"}";   
    JsonObject& wwtelops = wtelopbuffer.parseObject(wtelops);
    delay(10);
    if (!wwtelops.success()) {
      Serial.println("parseObject() failed2");
      return;
    }
    
    const char* telop = wwtelops[weathercodes].as<char*>();
    stelop = String(telop);
    char t0[256];
    stelop.toCharArray(t0, 256);
    }
 
    else {
      Serial.println("Error on HTTP request");
      }
    http.end();
  }
 return;
}

void drawpop(){  
       char t0[256];
       stelop.toCharArray(t0, 256);
       printEfont(&PopPageSprite, t0 ,0,0,2);
       char a0[5], a1[5], a2[5] , a3[5] , a4[10];
       a[0].toCharArray(a0, 5);
       printEfont(&PopPageSprite, a0 ,4,68,2);
       a[1].toCharArray(a1, 5);
       printEfont(&PopPageSprite, a1 ,52,68,2);
       a[2].toCharArray(a2, 5);       
       printEfont(&PopPageSprite, a2 ,104,68,2);
       a[3].toCharArray(a3, 5);
       printEfont(&PopPageSprite, a3 ,152,68,2);
       fdate.toCharArray(a4, 10);
       printEfont(&PopPageSprite, a4 ,95,38,1);
       printEfont(&PopPageSprite, "更新" ,160,38,1);
       printEfont(&PopPageSprite, "[0~]  [6~]  [12~]  [18~]" ,0,54,1);
       printEfont(&PopPageSprite, "降水確率(%)" ,0,38,1);
       PopPageSprite.pushSprite();  
}
void setup()
{
    setCpuFrequencyMhz(10);
    pinMode(LED_EXT_PIN, OUTPUT);
    digitalWrite(LED_EXT_PIN, HIGH);
    M5.begin();
        delay(100);

    if ( !M5.M5Ink.isInit()) {
      Serial.printf("Ink Init faild");
      while (1) {
        delay(100);
      }
    }
    
    Serial.println(__TIME__);
    M5.rtc.GetTime(&RTCTimeSave);
    M5.rtc.GetTime(&RTCtime);
    M5.update();

    Serial.print("RTCtime.Hours = ");
    Serial.println(RTCtime.Hours);

    int flag = 0 ;    
    if( M5.BtnMID.isPressed())
    {
        M5.Speaker.tone(2700,200);
        delay(100);
        M5.Speaker.mute();
        M5.M5Ink.clear();
//        M5.M5Ink.drawBuff((uint8_t *)image_CoreInkWWellcome);
        delay(100);
        wifiInit();
        ntpInit();
        setCpuFrequencyMhz(10);
        flag = 1 ;
    }

    //checkRTC();
    checkBatteryVoltage(false); //false
    TimePageSprite.creatSprite(0,0,200,100);
    PopPageSprite.creatSprite(0,100,200,100);
    drawTimePage(); 
    if (flag == 1){
    wifiInit();
    getweather();
    setCpuFrequencyMhz(10);
    drawpop();
    }
}

void loop()
{
    flushTimePage();

    if( M5.BtnPWR.wasPressed())
    {
        Serial.printf("Btn %d was pressed \r\n",BUTTON_EXT_PIN);
        digitalWrite(LED_EXT_PIN,LOW);
        M5.shutdown();
    }
    M5.update();
}
