#include <Nextion.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <virtuabotixRTC.h>
#include <SoftwareSerial.h>
#include <ESP_Mail_Client.h>

const char *ssid = "";      //Wifi Adı
const char *password = "";  //Wifi Şifresi
const char *apiKey = "";    // Api Anahtarı
#define SMTP_server "smtp.gmail.com"
#define SMTP_Port 465
#define sender_email ""     // Gönderici Gmail
#define sender_password ""  // Gönderici Gmail Şifresi
#define Recipient_email ""  // Alıcı Mail
#define Recipient_name ""   // Alıcı Adı
bool emailSent = false;
SMTPSession smtp;
float latitude = 0;
float longitude = 0;
static const uint32_t GPSBaud = 9600;
static const int RXPin = 25, TXPin = 26;
SoftwareSerial ss(RXPin, TXPin);
TinyGPSPlus gps;
const int forecastDays = 3;
const int nextionSerialTx = 17;
const int nextionSerialRx = 16;
bool page9Displayed = false;
bool page1Displayed = false;
unsigned long dailyWeatherInterval = 5000;
unsigned long airPollutionInterval = 3000;
unsigned long timeInterval = 1000;
unsigned long hourlyWeatherInterval = 5000;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 100;

NexDSButton bt0 = NexDSButton(11, 7, "bt0");
NexNumber numara = NexNumber(11, 4, "numara");
NexDSButton k = NexDSButton(11, 2, "k");
NexDSButton b = NexDSButton(11, 8, "b");
NexDSButton kb = NexDSButton(11, 9, "kb");
NexDSButton bk = NexDSButton(11, 10, "bk");

#define RELAY_PIN 5

NexDSButton buttons[] = {
  NexDSButton(11, 2, "k"),
  NexDSButton(11, 8, "b"),
  NexDSButton(11, 9, "kb"),
  NexDSButton(11, 10, "bk")
};

const char *buttonNames[] = { "k", "b", "kb", "bk" };

void kPopCallback(void *ptr) {
  handleDualStateButton(0, '<');
}

void bPopCallback(void *ptr) {
  handleDualStateButton(1, '>');
}

void kbPopCallback(void *ptr) {
  handleDualStateButton(2, '<=');
}

void bkPopCallback(void *ptr) {
  handleDualStateButton(3, '>=');
}

void handleDualStateButton(int buttonIndex, char comparisonOperator) {
  Serial.print("Dual-state button ");
  Serial.print(buttonNames[buttonIndex]);
  Serial.print(" Pressed, Value: ");

  uint32_t dualStateValue;
  buttons[buttonIndex].getValue(&dualStateValue);
  Serial.print(dualStateValue);
  Serial.println();

  uint32_t maxTempThresholdValue;
  numara.getValue(&maxTempThresholdValue);
  float maxTempThreshold = static_cast<float>(maxTempThresholdValue);

  float maxTemp = 20.0;

  switch (comparisonOperator) {
    case '<':
      if (maxTemp < maxTempThreshold) {
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        digitalWrite(RELAY_PIN, LOW);
      }
      break;
    case '>':
      if (maxTemp > maxTempThreshold) {
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        digitalWrite(RELAY_PIN, LOW);
      }
      break;
    case '<=':
      if (maxTemp <= maxTempThreshold) {
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        digitalWrite(RELAY_PIN, LOW);
      }
      break;
    case '>=':
      if (maxTemp >= maxTempThreshold) {
        digitalWrite(RELAY_PIN, HIGH);
      } else {
        digitalWrite(RELAY_PIN, LOW);
      }
      break;
    default:

      break;
  }
}

char buffer[100] = { 0 };

NexTouch *nex_listen_list[] = {
  &bt0,
  NULL
};

void bt0PopCallback(void *ptr) {
  Serial.println("Button Pressed");
  uint32_t dual_state;
  memset(buffer, 0, sizeof(buffer));

  // Tek düğmenin durumunu oku
  bt0.getValue(&dual_state);

  // Röleyi kontrol et
  digitalWrite(RELAY_PIN, dual_state ? HIGH : LOW);
}

