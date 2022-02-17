//https://dl.espressif.com/dl/package_esp32_index.json
//http://arduino.esp8266.com/stable/package_esp8266com_index.json
// 2.794 millimeter per once
// 90 miilmeter limit
// 20 min cooldwon reset
void ICACHE_RAM_ATTR countWater();

//#if defined(ESP32)
//#include <WiFi.h>
//#elif defined(ESP8266)
//#include <ESP8266WiFi.h>
//#endif
#include <AutoConnect.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include <addons/TokenHelper.h>

//Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#include <TridentTD_LineNotify.h>
#include <SPI.h>
#include <SD.h>

/* 1. Define the WiFi credentials */
//#define WIFI_SSID "babe"
//#define WIFI_PASSWORD "babe2509"
//For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "AIzaSyA6MFI7sd9MSxkPI3T868jnoDjtHcMXZ7I"

/* 3. Define the RTDB URL */
#define DATABASE_URL "rain-meter-5d6eb-default-rtdb.asia-southeast1.firebasedatabase.app" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "cpe.rainmeter@gmail.com"
#define USER_PASSWORD "cpe123456"

#define LINE_TOKEN  "f0E3HB6o7EubJvIVNw8AUC8QpoOeJn7H8sD3mUYm5K1"

//  Rtc DS3231
#include <Wire.h>
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
#define countof(a) (sizeof(a) / sizeof(a[0]))

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* ssid = "Rain Meter (Setting)";
const char* password = "11111111";

//boolean pordMode = true;
boolean pordMode = false;

AutoConnect      Portal;
AutoConnectConfig Config(ssid, password);

unsigned long currentMillis, previousMillis1, previousMillis2, previousMillis3, previousMillis4, previousMillis5, previousMillis6, previousMillis7 = 0;
// Task 1 : Push data
// Task 2 : Delete Data 7 day ago
// Task 3 : Handle WiFi
// Task 4 : Debounce Interrupt
// Task 5 :
// Task 6 : Reset cooldwon water
// Task 7 : Debounce Line notify
int t3Interval = 2000, t4Interval = 200, t5Interval = 25000;

int t1Interval = pordMode ? 60000 * 15 : 15000;
int t2Interval = pordMode ? 60000 * 60 : 10000;
int t6Interval = pordMode ? 60000 : 1000;
int t7Interval = pordMode ? 60000 * 20 : 20000;

char dateTimestring[20];
char datestring[11];
String dateTimeNow = "";

uint8_t countWaterPin = D3;
volatile int waterCount = 0;
bool counted = false;
const float amountOfWater = 2.794;
const int waterLimit = 90;

const int cooldownReset = 20;
int previousWaterCount = 0;
int duplicateCount = 0;
boolean sendLineNotify = true;

int day = 0;
int month = 0;
int year = 0;

File indexFile;
File dataFile;
const int sdCardPin = D4;

void setup() {
  //  ปี เดือน วัน ชั่วโมง นาที วินาที
  //  สำหรับตั้งเวลา
  //  RtcDateTime compiled = RtcDateTime(2021, 12, 11, 17, 19, 00);
  //  Rtc.SetDateTime(compiled);
  delay(1000);
  Serial.begin(115200);

  pinMode(countWaterPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(countWaterPin), countWater, FALLING);

  if (!SD.begin(sdCardPin)) {
    Serial.println("initialization SD Card failed !");
    return;
  }

  Rtc.Begin();
  RtcDateTime now = Rtc.GetDateTime();
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  // กำหนด Line Token
  LINE.setToken(LINE_TOKEN);

  Serial.println("Starting.....  X)");
  Config.autoReconnect = true;    // Attempt automatic reconnection.
  Config.reconnectInterval = 1;   // Seek interval time is 180[s].
  digitalWrite(LED_BUILTIN, LOW);
  //  Config.ticker = true;
  //  Config.tickerPort = 2;
  //  Config.tickerOn = LOW;
  Portal.config(Config);
  AutoConnectConfig(ssid, password);
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    digitalWrite(LED_BUILTIN, HIGH);
    LINE.notifySticker("เครื่องวัดปริมาณน้ำฝนออนไลน์แล้ว !", 446, 1993);
    //    Config.ticker = false;
  }

  //  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //  Serial.print("Connecting to Wi-Fi");
  //  while (WiFi.status() != WL_CONNECTED)
  //  {
  //    Serial.print(".");
  //    delay(300);
  //  }
  //  Serial.println();
  //  Serial.print("Connected with IP: ");
  //  Serial.println(WiFi.localIP());
  //  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  //Or use legacy authenticate method
  //config.database_url = DATABASE_URL;
  //config.signer.tokens.legacy_token = "<database secret>";

  //////////////////////////////////////////////////////////////////////////////////////////////
  //Please make sure the device free Heap is not lower than 80 k for ESP32 and 10 k for ESP8266,
  //otherwise the SSL connection will fail.
  //////////////////////////////////////////////////////////////////////////////////////////////

  Firebase.begin(&config, &auth);

  //Comment or pass false value when WiFi reconnection will control by your code or third party library
  Firebase.reconnectWiFi(false);
  Firebase.setDoubleDigits(5);
}