void updateIntervals(unsigned long duration, unsigned long &interval) {

  const float updateFactor = 0.9;  // Güncelleme oranını ayarlamak için bir faktör

  interval = updateFactor * interval + (1 - updateFactor) * duration;
}

unsigned long lastTime = 0;
unsigned long lastDailyWeatherTime = 0;
unsigned long lastHourlyWeatherTime = 0;
unsigned long lastAirPollutionTime = 0;

int conditionCounter = 0;

static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec) {
  if (!valid) {
    while (len-- > 1)
      Serial.print(' ');
    Serial.print(' ');
  } else {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1);
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3
                           : vi >= 10  ? 2
                                       : 1;
    for (int i = flen; i < len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len) {
  char sz[32] = "";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i = strlen(sz); i < len; ++i)
    sz[i] = ' ';
  if (len > 0)
    sz[len - 1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printStr(const char *str, int len) {
  int slen = strlen(str);
  for (int i = 0; i < len; ++i)
    Serial.print(i < slen ? str[i] : ' ');
  smartDelay(0);
}

virtuabotixRTC myRTC(18, 19, 21);  // clk 18, dat 19, rst 21

const char *monthNames[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayis", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralik" };

void readGPSTime() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      if (gps.time.isValid()) {
        int hour = gps.time.hour();
        int minute = gps.time.minute();
        int second = gps.time.second();

        myRTC.setDS1302Time(second, minute, hour, myRTC.dayofweek, myRTC.dayofmonth, myRTC.month, myRTC.year);
      }
    }
  }
}

void sendEmail(const char *subject, const char *htmlContent) {
  // E-posta gönderme işlemleri buraya eklenir
  ESP_Mail_Session session;
  session.server.host_name = SMTP_server;
  session.server.port = SMTP_Port;
  session.login.email = sender_email;
  session.login.password = sender_password;
  session.login.user_domain = "";

  // Tarih bilgisini al
  char dateStr[20];
  snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", myRTC.dayofmonth, myRTC.month, myRTC.year);

  // Konu başlığını oluştur
  char fullSubject[100];
  snprintf(fullSubject, sizeof(fullSubject), "%s - %s", subject, dateStr);

  // E-posta mesajı oluşturulur
  SMTP_Message message;
  message.sender.name = "Hava Durumu Uyarı";
  message.sender.email = sender_email;
  message.subject = fullSubject;
  message.addRecipient(Recipient_name, Recipient_email);

  // HTML içeriği ve diğer ayarlar atanır
  message.html.content = htmlContent;
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  // E-posta gönderme işlemi
  if (!smtp.connect(&session))
    return;

  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}

void sendEmailBasedOnWeatherIcon(int weatherIcon) {
  // Check if the email has already been sent
  if (emailSent) {
    return;
  }

  const char *subject;
  const char *htmlContent;

  switch (weatherIcon) {
    case 4:
      subject = "Yağmurlu Hava";
      htmlContent = "<div style=\"color:#000000;\"><h1 style=\"color: red;\">Yağmurlu Hava Uyarısı</h1><p>Yağmurlu hava için hazırlıklı olun! <img src=\"https://basmilius.github.io/weather-icons/production/fill/all/rain.svg\" alt=\"Yağmurlu Hava İkonu\"></p></div>";
      break;
    case 5:
      subject = "Hafif Yağışlı Hava";
      htmlContent = "<div style=\"color:#000000;\"><h1 style=\"color: red;\">Hafif Yağış Uyarısı</h1><p>Bugün hafif yağış bekleniyor. <img src=\"https://basmilius.github.io/weather-icons/production/fill/all/drizzle.svg\" alt=\"Hafif Yağış İkonu\"></p></div>";
      break;
    case 6:
      subject = "Gökgürültülü Yağışlı Hava";
      htmlContent = "<div style=\"color:#000000;\"><h1 style=\"color: red;\">Gökgürültüsü Uyarısı</h1><p>Dikkatli olun, gökgürültülü yağmurlar bekleniyor. <img src=\"https://basmilius.github.io/weather-icons/production/fill/all/thunderstorms-rain.svg\" alt=\"Gökgürültülü Yağış İkonu\"></p></div>";
      break;
    case 7:
      subject = "Kar Yağışlı Hava";
      htmlContent = "<div style=\"color:#000000;\"><h1 style=\"color: red;\">Kar Yağışı Uyarısı</h1><p>Bölgenizde kar yağışına hazırlıklı olun. <img src=\"https://basmilius.github.io/weather-icons/production/fill/all/snow.svg\" alt=\"Kar Yağışı İkonu\"></p></div>";
      break;
    default:
      return;
  }

  // Send the email
  sendEmail(subject, htmlContent);

  // Update the flag to indicate that the email has been sent
  emailSent = true;

  char mailTxtCommand[50];
  sprintf(mailTxtCommand, "mail.pic=32");
  sendNextionCommand("", mailTxtCommand);
}

void getAirPollutionData() {
  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/air_pollution?lat=" + String(latitude) + "&lon=" + String(longitude) + "&appid=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    DynamicJsonDocument doc(25000);
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      processAirPollutionData(doc);
    } else {
      Serial.println("Error parsing air pollution data");
    }
  } else {
    Serial.println("Error on air pollution HTTP request");
    char httpTxtCommand[50];
    sprintf(httpTxtCommand, "http.txt=\"Error on air pollution HTTP request\"");
    sendNextionCommand("", httpTxtCommand);
  }

  http.end();
}

void processAirPollutionData(JsonDocument &doc) {
  int aqi = doc["list"][0]["main"]["aqi"];

  const char *airQuality;
  uint32_t textColor;

  switch (aqi) {
    case 1:
      airQuality = "İyi";
      textColor = 2016;
      break;
    case 2:
      airQuality = "Orta";
      textColor = 65504;
      break;
    case 3:
      airQuality = "Kötü";
      textColor = 64512;
      break;
    case 4:
      airQuality = "Çok Kötü";
      textColor = 63488;
      break;
    case 5:
      airQuality = "Tehlikeli";
      textColor = 32799;
      break;
    default:
      airQuality = "Bilinmiyor";
      textColor = 65535;
      break;
  }

  char aqiCommand[50];
  sprintf(aqiCommand, "kayan.txt=\"Hava Kalitesi %s\"", airQuality);
  sendNextionCommand("", aqiCommand);

  char textColorCommand[50];
  sprintf(textColorCommand, "kayan.pco=%u", textColor);
  sendNextionCommand("", textColorCommand);
}

void processHourlyWeatherData(JsonDocument &doc, int index) {
  float temperature = doc["list"][index]["main"]["temp"];
  int humidity = doc["list"][index]["main"]["humidity"];
  const char *description = doc["list"][index]["weather"][0]["description"];
  const char *dt_txt = doc["list"][index]["dt_txt"];

  char tempCommand[50];
  sprintf(tempCommand, "%d.txt=\"%2.1f\"", index + 1, temperature);
  sendNextionCommand("stemp", tempCommand);

  char humidityCommand[50];
  sprintf(humidityCommand, "%d.txt=\"%d\"", index + 1, humidity);
  sendNextionCommand("snem", humidityCommand);

  char descriptionCommand[50];
  sprintf(descriptionCommand, "%d.txt=\"%s\"", index + 1, description);
  sendNextionCommand("durum", descriptionCommand);

  char dtTxtCommand[50];
  sprintf(dtTxtCommand, "%d.txt=\"%s\"", index + 1, dt_txt);
  sendNextionCommand("starih", dtTxtCommand);
}