void t1Callback() {
  RtcDateTime now = Rtc.GetDateTime();
  //  Serial.println(now);
  //  Serial.printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, "/test/bool", count % 2 == 0) ? "ok" : fbdo.errorReason().c_str());
  //  Serial.printf("Get bool... %s\n", Firebase.RTDB.getBool(&fbdo, "/test/bool") ? fbdo.to<bool>() ? "true" : "false" : fbdo.errorReason().c_str());
  //  bool bVal;
  //  Serial.printf("Get bool ref... %s\n", Firebase.RTDB.getBool(&fbdo, "/test/bool", &bVal) ? bVal ? "true" : "false" : fbdo.errorReason().c_str());
  //  Serial.printf("Set int... %s\n", Firebase.RTDB.setInt(&fbdo, "/test/int", count) ? "ok" : fbdo.errorReason().c_str());
  //  Serial.printf("Get int... %s\n", Firebase.RTDB.getInt(&fbdo, "/test/int") ? String(fbdo.to<int>()).c_str() : fbdo.errorReason().c_str());
  FirebaseJson json;
  //  if (count == 0)
  //  {

  dateTimeNow = printDateTime(now);
  json.set("water", waterCount * amountOfWater);
  json.set("time", dateTimeNow);
  //  json.set("time/.sv", "timestamp");

  //  Serial.printf("Set json... %s\n", Firebase.RTDB.set(&fbdo, "/testset", &json) ? "ok" : fbdo.errorReason().c_str());
  //  Firebase.RTDB.set(&fbdo, "/testset", &json);
  Firebase.RTDB.set(&fbdo, "/rainmeter/" + printDate(now) + "/" + dateTimeNow, &json);
  //  Firebase.RTDB.pushJSON(&fbdo, "/" + printDate(now), &json);
  //  Serial.println(Firebase.RTDB.pushJSON(&fbdo, "/" + printDateTime(now), &json));
  //  Serial.printf("Push data with timestamp... %s\n", Firebase.RTDB.pushJSON(&fbdo, "/" + printDateTime(now), &json) ? "ok" : fbdo.errorReason().c_str());

  //  Firebase.RTDB.deleteNode(&fbdo, "/test/append");


  //  }
  //  else
  //  {
  //  json.add(String(count), "smart!");
  //  Serial.printf("Update node... %s\n", Firebase.RTDB.updateNode(&fbdo, "/test/json/value/round", &json) ? "ok" : fbdo.errorReason().c_str());
  //  }
  if (waterCount * amountOfWater >= waterLimit && sendLineNotify) {
    //        LINE.notify("ปริมาณน้ำฝนเกิน 90 มิลลิเมตรแล้ว !");
    LINE.notifySticker("ปริมาณน้ำฝนเกิน 90 มิลลิเมตรแล้ว !", 11538, 51626522);
    sendLineNotify = false;
  }
  Serial.println();
}