void printWeatherIcon(int weatherId, int day) {
  int weatherIcon = determineWeatherIcon(weatherId);
  char dayIconCommand[50];
  sprintf(dayIconCommand, "%d.pic=%d", day + 1, weatherIcon);
  sendNextionCommand("picWeather", dayIconCommand);

  if (weatherIcon >= 4 && weatherIcon <= 7) {
    char bildirimCommand[50];
    sprintf(bildirimCommand, "bildirim.pic=9");
    sendNextionCommand("", bildirimCommand);
  }
}
void sendNextionCommand2(const char *component, const char *command) {
  Serial.print("Sending command to Nextion: ");
  Serial.print(component);
  Serial.print(".");
  Serial.print(command);
  Serial.println();

  Serial2.print(component);
  Serial2.print(".");
  Serial2.print(command);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}

void sendNextionCommand(const char *command, const char *dayCommand) {
  Serial.print("Sending command to Nextion: ");
  Serial.print(command);
  Serial.println(dayCommand);

  Serial2.print(command);
  Serial2.print(dayCommand);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}

void updateGoogleMapsLink() {
  char mapsLinkCommand[100];
  sprintf(mapsLinkCommand, "qr.txt=\"https://maps.google.com/maps?q=%.6f,%.6f\"", latitude, longitude);
  sendNextionCommand("", mapsLinkCommand);
}

void printGPSDataToNextion() {
  char satellitesCommand[50];
  sprintf(satellitesCommand, "uydu.txt=\"%d\"", gps.satellites.value());
  sendNextionCommand("", satellitesCommand);

  float hdopValue = gps.hdop.hdop();
  char hdopValueStr[10];
  dtostrf(hdopValue, 4, 1, hdopValueStr);

  sendNextionCommand("hdop.val=", hdopValueStr);

  uint16_t textColor;
  if (hdopValue >= 0 && hdopValue < 2) {
    textColor = 2016;
  } else if (hdopValue >= 2 && hdopValue < 5) {
    textColor = 65504;
  } else if (hdopValue >= 5 && hdopValue < 10) {
    textColor = 64512;
  } else {
    textColor = 63488;
  }

  char hdopCommand[50];

  if (hdopValue >= 0 && hdopValue < 2) {
    sprintf(hdopCommand, "hdopy.txt=\"Mükemmel Doğruluk\"");
  } else if (hdopValue >= 2 && hdopValue < 5) {
    sprintf(hdopCommand, "hdopy.txt=\"İyi Doğruluk\"");
  } else if (hdopValue >= 5 && hdopValue < 10) {
    sprintf(hdopCommand, "hdopy.txt=\"Orta Doğruluk\"");
  } else {
    sprintf(hdopCommand, "hdopy.txt=\"Düşük Doğruluk\"");
  }

  sendNextionCommand("", hdopCommand);
  sprintf(hdopCommand, "hdopy.pco=%u", textColor);
  sendNextionCommand("", hdopCommand);

  sprintf(hdopCommand, "hdop.txt.pco=%u", textColor);
  sendNextionCommand("", hdopCommand);


  char latitudeCommand[50];
  sprintf(latitudeCommand, "enlem.txt=\"%2.6f\"", gps.location.lat());
  sendNextionCommand("", latitudeCommand);

  char longitudeCommand[50];
  sprintf(longitudeCommand, "boylam.txt=\"%2.6f\"", gps.location.lng());
  sendNextionCommand("", longitudeCommand);

  char altitudeCommand[50];
  sprintf(altitudeCommand, "rakim.txt=\"%2.2f\"", gps.altitude.meters());
  sendNextionCommand("", altitudeCommand);
}

int determineWeatherIcon(int weatherId) {
  if (weatherId >= 200 && weatherId <= 232) {
    return 6;  // Gökgürültülü
  } else if (weatherId >= 300 && weatherId <= 321) {
    return 5;  // Hafif Yağmur
  } else if (weatherId >= 500 && weatherId <= 531) {
    return 4;  // Yağmur
  } else if (weatherId >= 600 && weatherId <= 622) {
    return 7;  // Kar
  } else if (weatherId >= 701 && weatherId <= 781) {
    return 8;  // Sis
  } else if (weatherId == 800) {
    return 0;  // Açık
  } else if (weatherId >= 801 && weatherId <= 804) {
    if (weatherId == 801) {
      return 2;  // Az bulutlu
    } else if (weatherId == 802) {
      return 1;  // Parçalı bulutlu
    } else {
      return 3;  // Bulutlu
    }
  } else {
    return 1;  // Varsayılan
  }
}

void printDateFromTimestamp(unsigned long timestamp, int day) {
  struct tm *timeinfo;
  time_t rawtime = timestamp;
  timeinfo = localtime(&rawtime);

  if (timeinfo != NULL) {
    char dateCommand[50];
    sprintf(dateCommand, "%d.txt=\"%02d %s %d\"", day + 1, timeinfo->tm_mday, monthNames[timeinfo->tm_mon], 1900 + timeinfo->tm_year);
    sendNextionCommand("date", dateCommand);
  } else {
    Serial.println("Error converting timestamp to date");
  }
}

void sendWeatherDataToNextion(float maxTemp, float minTemp, int humidity, int day) {
  char maxTempCommand[50];
  sprintf(maxTempCommand, "%d.txt=\"%2.1f\"", day + 1, maxTemp);
  sendNextionCommand("maxTemp", maxTempCommand);

  char minTempCommand[50];
  sprintf(minTempCommand, "%d.txt=\"%2.1f\"", day + 1, minTemp);
  sendNextionCommand("minTemp", minTempCommand);

  char humidityCommand[50];
  sprintf(humidityCommand, "%d.txt=\"%d\"", day + 1, humidity);
  sendNextionCommand("humidity", humidityCommand);
}

void sendCityCountryToNextion(const char *cityName, const char *countryName) {
  char cityCommand[50];
  sprintf(cityCommand, "sehir.txt=\"%s\"", cityName);
  sendNextionCommand("", cityCommand);

  char countryCommand[50];
  sprintf(countryCommand, "ulke.txt=\"%s\"", countryName);
  sendNextionCommand("", countryCommand);
}

void zamanVerisiGuncelle() {
  readGPSTime();
  myRTC.updateTime();

  int currentYear = myRTC.year;
  int currentMonth = myRTC.month;
  int currentDay = myRTC.dayofmonth;
  int currentHour = myRTC.hours;
  int currentMinute = myRTC.minutes;
  int currentSecond = myRTC.seconds;

  char dateTimeCommand[50];
  sprintf(dateTimeCommand, "time.txt=\"  %02d:%02d:%02d   %02d/%02d/%d\"", currentHour, currentMinute, currentSecond, currentDay, currentMonth, currentYear);
  sendNextionCommand("", dateTimeCommand);
}

void handlePage(int pageNumber, bool &pageDisplayed) {
  if (!pageDisplayed) {
    while (!gps.location.isValid()) {
      smartDelay(1000);
      if (millis() > 5000 && gps.charsProcessed() < 10) {
        Serial.println(F("GPS verisi alınamadı: Bağlantıları kontrol edin"));
      }
    }

    printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
    printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
    printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);

    latitude = gps.location.lat();
    longitude = gps.location.lng();

    Serial.println();

    smartDelay(1000);

    char nextPageCommand[50];
    sprintf(nextPageCommand, "page %d", pageNumber);
    sendNextionCommand("", nextPageCommand);

    pageDisplayed = true;
  }
}