void t2Callback() {
  RtcDateTime now = Rtc.GetDateTime();
  //  int tmpDay = 1;
  //  int tmpMonth = 1;
  //  int tmpYear = 2022;
  //  day = tmpDay;
  //  month = tmpMonth;
  //  year = tmpYear;
  day = now.Day();
  month = now.Month();
  year = now.Year();
  if (day > 7) {
    day -= 7;
  } else {
    switch (day) {
      case 7:
        day = 31;
        break;
      case 6:
        day = 30;
        break;
      case 5:
        day = 29;
        break;
      case 4:
        day = 28;
        break;
      case 3:
        day = 27;
        break;
      case 2:
        day = 26;
        break;
      case 1:
        day = 25;
        break;
      default:
        day = 0;
        break;
    }
    month -= 1;
    if (month <= 0) {
      month = 12;
      year -= 1;
    }
  }
  //  Serial.println("Delete Input : " + String(tmpDay) + "-" + String(tmpMonth) + "-" + String(tmpYear));
  //  Serial.println("Delete Input : " + String(now.Day()) + "-" + String(now.Month()) + "-" + String(now.Year()));
  Serial.println("Delete Output : " + String(day) + "-" + String(month) + "-" + String(year));
  Firebase.RTDB.deleteNode(&fbdo, "/rainmeter/" + String(day) + "-" + String(month) + "-" + String(year));
}

void t3Callback() {
  Serial.print("water : ");
  Serial.println(waterCount * amountOfWater);
  Portal.handleClient();
  //  if (WiFi.status() == WL_CONNECTED) {
  //    Serial.println(printDateTime(now) + "Im connected...  :)");
  //  } else {
  //    Serial.println(printDateTime(now) + "Im not connected...  :(");
  //  }
  //  Serial.println(waterCount);}
}

void t4Callback() {
  counted = false;
}

void t5Callback() {

}

void t6Callback() {
  if (previousWaterCount == waterCount) {
    duplicateCount++;
  } else {
    duplicateCount = 0;
  }
  if (duplicateCount >= cooldownReset) {
    waterCount = 0;
  }
  previousWaterCount = waterCount;
}

void t7Callback() {
  sendLineNotify = true;
}

void loop()
{
  currentMillis = millis();
  if ((currentMillis - previousMillis1 >= t1Interval)) {
    Serial.println("T1 Run : ");
    if (WiFi.status() == WL_CONNECTED && Firebase.ready() && previousMillis1 >= 5000) {
      t1Callback();
    }
    previousMillis1 = millis();
  }
  if (currentMillis - previousMillis2 >= t2Interval) {
    Serial.println("T2 Run : ");
    t2Callback();
    previousMillis2 = millis();
  }
  if (currentMillis - previousMillis3 >= t3Interval) {
    Serial.println("T3 Run : ");
    t3Callback();
    previousMillis3 = millis();
  }
  if (currentMillis - previousMillis4 >= t4Interval) {
    Serial.println("T4 Run : ");
    t4Callback();
    previousMillis4 = millis();
  }
  if (currentMillis - previousMillis5 >= t5Interval) {
    Serial.println("T5 Run : ");
    t5Callback();
    previousMillis5 = millis();
  }
  if (currentMillis - previousMillis6 >= t6Interval) {
    Serial.println("T6 Run : ");
    t6Callback();
    previousMillis6 = millis();
  }
  if (currentMillis - previousMillis7 >= t7Interval) {
    Serial.println("T7 Run : ");
    t7Callback();
    previousMillis7 = millis();
  }
}



String printDateTime(const RtcDateTime & dt)
{
  snprintf_P(dateTimestring, countof(dateTimestring), PSTR("%02u-%02u-%04u %02u:%02u:%02u"), dt.Day(), dt.Month(), dt.Year(), dt.Hour(), dt.Minute(), dt.Second());
  return dateTimestring;
}

String printDate(const RtcDateTime & dt)
{
  snprintf_P(datestring, countof(datestring), PSTR("%02u-%02u-%04u"), dt.Day(), dt.Month(), dt.Year());
  return datestring;
}

void ICACHE_RAM_ATTR countWater() {
  if (!counted) {
    waterCount++;
    counted = true;
    Serial.print("water count : ");
    Serial.println(waterCount);
  }
}