void setup() {
  Serial.begin(9600);
  ss.begin(GPSBaud);
  Serial2.begin(9600, SERIAL_8N1, nextionSerialRx, nextionSerialTx);
  // saniye, dakika, saat, haftanın günü, gün, ay, yıl
  //myRTC.setDS1302Time(0, 4, 13, 4, 23, 1, 2024); //Saat Senkronize etme

  nexInit();

  bt0.attachPop(bt0PopCallback, &bt0);

  pinMode(RELAY_PIN, OUTPUT);

  unsigned long wifiConnectStartTime = millis();
  bool connectedToWiFi = false;

  WiFi.begin(ssid, password);

  while (millis() - wifiConnectStartTime < 30000) {
    if (WiFi.status() == WL_CONNECTED) {
      connectedToWiFi = true;
      break;
    }
    delay(1000);
    Serial.println("Connecting to WiFi...");

    unsigned long page7StartTime = millis();
    while (millis() - page7StartTime < 2000) {
      char nextPageCommand[50];
      sprintf(nextPageCommand, "page 0");
      sendNextionCommand("", nextPageCommand);
      delay(100);
    }
  }

  if (connectedToWiFi) {
    Serial.println("Connected to WiFi");

    unsigned long page8StartTime = millis();
    while (millis() - page8StartTime < 2000) {
      char nextPageCommand[50];
      sprintf(nextPageCommand, "page 8");
      sendNextionCommand("", nextPageCommand);
      delay(100);
    }

  } else {
    Serial.println("Failed to connect to WiFi");
    char nextPageCommand[50];
    sprintf(nextPageCommand, "page 10");
    sendNextionCommand("", nextPageCommand);
    return;
  }
}

void loop() {
  // Sayfa yönetim işlemleri
  handlePage(9, page9Displayed);
  handlePage(1, page1Displayed);
  // Zaman ölçümleri
  unsigned long currentTime = millis();
  unsigned long loopStartTime = currentTime;

  // Ana güncelleme işlemleri
  if (currentTime - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentTime;
    zamanVerisiGuncelle();
    printGPSDataToNextion();
    updateGoogleMapsLink();
  }

  // Günlük hava durumu güncellemesi
  if (currentTime - lastDailyWeatherTime >= dailyWeatherInterval) {
    unsigned long dailyWeatherStartTime = millis();

    lastDailyWeatherTime = currentTime;

    HTTPClient http;
    String url = "https://api.openweathermap.org/data/2.5/forecast/daily?lat=" + String(latitude) + "&lon=" + String(longitude) + "&cnt=" + String(forecastDays) + "&lang=tr&units=metric&appid=" + String(apiKey);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, http.getString());

      if (!error) {
        for (int i = 0; i < forecastDays; ++i) {
          unsigned long timestamp = doc["list"][i]["dt"].as<unsigned long>();
          printDateFromTimestamp(timestamp, i);

          float maxTemp = doc["list"][i]["temp"]["max"];
          float minTemp = doc["list"][i]["temp"]["min"];
          int humidity = doc["list"][i]["humidity"];
          int weatherId = doc["list"][i]["weather"][0]["id"];
          const char *cityName = doc["city"]["name"];
          const char *countryName = doc["city"]["country"];

          sendWeatherDataToNextion(maxTemp, minTemp, humidity, i);

          sendCityCountryToNextion(cityName, countryName);

          printWeatherIcon(weatherId, i);

          // Hava durumu ikonu ile ilgili işlemler
          int weatherIcon = determineWeatherIcon(weatherId);

          // Hava durumu ikonuna göre e-posta gönderme işlemi yapılır
          sendEmailBasedOnWeatherIcon(weatherIcon);

          // Belirli bir koşulda bildirim gönderme
          if (weatherIcon >= 4 && weatherIcon <= 7 && conditionCounter == 0) {
            char bildirimCommand[50];
            sprintf(bildirimCommand, "bildirim.pic=9");
            sendNextionCommand("", bildirimCommand);

            conditionCounter++;

            char yaziCommand[50];
            sprintf(yaziCommand, "yazi.txt=\"%d\"", conditionCounter);
            sendNextionCommand("", yaziCommand);
          }
        }
      }
    } else {
      // Hata durumunda sayfayı değiştirme
      Serial.println("Error on daily HTTP request");
      char nextPageCommand[50];
      sprintf(nextPageCommand, "page 16");
      sendNextionCommand("", nextPageCommand);
      char httpTxtCommand[50];
      sprintf(httpTxtCommand, "http.txt=\"Error on daily HTTP request\"");
      sendNextionCommand("", httpTxtCommand);
    }

    http.end();

    unsigned long dailyWeatherEndTime = millis();
    Serial.print("Daily Weather Duration: ");
    Serial.println(dailyWeatherEndTime - dailyWeatherStartTime);
    updateIntervals(millis() - dailyWeatherStartTime, dailyWeatherInterval);
  }

  // Hava kirliliği verilerinin güncellenmesi
  if (currentTime - lastAirPollutionTime >= airPollutionInterval) {
    unsigned long airPollutionStartTime = millis();

    lastAirPollutionTime = currentTime;

    getAirPollutionData();

    unsigned long airPollutionEndTime = millis();
    Serial.print("Air Pollution Duration: ");
    Serial.println(airPollutionEndTime - airPollutionStartTime);
    updateIntervals(millis() - airPollutionStartTime, airPollutionInterval);
  }

  // Saatlik hava durumu güncellemesi
  if (currentTime - lastHourlyWeatherTime >= hourlyWeatherInterval) {
    unsigned long hourlyWeatherStartTime = millis();

    lastHourlyWeatherTime = currentTime;

    HTTPClient httpHourly;
    String urlHourly = "https://pro.openweathermap.org/data/2.5/forecast/hourly?lat=" + String(latitude) + "&lon=" + String(longitude) + "&appid=" + String(apiKey) + "&mode=json&lang=tr&units=metric";

    httpHourly.begin(urlHourly);
    int httpCodeHourly = httpHourly.GET();

    if (httpCodeHourly > 0) {
      DynamicJsonDocument docHourly(50000);
      DeserializationError errorHourly = deserializeJson(docHourly, httpHourly.getString());

      if (!errorHourly) {
        for (int i = 0; i < 3; ++i) {
          for (int j = 0; j < 24; ++j) {
            int index = i * 24 + j;
            processHourlyWeatherData(docHourly, index);
          }
        }
      }
    } else {
      // Hata durumunda sayfayı değiştirme
      Serial.println("Error on hourly HTTP request");
      char nextPageCommand[50];
      sprintf(nextPageCommand, "page 16");
      sendNextionCommand("", nextPageCommand);
      char httpTxtCommand[50];
      sprintf(httpTxtCommand, "http.txt=\"Error on hourly HTTP request\"");
      sendNextionCommand("", httpTxtCommand);
    }

    httpHourly.end();

    unsigned long hourlyWeatherEndTime = millis();
    Serial.print("Hourly Weather Duration: ");
    Serial.println(hourlyWeatherEndTime - hourlyWeatherStartTime);
  }

  // Kontrol butonları için işlemler
  unsigned long loopEndTime = millis();
  Serial.print("Loop Duration: ");
  Serial.println(loopEndTime - loopStartTime);
  nexLoop(nex_listen_list);
  uint32_t value;
  numara.getValue(&value);
  float maxTempThreshold = static_cast<float>(value);
  for (int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    nexLoop(nex_listen_list);
    nexLoop(nex_listen_list);
    uint32_t value;
    numara.getValue(&value);
    float maxTempThreshold = static_cast<float>(value);
    if (maxTempThreshold <= 10) {
      char pcoCommand[50];
      sprintf(pcoCommand, "numara.pco=31");
      sendNextionCommand("", pcoCommand);
    } else if (maxTempThreshold > 10 && maxTempThreshold <= 25) {
      char pcoCommand[50];
      sprintf(pcoCommand, " .pco=65504");
      sendNextionCommand("", pcoCommand);
    } else {
      char pcoCommand[50];
      sprintf(pcoCommand, "numara.pco=63488");
      sendNextionCommand("", pcoCommand);
    }
  }
}