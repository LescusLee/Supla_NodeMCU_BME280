#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>

#define SUPLADEVICE_CPP
#include <SuplaDevice.h>

#include <DoubleResetDetector.h> //Bilioteka by Stephen Denne

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
extern "C" {
#include "user_interface.h"
}

#define EEPROM_SIZE           4096

#define EEPROM_SSID           32
#define EEPROM_PASSWORD       64
#define EEPROM_DHCPEN         190
#define EEPROM_ADRESS         4000
#define EEPROM_MASK           4015
#define EEPROM_GATEWAY        4030
#define EEPROM_SUPLASRV       300
#define EEPROM_SUPLALOCID     350
#define EEPROM_SUPLALOCPASS   360
#define EEPROM_HIGHTCAL       200

#define MAX_SSID           32
#define MAX_PASSWORD       32
#define MAX_DHCPEN         8
#define MAX_ADRESS         16
#define MAX_MASK           16
#define MAX_GATEWAY        16
#define MAX_SUPLASRV       32
#define MAX_SUPLALOCID     8
#define MAX_SUPLALOCPASS   8
#define MAX_HIGHTCAL       8

#define DRD_TIMEOUT 2// Number of seconds after reset during which a subseqent reset will be considered a double reset.
#define DRD_ADDRESS 0 // RTC Memory Address for the DoubleResetDetector to use
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

ADC_MODE(ADC_VCC);

String config_Wifi_name = "SUPLA-BME280";
//const char* config_Wifi_pass = "12345678";
const char* update_username = "admin";
const char* update_password = "supla";

const char* suplaDeviceName = "SUPLA-BME280";
const String SuplaDevice_setName = "SUPLA - BME280 by Yepestis v.1.0.0";


char Supla_server[40];
char Location_id[15];
char Location_Pass[20];
int aktualny_status;
String status_msg = "";
String old_status_msg = "";
int empty_start = 0;;

String esid = "";
String epass = "";

uint8_t mac[WL_MAC_ADDR_LENGTH];


word Licznik_iteracji = 0;

WiFiClient client;
const char* host = "supla";
const char* update_path = "/firmware";
String My_guid = "";
String My_mac = "";
String custom_Supla_server = "";
String Location_id_string = "";
String custom_Location_Pass = "";
bool DHCP = 0;
bool Nowy_pomiar = 0;

ESP8266WebServer httpServer(80);

String content;
String zmienna = "";


int zmienna_int = 0;

ESP8266HTTPUpdateServer httpUpdater;

byte Modul_tryb_konfiguracji = 0;

double last_temperature = -99.99;
double last_humidity = -99.99;
double last_pressure = -99.99;
double last_pressure_sea = -99.99;


//*********************************************************************************************************

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;
const float alfa = 0.9;      //default 0.9

Adafruit_BME280 bme;         //I2C

//*********************************************************************************************************

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  delay(100);
  Serial.print(" ");
  Serial.println(" ");
  Serial.print(" ");
  Serial.println(" ");
  Serial.print(" ");

  if (Odczytaj_zakres_eeprom(EEPROM_SSID, MAX_SSID)[0] == char(0xFF)) {
    Serial.println(" ");
    Serial.print("Brak skonfigurowanej sieci. Uruchamiam tryb konfiguracji");
    Serial.println(" ");
    //Serial.println(zmienna);
    empty_start = 1;
    Tryb_konfiguracji();
  }

  if (drd.detectDoubleReset()) {
    //drd.stop();
    Tryb_konfiguracji();
  }

  delay(2000);
  drd.stop();

  Pokaz_zawartosc_eeprom();

  Odczytaj_parametry_IP();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi_up();
  }

  WiFi.hostname(config_Wifi_name);
  WiFi.softAPdisconnect(true); // wyłączenie rozgłaszania sieci ESP

  // Inicjalizacja BME280
  Wire.begin(); //(1,3);                           // GPIO1 - SDA GPIO3 - SCL domyślnie 5, 4

  if (!bme.begin()) {
    Serial.println("Nie znaleleziono czujnika BME280, sprawdz poprawność podłączenia i okablowanie!");
    while (1);
  }

  WiFi.macAddress(mac);

  char GUID[SUPLA_GUID_SIZE] = {0x20, 0x18, 0x10, 0x30,
                                mac[WL_MAC_ADDR_LENGTH - 6],
                                mac[WL_MAC_ADDR_LENGTH - 5],
                                mac[WL_MAC_ADDR_LENGTH - 4],
                                mac[WL_MAC_ADDR_LENGTH - 3],
                                mac[WL_MAC_ADDR_LENGTH - 2],
                                mac[WL_MAC_ADDR_LENGTH - 1],
                                0x04, 0x12, 0x34, 0x56, 0x78, 0x91
                               };

  My_guid = "20181227-" + String(mac[WL_MAC_ADDR_LENGTH - 6], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 5], HEX) + "-" + String(mac[WL_MAC_ADDR_LENGTH - 4], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) + "-" + String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX) + "-041234567890";

  if (mac[WL_MAC_ADDR_LENGTH - 6] <= 15) {
    My_mac = "0" + String(mac[WL_MAC_ADDR_LENGTH - 6], HEX) + ":";
  } else {
    My_mac = String(mac[WL_MAC_ADDR_LENGTH - 6], HEX) + ":";
  }
  if (mac[WL_MAC_ADDR_LENGTH - 5] <= 15) {
    My_mac += "0" + String(mac[WL_MAC_ADDR_LENGTH - 5], HEX) + ":";
  } else {
    My_mac += String(mac[WL_MAC_ADDR_LENGTH - 5], HEX) + ":";
  }
  if (mac[WL_MAC_ADDR_LENGTH - 4] <= 15) {
    My_mac += "0" + String(mac[WL_MAC_ADDR_LENGTH - 4], HEX) + ":";
  } else {
    My_mac += String(mac[WL_MAC_ADDR_LENGTH - 4], HEX) + ":";
  }
  if (mac[WL_MAC_ADDR_LENGTH - 3] <= 15) {
    My_mac += "0" + String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) + ":";
  } else {
    My_mac += String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) + ":";
  }
  if (mac[WL_MAC_ADDR_LENGTH - 2] <= 15) {
    My_mac += "0" + String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + ":";
  } else {
    My_mac += String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + ":";
  }
  if (mac[WL_MAC_ADDR_LENGTH - 1] <= 15) {
    My_mac += "0" + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  } else {
    My_mac += String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  }

  //Add Supla channels
  SuplaDevice.addPressureSensor();        // channel 0 - pressure from BME280 (hPa)
  SuplaDevice.addDS18B20Thermometer();    // chaneel 1 - wifi RSSI
  SuplaDevice.addDHT22();                 // channel 2 - temperature & humidity from BME280
  SuplaDevice.addDS18B20Thermometer();    // chaneel 3 - power supply voltage ESP
  SuplaDevice.addPressureSensor();        // channel 4 - pressure from BME280 with reference to sea level (hPa)

  SuplaDevice.setPressureCallback(&get_pressure);
  SuplaDevice.setTemperatureHumidityCallback(&get_temperature_and_humidity);
  SuplaDevice.setStatusFuncImpl(&status_func);

  Read_auth_param();
  int Location_id = Location_id_string.toInt();

  if (custom_Supla_server != "") {
    strcpy(Supla_server, custom_Supla_server.c_str());
    strcpy(Location_Pass, custom_Location_Pass.c_str());

    SuplaDevice.setName(suplaDeviceName);
    SuplaDevice.begin(GUID,              // Global Unique Identifier
                      mac,               // Ethernet MAC address
                      Supla_server,      // SUPLA server address
                      Location_id,       // Location ID
                      Location_Pass);    // Location Password
  }

  Serial.println();
  Serial.println("Uruchamianie serwera aktualizacji...");
  WiFi.mode(WIFI_STA);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Brak WiFi.");
  }

  createWebServer();

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();
}

//*********************************************************************************************************

void loop() {

  delay(5);

  if (WiFi.status() != WL_CONNECTED) {
    WiFi_up();
  }

  SuplaDevice.iterate();
  SuplaDevice.setTemperatureCallback(&get_pressure);
  SuplaDevice.setTemperatureHumidityCallback(&get_temperature_and_humidity);

  ++Licznik_iteracji;

  if (Nowy_pomiar == 1) {
    SuplaDevice.iterate();
    Licznik_iteracji = 0;
    Nowy_pomiar = 0;
  }

  if (Licznik_iteracji == 100) {
    Licznik_iteracji = 0;
    SuplaDevice.iterate();

    switch (aktualny_status) {
      case 2:  status_msg = "ALREADY INITIALIZED";      break;
      case 3:  status_msg = "CB NOT ASSIGNED";          break;
      case 4:  status_msg = "INVALID GUID";             break;
      case 5:  status_msg = "UNKNOWN SERVER_ADDRESS";   break;
      case 6:  status_msg = "UNKNOWN LOCATION_ID";      break;
      case 7:  status_msg = "INITIALIZED";              break;
      case 8:  status_msg = "CHANNEL LIMIT EXCEEDED";   break;
      case 9:  status_msg = "DISCONNECTED";             break;
      case 10: status_msg = "REGISTER IN PROGRESS";     break;
      case 11: status_msg = "ITERATE FAIL";             break;
      case 12: status_msg = "PROTOCOL VERSION ERROR";   break;
      case 13: status_msg = "BAD CREDENTIALS";          break;
      case 14: status_msg = "TEMPORARILY UNAVAILABLE";  break;
      case 15: status_msg = "LOCATION CONFLICT";        break;
      case 16: status_msg = "CHANNEL CONFLICT";         break;
      case 17: status_msg = "REGISTERED AND READY";     break;
      case 18: status_msg = "DEVICE IS DISABLED ";      break;
      case 19: status_msg = "LOCATION IS DISABLED";     break;
      case 20: status_msg = "DEVICE LIMIT EXCEEDED";    break;
    }

    if (old_status_msg != status_msg) {
      Serial.println(status_msg);
      Serial.println("");
      old_status_msg = status_msg;
    }
  }

  if (Licznik_iteracji == 0 or Licznik_iteracji == 20 or Licznik_iteracji == 40 or Licznik_iteracji == 60 or Licznik_iteracji == 80) {
    httpServer.handleClient();
  }
}

//*********************************************************************************************************

// Odczyt z czujnika BME280 temperatury i wilgotności
void get_temperature_and_humidity (int channelNumber, double *temp, double *humidity) {

  // wygładzenie temperatury nie będzie gwałtownych skoków
  static double usrednionaWartoscT = bme.readTemperature();
  usrednionaWartoscT = (alfa * usrednionaWartoscT) + ((1 - alfa) * bme.readTemperature());

  *temp = usrednionaWartoscT; //- 0.5;  // - 0.5 korekta odczytu
  //*temp = bme.readTemperature() -0.5;   // bez uśredniania
  *humidity = bme.readHumidity();
  Serial.print("BME Temperatura: ");
  Serial.print(*temp);
  Serial.print("   Wilgotność : ");
  Serial.println(*humidity);
  Serial.println("");


  last_humidity = *humidity; //GUI
  last_temperature = *temp; //GUI

  if ( isnan(*temp) || isnan(*humidity) ) {
    *temp = -275;
    *humidity = -1;
  }
}

//*********************************************************************************************************

// Odczyt z czujnika BME280 ciśnienie i siły sygnału Wifi z ESP
double get_pressure(int channelNumber, double last_val) {

  double val = -275;
  double pressure = -275;
  float elevation = 0;

  switch (channelNumber)
  {
    case 0:
      elevation = Odczytaj_zakres_eeprom(EEPROM_HIGHTCAL, MAX_HIGHTCAL).toFloat();
      pressure = bme.readPressure();
      val = pressure / pow(2.718281828, -(elevation / ((273.15 + bme.readTemperature()) * 29.263))) / 100.0;
      //val = (bme.readPressure() + Odczytaj_zakres_eeprom(EEPROM_HIGHTCAL, MAX_HIGHTCAL).toFloat()) / 1000.0F; // + korekta uwzględniająca wysokość na jakiej znajduje się czujnik co do poziomu morza
      Serial.print("BME Ciśnienie zredukowane do poziomu morza: ");          
      Serial.println(val);
      last_pressure_sea = val; //GUI
      break;
    case 1:
      val = WiFi.RSSI();
      Serial.print("ESP RSSI : ");
      Serial.println(val);
      break;
    case 3:
      val = (ESP.getVcc() / 1000.0);  // power supply voltage
      Serial.print("ESP Vcc : ");
      Serial.println(val);
      break;
    case 4:
      pressure = bme.readPressure();
      val = pressure / 100.0F;  // pressure from BME280
      Serial.print("BME Ciśnienie absolutne: ");          
      Serial.println(val);
      last_pressure = val; //GUI
      break;
  };
  return val;

}

//*********************************************************************************************************

void createWebServer()
{

  String aux = "";
  String encrypr_type;

  httpServer.on("/", []() {
    content = "<!DOCTYPE HTML>";
    content += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>";
    content += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
    content += "<style>body{font-size:14px;font-family:HelveticaNeue,'Helvetica Neue',HelveticaNeueRoman,HelveticaNeue-Roman,'Helvetica Neue Roman',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:400;font-stretch:normal;background:#00d151;color:#fff;line-height:20px;padding:0}.s{width:460px;margin:0 auto;margin-top:calc(50vh - 340px);border:solid 3px #fff;padding:0 10px 10px;border-radius:3px}#l{display:block;max-width:150px;height:155px;margin:-80px auto 20px;background:#00d151;padding-right:5px}#l path{fill:#000}.w{margin:3px 0 16px;padding:5px 0;border-radius:3px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3)}h1,h3{margin:10px 8px;font-family:HelveticaNeueLight,HelveticaNeue-Light,'Helvetica Neue Light',HelveticaNeue,'Helvetica Neue',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:300;font-stretch:normal;color:#000;font-size:23px}h1{margin-bottom:14px;color:#fff}span{display:block;margin:10px 7px 14px}i{display:block;font-style:normal;position:relative;border-bottom:solid 1px #00d151;height:42px}i:last-child{border:none}label{position:absolute;display:inline-block;top:0;left:8px;color:#00d151;line-height:41px;pointer-events:none}input,select{width:calc(100% - 145px);border:none;font-size:16px;line-height:40px;border-radius:0;letter-spacing:-.5px;background:#fff;color:#000;padding-left:144px;-webkit-appearance:none;-moz-appearance:none;appearance:none;outline:0!important;height:40px}select{padding:0;float:right;margin:1px 3px 1px 2px}button{width:100%;border:0;background:#000;padding:5px 10px;font-size:16px;line-height:40px;color:#fff;border-radius:3px;box-shadow:0 1px 3px rgba(0,0,0,.3);cursor:pointer}.c{background:#ffe836;position:fixed;width:100%;line-height:80px;color:#000;top:0;left:0;box-shadow:0 1px 3px rgba(0,0,0,.3);text-align:center;font-size:26px;z-index:100}@media all and (max-height:920px){.s{margin-top:80px}}@media all and (max-width:900px){.s{width:calc(100% - 20px);margin-top:40px;border:none;padding:0 8px;border-radius:0}#l{max-width:80px;height:auto;margin:10px auto 20px}h1,h3{font-size:19px}i{border:none;height:auto}label{display:block;margin:4px 0 12px;color:#00d151;font-size:13px;position:relative;line-height:18px}input,select{width:calc(100% - 10px);font-size:16px;line-height:28px;padding:0 5px;border-bottom:solid 1px #00d151}select{width:100%;float:none;margin:0}}</style>";
    content += "<div class='s'><svg version='1.1' id='l' x='0' y='0' viewBox='0 0 200 200' xml:space='preserve'><path d='M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1.7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22.9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0.3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1.8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2,14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1.1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-21.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,17.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8,0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2,4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22.4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188.6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,88.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z'/></svg>";

    content += "<h1><center>" + String(SuplaDevice_setName) + "</center></h1>";
    content += "<font size='2'>STATUS: " + status_msg + "</font><br>";
    content += "<font size='2'>GUID: " + My_guid + "</font><br>";
    content += "<font size='2'>MAC:  " + My_mac + "</font><br>";
    long rssi = WiFi.RSSI();            content += "<font size='2'>RSSI: " + String(rssi) + "</font>";
    content += "<form method='post' action='set0'>";

    content += "<div class='w'>";
    content += "<h3>Ustawienia WIFI</h3>";
    if (Odczytaj_zakres_eeprom(EEPROM_SSID, MAX_SSID)[0] == char(0xFF)) {
      content += "<i><input name='wifi_ssid' value=''length=32><label>Nazwa sieci</label></i>";
    }
    else {
      content += "<i><input name='wifi_ssid' value='" + String(Odczytaj_zakres_eeprom(EEPROM_SSID, MAX_SSID).c_str()) + "'length=32><label>Nazwa sieci</label></i>";
    }
    if (Odczytaj_zakres_eeprom(EEPROM_PASSWORD, MAX_PASSWORD)[0] == char(0xFF)) {
      content += "<i><input type='password' name='wifi_pass' value=''minlength='8' required length=64><label>Hasło</label></i>";
    }
    else {
      content += "<i><input type='password' name='wifi_pass' value='" + String(Odczytaj_zakres_eeprom(EEPROM_PASSWORD, MAX_PASSWORD).c_str()) + "'minlength='8' required length=64><label>Hasło</label></i>";
    }
    content += "</div>";

    content += "<div class='w'>";
    content += "<h3>Konfiguracja DHCP</h3>";
    if (String(Odczytaj_zakres_eeprom(EEPROM_DHCPEN, MAX_DHCPEN).c_str()) != "0") {
      content += "<i><select name=\"DHCP_select\">";
      content += "<option value=\"0\">NIE</option>";
      content += "<option value=\"1\" selected>TAK</option>";
      content += "</select><label>Pobierz adres</label>";
      content += "</i>";
      content += "</div>";
      content += "<div class='w' visibility: hidden>";
      content += "<h3>Adresacja IP";
      content += "</h3>";
      content += "<i><input name='ip_address' value='0.0.0.0'length=15><label>IP:</label></i>";
      content += "<i><input name='ip_mask' value='0.0.0.0'length=15><label>Maska:</label></i>";
      content += "<i><input name='ip_gateway' value='0.0.0.0'length=15><label>Brama:</label></i>";
      content += "</div>";
    }
    else {
      content += "<i><select name=\"DHCP_select\">";
      content += "<option value=\"0\" selected>NIE</option>";
      content += "<option value=\"1\">TAK</option>";
      content += "</select><label>Pobierz adres</label>";
      content += "</i>";
      content += "</div>";
      content += "<div class='w'>";
      content += "<h3>Adresacja IP";
      content += "</h3>";

      if (Odczytaj_zakres_eeprom(EEPROM_ADRESS, MAX_ADRESS)[0] == char(0xFF)) {
        content += "<i><input name='ip_address' value=''length=15><label>IP:</label></i>";
      } else {
        content += "<i><input name='ip_address' value='" + String(Odczytaj_zakres_eeprom(EEPROM_ADRESS, MAX_ADRESS).c_str()) + "'length=15><label>IP:</label></i>";
      }
      if (Odczytaj_zakres_eeprom(EEPROM_MASK, MAX_MASK)[0] == char(0xFF)) {
        content += "<i><input name='ip_mask' value=''length=15><label>Maska:</label></i>";
      } else {
        content += "<i><input name='ip_mask' value='" + String(Odczytaj_zakres_eeprom(EEPROM_MASK, MAX_MASK).c_str()) + "'length=15><label>Maska:</label></i>";
      }
      if (Odczytaj_zakres_eeprom(EEPROM_GATEWAY, MAX_GATEWAY)[0] == char(0xFF)) {
        content += "<i><input name='ip_gateway' value=''length=15><label>Brama:</label></i>";
      } else {
        content += "<i><input name='ip_gateway' value='" + String(Odczytaj_zakres_eeprom(EEPROM_GATEWAY, MAX_GATEWAY).c_str()) + "'length=15><label>Brama:</label></i>";
      }
      content += "</div>";
    }

    content += "<div class='w'>";
    content += "<h3>Ustawienia SUPLA</h3>";

    if (Odczytaj_zakres_eeprom(EEPROM_SUPLASRV, MAX_SUPLASRV)[0] == char(0xFF)) {
      content += "<i><input name='Supla_Server_ADR' value=''length=32><label>Adres serwera</label></i>";
    } else {
      content += "<i><input name='Supla_Server_ADR' value='" + String(Odczytaj_zakres_eeprom(EEPROM_SUPLASRV, MAX_SUPLASRV).c_str()) + "'length=32><label>Adres serwera</label></i>";
    }
    if (Odczytaj_zakres_eeprom(EEPROM_SUPLALOCID, MAX_SUPLALOCID)[0] == char(0xFF)) {
      content += "<i><input name='Supla_Location_ID' value=''length=10><label>ID Lokalizacji</label></i>";
    } else {
      content += "<i><input name='Supla_Location_ID' value='" + String(Odczytaj_zakres_eeprom(EEPROM_SUPLALOCID, MAX_SUPLALOCID).c_str()) + "'length=10><label>ID Lokalizacji</label></i>";
    }
    if (Odczytaj_zakres_eeprom(EEPROM_SUPLALOCPASS, MAX_SUPLALOCPASS)[0] == char(0xFF)) {
      content += "<i><input type='password' name='Supla_Location_Pass' value=''minlength='4' required length=50><label>Hasło Lokalizacji</label></i>";
    } else {
      content += "<i><input type='password' name='Supla_Location_Pass' value='" + String(Odczytaj_zakres_eeprom(EEPROM_SUPLALOCPASS, MAX_SUPLALOCPASS).c_str()) + "'minlength='4' required length=50><label>Hasło Lokalizacji</label></i>";
    }
    content += "</div>";

    content += "<div class='w'>";
    content += "<h3>Kalibracja BME280</h3>";
    if (Odczytaj_zakres_eeprom(EEPROM_HIGHTCAL, MAX_HIGHTCAL)[0] == char(0xFF)) {
      content += "<i><input name='kal_wys' value='0' type='number' min='-1000' max='9999' step='1'><label>Wysokości m.n.p.m</label></i>";
    } else {
      content += "<i><input name='kal_wys' value='" + String(Odczytaj_zakres_eeprom(EEPROM_HIGHTCAL, MAX_HIGHTCAL).c_str()) + "' type='number' min='-1000' max='9999' step='1'><label>Wysokości m.n.p.m</label></i>";
    }
    content += "</div>";

    content += "<button type='submit'>Zapisz</button></form>";
    content += "<br>";
    content += "<a href='/firmware_up'><button>Aktualizacja</button></a>";
    content += "<br><br>";
    content += "<a href='/about'><button>Info</button></a>";
    content += "<br><br>";
    content += "<form method='post' action='reboot'>";
    content += "<button type='submit'>Restart</button></form></div>";
    content += "<br><br>";

    httpServer.send(200, "text/html", content);
  });



  httpServer.on("/about", []() {
    content = "<!DOCTYPE HTML>";
    content += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>";
    content += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
    content += "<style>body{font-size:14px;font-family:HelveticaNeue,'Helvetica Neue',HelveticaNeueRoman,HelveticaNeue-Roman,'Helvetica Neue Roman',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:400;font-stretch:normal;background:#00d151;color:#fff;line-height:20px;padding:0}.s{width:460px;margin:0 auto;margin-top:calc(50vh - 340px);border:solid 3px #fff;padding:0 10px 10px;border-radius:3px}#l{display:block;max-width:150px;height:155px;margin:-80px auto 20px;background:#00d151;padding-right:5px}#l path{fill:#000}.w{margin:3px 0 16px;padding:5px 0;border-radius:3px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3)}h1,h3{margin:10px 8px;font-family:HelveticaNeueLight,HelveticaNeue-Light,'Helvetica Neue Light',HelveticaNeue,'Helvetica Neue',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:300;font-stretch:normal;color:#000;font-size:23px}h1{margin-bottom:14px;color:#fff}span{display:block;margin:10px 7px 14px}i{display:block;font-style:normal;position:relative;border-bottom:solid 1px #00d151;height:42px}i:last-child{border:none}label{position:absolute;display:inline-block;top:0;left:8px;color:#00d151;line-height:41px;pointer-events:none}input,select{width:calc(100% - 145px);border:none;font-size:16px;line-height:40px;border-radius:0;letter-spacing:-.5px;background:#fff;color:#000;padding-left:144px;-webkit-appearance:none;-moz-appearance:none;appearance:none;outline:0!important;height:40px}select{padding:0;float:right;margin:1px 3px 1px 2px}button{width:100%;border:0;background:#000;padding:5px 10px;font-size:16px;line-height:40px;color:#fff;border-radius:3px;box-shadow:0 1px 3px rgba(0,0,0,.3);cursor:pointer}.c{background:#ffe836;position:fixed;width:100%;line-height:80px;color:#000;top:0;left:0;box-shadow:0 1px 3px rgba(0,0,0,.3);text-align:center;font-size:26px;z-index:100}@media all and (max-height:920px){.s{margin-top:80px}}@media all and (max-width:900px){.s{width:calc(100% - 20px);margin-top:40px;border:none;padding:0 8px;border-radius:0}#l{max-width:80px;height:auto;margin:10px auto 20px}h1,h3{font-size:19px}i{border:none;height:auto}label{display:block;margin:4px 0 12px;color:#00d151;font-size:13px;position:relative;line-height:18px}input,select{width:calc(100% - 10px);font-size:16px;line-height:28px;padding:0 5px;border-bottom:solid 1px #00d151}select{width:100%;float:none;margin:0}}</style>";
    content += "<div class='s'><svg version='1.1' id='l' x='0' y='0' viewBox='0 0 200 200' xml:space='preserve'><path d='M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1.7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22.9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0.3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1.8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2,14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1.1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-21.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,17.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8,0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2,4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22.4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188.6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,88.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z'/></svg>";
    content += "<h1><center>" + String(SuplaDevice_setName) + "</center></h1>";
    content += "<font size='2'>GUID: " + My_guid + "</font><br>";
    content += "<font size='2'>MAC:  " + My_mac + "</font><br>";
    long rssi = WiFi.RSSI();
    content += "<font size='2'>RSSI: " + String(rssi) + "</font>";
    content += "<div class='w'>";
    content += "<h3>Pomiary BME280";
    content += "</h3>";
    content += "<i><input name='pomiar_t' value='" + String(last_temperature) + "'length=15><label>Temperatura [°C]</label></i>";
    content += "<i><input name='pomiar_w' value='" + String(last_humidity) + "'length=15><label>Wilgotność [%]</label></i>";
    content += "<i><input name='pomiar_p' value='" + String(last_pressure) + "'length=15><label>Ciśnienie [kPa]</label></i>";
    content += "<i><input name='pomiar_p2' value='" + String(last_pressure_sea) + "'length=15><label>Ciśn. n.p.m. [hPa]</label></i>";
    content += "</div>";
    content += "<a href='https://forum.supla.org/viewtopic.php?f=24&t=4136'>";
    content += "<div class='w'>";
    content += "<h3><center>Zapraszam na forum.supla.org</center></h3>";
    content += "</div></a>";
    content += "<font size='3' >"
               "Na kanale 0 dostępne jest ciśnienie atmosferyczne zredukowane do poziomu morza, należy je skalibrować o wysokość nad poziomem morza.\n"
               "Na kanale 1 (temp.) dostępny jest poziom sygnału RSSI modułu ESP.\n"
               "Kanał 2 to temperatura i wilgotność. Na kanale 3 (temp.) przedstawione jest napięcie zasilania modułu ESP. Kanał 4 prezentuje ciśnienie absolutne.\n"
               "</font><br>";
    content += "<meta http-equiv=\"Refresh\" content=\"5; url=/about\" />";
    content += "<br>";
    content += "<a href='/'><button>POWRÓT</button></a>";
    content += "<br><br>";
    httpServer.send(200, "text/html", content);
  });

  httpServer.on("/set0", []() {

    String qssid = "";
    String qpass = "";
    String qip_address = "";
    String qip_mask = "";
    String qip_gateway = "";
    String qSupla_Server_ADR = "";
    String qSupla_Location_ID = "";
    String qSupla_Location_Pass = "";
    String qkal_wys = "";
    String qDHCP_on_off = "";
    String qDHCP_select = "";


    qssid = httpServer.arg("wifi_ssid");
    qpass = httpServer.arg("wifi_pass");
    qip_address = httpServer.arg("ip_address");
    qip_mask = httpServer.arg("ip_mask");
    qip_gateway = httpServer.arg("ip_gateway");
    qSupla_Server_ADR = httpServer.arg("Supla_Server_ADR");
    qSupla_Location_ID = httpServer.arg("Supla_Location_ID");
    qSupla_Location_Pass = httpServer.arg("Supla_Location_Pass");
    qkal_wys = httpServer.arg("kal_wys");
    qDHCP_on_off = httpServer.arg("DHCP_on_off");
    qDHCP_select = httpServer.arg("DHCP_select");

    EEPROM.begin(EEPROM_SIZE);
    delay(100);
    for (int zmienna_int = 0; zmienna_int < EEPROM_SIZE; ++zmienna_int) {
      EEPROM.write(zmienna_int, 0);  //Czyszczenie pamięci EEPROM

    }
    EEPROM.end();
    delay(100);
    EEPROM.begin(EEPROM_SIZE);
    delay(100);
    for (zmienna_int = 0; zmienna_int < qssid.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_SSID + zmienna_int, qssid[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qpass.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_PASSWORD + zmienna_int, qpass[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qip_address.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_ADRESS + zmienna_int, qip_address[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qip_mask.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_MASK + zmienna_int, qip_mask[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qip_gateway.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_GATEWAY + zmienna_int, qip_gateway[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qSupla_Server_ADR.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_SUPLASRV + zmienna_int, qSupla_Server_ADR[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qSupla_Location_ID.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_SUPLALOCID + zmienna_int, qSupla_Location_ID[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qSupla_Location_Pass.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_SUPLALOCPASS + zmienna_int, qSupla_Location_Pass[zmienna_int]);
      delay(3);
    }
    for (zmienna_int = 0; zmienna_int < qkal_wys.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_HIGHTCAL + zmienna_int, qkal_wys[zmienna_int]);
      delay(3);
    }
    //for (zmienna_int = 0; zmienna_int < qDHCP_on_off.length(); ++zmienna_int)         {EEPROM.write(190+zmienna_int, qDHCP_on_off[zmienna_int]);delay(3);}
    for (zmienna_int = 0; zmienna_int < qDHCP_select.length(); ++zmienna_int) {
      EEPROM.write(EEPROM_DHCPEN + zmienna_int, qDHCP_select[zmienna_int]);
      delay(3);
    }

    EEPROM.end();
    delay(200);

    content = "<!DOCTYPE HTML>";
    content += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>";
    content += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
    content += "<style>body{font-size:14px;font-family:HelveticaNeue,'Helvetica Neue',HelveticaNeueRoman,HelveticaNeue-Roman,'Helvetica Neue Roman',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:400;font-stretch:normal;background:#00d151;color:#fff;line-height:20px;padding:0}.s{width:460px;margin:0 auto;margin-top:calc(50vh - 340px);border:solid 3px #fff;padding:0 10px 10px;border-radius:3px}#l{display:block;max-width:150px;height:155px;margin:-80px auto 20px;background:#00d151;padding-right:5px}#l path{fill:#000}.w{margin:3px 0 16px;padding:5px 0;border-radius:3px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3)}h1,h3{margin:10px 8px;font-family:HelveticaNeueLight,HelveticaNeue-Light,'Helvetica Neue Light',HelveticaNeue,'Helvetica Neue',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:300;font-stretch:normal;color:#000;font-size:23px}h1{margin-bottom:14px;color:#fff}span{display:block;margin:10px 7px 14px}i{display:block;font-style:normal;position:relative;border-bottom:solid 1px #00d151;height:42px}i:last-child{border:none}label{position:absolute;display:inline-block;top:0;left:8px;color:#00d151;line-height:41px;pointer-events:none}input,select{width:calc(100% - 145px);border:none;font-size:16px;line-height:40px;border-radius:0;letter-spacing:-.5px;background:#fff;color:#000;padding-left:144px;-webkit-appearance:none;-moz-appearance:none;appearance:none;outline:0!important;height:40px}select{padding:0;float:right;margin:1px 3px 1px 2px}button{width:100%;border:0;background:#000;padding:5px 10px;font-size:16px;line-height:40px;color:#fff;border-radius:3px;box-shadow:0 1px 3px rgba(0,0,0,.3);cursor:pointer}.c{background:#ffe836;position:fixed;width:100%;line-height:80px;color:#000;top:0;left:0;box-shadow:0 1px 3px rgba(0,0,0,.3);text-align:center;font-size:26px;z-index:100}@media all and (max-height:920px){.s{margin-top:80px}}@media all and (max-width:900px){.s{width:calc(100% - 20px);margin-top:40px;border:none;padding:0 8px;border-radius:0}#l{max-width:80px;height:auto;margin:10px auto 20px}h1,h3{font-size:19px}i{border:none;height:auto}label{display:block;margin:4px 0 12px;color:#00d151;font-size:13px;position:relative;line-height:18px}input,select{width:calc(100% - 10px);font-size:16px;line-height:28px;padding:0 5px;border-bottom:solid 1px #00d151}select{width:100%;float:none;margin:0}}</style>";
    content += "<div class='s'><svg version='1.1' id='l' x='0' y='0' viewBox='0 0 200 200' xml:space='preserve'><path d='M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1.7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22.9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0.3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1.8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2,14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1.1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-21.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,17.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8,0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2,4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22.4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188.6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,88.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z'/></svg>";
    content += "<h1><center>" + String(SuplaDevice_setName) + "</center></h1>";
    content += "<h3><center>ZAPISANO</center></h3>";
    content += "<meta http-equiv=\"Refresh\" content=\"1; url=/\" />";
    content += "</div><br><br>";

    httpServer.send(200, "text/html", content);
  });

  //************************************************************

  httpServer.on("/firmware_up", []() {
    content = "<!DOCTYPE HTML>";
    content += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>";
    content += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
    content += "<style>body{font-size:14px;font-family:HelveticaNeue,'Helvetica Neue',HelveticaNeueRoman,HelveticaNeue-Roman,'Helvetica Neue Roman',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:400;font-stretch:normal;background:#00d151;color:#fff;line-height:20px;padding:0}.s{width:460px;margin:0 auto;margin-top:calc(50vh - 340px);border:solid 3px #fff;padding:0 10px 10px;border-radius:3px}#l{display:block;max-width:150px;height:155px;margin:-80px auto 20px;background:#00d151;padding-right:5px}#l path{fill:#000}.w{margin:3px 0 16px;padding:5px 0;border-radius:3px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3)}h1,h3{margin:10px 8px;font-family:HelveticaNeueLight,HelveticaNeue-Light,'Helvetica Neue Light',HelveticaNeue,'Helvetica Neue',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:300;font-stretch:normal;color:#000;font-size:23px}h1{margin-bottom:14px;color:#fff}span{display:block;margin:10px 7px 14px}i{display:block;font-style:normal;position:relative;border-bottom:solid 1px #00d151;height:42px}i:last-child{border:none}label{position:absolute;display:inline-block;top:0;left:8px;color:#00d151;line-height:41px;pointer-events:none}input,select{width:calc(100% - 145px);border:none;font-size:16px;line-height:40px;border-radius:0;letter-spacing:-.5px;background:#fff;color:#000;padding-left:144px;-webkit-appearance:none;-moz-appearance:none;appearance:none;outline:0!important;height:40px}select{padding:0;float:right;margin:1px 3px 1px 2px}button{width:100%;border:0;background:#000;padding:5px 10px;font-size:16px;line-height:40px;color:#fff;border-radius:3px;box-shadow:0 1px 3px rgba(0,0,0,.3);cursor:pointer}.c{background:#ffe836;position:fixed;width:100%;line-height:80px;color:#000;top:0;left:0;box-shadow:0 1px 3px rgba(0,0,0,.3);text-align:center;font-size:26px;z-index:100}@media all and (max-height:920px){.s{margin-top:80px}}@media all and (max-width:900px){.s{width:calc(100% - 20px);margin-top:40px;border:none;padding:0 8px;border-radius:0}#l{max-width:80px;height:auto;margin:10px auto 20px}h1,h3{font-size:19px}i{border:none;height:auto}label{display:block;margin:4px 0 12px;color:#00d151;font-size:13px;position:relative;line-height:18px}input,select{width:calc(100% - 10px);font-size:16px;line-height:28px;padding:0 5px;border-bottom:solid 1px #00d151}select{width:100%;float:none;margin:0}}</style>";
    content += "<div class='s'><svg version='1.1' id='l' x='0' y='0' viewBox='0 0 200 200' xml:space='preserve'><path d='M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1.7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22.9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0.3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1.8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2,14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1.1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-21.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,17.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8,0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2,4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22.4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188.6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,88.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z'/></svg>";
    content += "<h1><center>" + String(SuplaDevice_setName) + "</center></h1>";
    content += "<h3><center>Aktualizacja opr.</center></h3>";
    content += "<br>";
    content += "<center>";
    //content += "<div>";
    content += "<iframe src=/firmware>Twoja przeglądarka nie akceptuje ramek! width='200' height='100' frameborder='100'></iframe>";
    //content += "</div>";
    content += "</center>";
    content += "<a href='/'><button>POWRÓT</button></a></div>";
    content += "<br><br>";
    httpServer.send(200, "text/html", content);
  });
  //************************************************************

  httpServer.on("/reboot", []() {
    content = "<!DOCTYPE HTML>";
    content += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>";
    content += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>";
    content += "<style>body{font-size:14px;font-family:HelveticaNeue,'Helvetica Neue',HelveticaNeueRoman,HelveticaNeue-Roman,'Helvetica Neue Roman',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:400;font-stretch:normal;background:#00d151;color:#fff;line-height:20px;padding:0}.s{width:460px;margin:0 auto;margin-top:calc(50vh - 340px);border:solid 3px #fff;padding:0 10px 10px;border-radius:3px}#l{display:block;max-width:150px;height:155px;margin:-80px auto 20px;background:#00d151;padding-right:5px}#l path{fill:#000}.w{margin:3px 0 16px;padding:5px 0;border-radius:3px;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3)}h1,h3{margin:10px 8px;font-family:HelveticaNeueLight,HelveticaNeue-Light,'Helvetica Neue Light',HelveticaNeue,'Helvetica Neue',TeXGyreHerosRegular,Helvetica,Tahoma,Geneva,Arial,sans-serif;font-weight:300;font-stretch:normal;color:#000;font-size:23px}h1{margin-bottom:14px;color:#fff}span{display:block;margin:10px 7px 14px}i{display:block;font-style:normal;position:relative;border-bottom:solid 1px #00d151;height:42px}i:last-child{border:none}label{position:absolute;display:inline-block;top:0;left:8px;color:#00d151;line-height:41px;pointer-events:none}input,select{width:calc(100% - 145px);border:none;font-size:16px;line-height:40px;border-radius:0;letter-spacing:-.5px;background:#fff;color:#000;padding-left:144px;-webkit-appearance:none;-moz-appearance:none;appearance:none;outline:0!important;height:40px}select{padding:0;float:right;margin:1px 3px 1px 2px}button{width:100%;border:0;background:#000;padding:5px 10px;font-size:16px;line-height:40px;color:#fff;border-radius:3px;box-shadow:0 1px 3px rgba(0,0,0,.3);cursor:pointer}.c{background:#ffe836;position:fixed;width:100%;line-height:80px;color:#000;top:0;left:0;box-shadow:0 1px 3px rgba(0,0,0,.3);text-align:center;font-size:26px;z-index:100}@media all and (max-height:920px){.s{margin-top:80px}}@media all and (max-width:900px){.s{width:calc(100% - 20px);margin-top:40px;border:none;padding:0 8px;border-radius:0}#l{max-width:80px;height:auto;margin:10px auto 20px}h1,h3{font-size:19px}i{border:none;height:auto}label{display:block;margin:4px 0 12px;color:#00d151;font-size:13px;position:relative;line-height:18px}input,select{width:calc(100% - 10px);font-size:16px;line-height:28px;padding:0 5px;border-bottom:solid 1px #00d151}select{width:100%;float:none;margin:0}}</style>";
    content += "<div class='s'><svg version='1.1' id='l' x='0' y='0' viewBox='0 0 200 200' xml:space='preserve'><path d='M59.3,2.5c18.1,0.6,31.8,8,40.2,23.5c3.1,5.7,4.3,11.9,4.1,18.3c-0.1,3.6-0.7,7.1-1.9,10.6c-0.2,0.7-0.1,1.1,0.6,1.5c12.8,7.7,25.5,15.4,38.3,23c2.9,1.7,5.8,3.4,8.7,5.3c1,0.6,1.6,0.6,2.5-0.1c4.5-3.6,9.8-5.3,15.7-5.4c12.5-0.1,22.9,7.9,25.2,19c1.9,9.2-2.9,19.2-11.8,23.9c-8.4,4.5-16.9,4.5-25.5,0.2c-0.7-0.3-1-0.2-1.5,0.3c-4.8,4.9-9.7,9.8-14.5,14.6c-5.3,5.3-10.6,10.7-15.9,16c-1.8,1.8-3.6,3.7-5.4,5.4c-0.7,0.6-0.6,1,0,1.6c3.6,3.4,5.8,7.5,6.2,12.2c0.7,7.7-2.2,14-8.8,18.5c-12.3,8.6-30.3,3.5-35-10.4c-2.8-8.4,0.6-17.7,8.6-22.8c0.9-0.6,1.1-1,0.8-2c-2-6.2-4.4-12.4-6.6-18.6c-6.3-17.6-12.7-35.1-19-52.7c-0.2-0.7-0.5-1-1.4-0.9c-12.5,0.7-23.6-2.6-33-10.4c-8-6.6-12.9-15-14.2-25c-1.5-11.5,1.7-21.9,9.6-30.7C32.5,8.9,42.2,4.2,53.7,2.7c0.7-0.1,1.5-0.2,2.2-0.2C57,2.4,58.2,2.5,59.3,2.5z M76.5,81c0,0.1,0.1,0.3,0.1,0.6c1.6,6.3,3.2,12.6,4.7,18.9c4.5,17.7,8.9,35.5,13.3,53.2c0.2,0.9,0.6,1.1,1.6,0.9c5.4-1.2,10.7-0.8,15.7,1.6c0.8,0.4,1.2,0.3,1.7-0.4c11.2-12.9,22.5-25.7,33.4-38.7c0.5-0.6,0.4-1,0-1.6c-5.6-7.9-6.1-16.1-1.3-24.5c0.5-0.8,0.3-1.1-0.5-1.6c-9.1-4.7-18.1-9.3-27.2-14c-6.8-3.5-13.5-7-20.3-10.5c-0.7-0.4-1.1-0.3-1.6,0.4c-1.3,1.8-2.7,3.5-4.3,5.1c-4.2,4.2-9.1,7.4-14.7,9.7C76.9,80.3,76.4,80.3,76.5,81z M89,42.6c0.1-2.5-0.4-5.4-1.5-8.1C83,23.1,74.2,16.9,61.7,15.8c-10-0.9-18.6,2.4-25.3,9.7c-8.4,9-9.3,22.4-2.2,32.4c6.8,9.6,19.1,14.2,31.4,11.9C79.2,67.1,89,55.9,89,42.6z M102.1,188.6c0.6,0.1,1.5-0.1,2.4-0.2c9.5-1.4,15.3-10.9,11.6-19.2c-2.6-5.9-9.4-9.6-16.8-8.6c-8.3,1.2-14.1,8.9-12.4,16.6C88.2,183.9,94.4,188.6,102.1,188.6z M167.7,88.5c-1,0-2.1,0.1-3.1,0.3c-9,1.7-14.2,10.6-10.8,18.6c2.9,6.8,11.4,10.3,19,7.8c7.1-2.3,11.1-9.1,9.6-15.9C180.9,93,174.8,88.5,167.7,88.5z'/></svg>";
    content += "<h1><center>" + String(SuplaDevice_setName) + "</center></h1>";
    content += "<h3><center>RESTART ESP</center></h3>";
    content += "<br>";
    content += "<a href='/'><button>POWRÓT</button></a></div>";
    content += "<br><br>";
    httpServer.send(200, "text/html", content);
    ESP.reset();
  }
               );
}

//****************************************************************************************************************************************
void Tryb_konfiguracji()
{
  Serial.println("Tryb konfiguracji");
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP((config_Wifi_name + "-" + WiFi.macAddress()).c_str());  //without password
  //WiFi.softAP((config_Wifi_name + "-" + WiFi.macAddress()).c_str()) , config_Wifi_pass);
  delay(1000);
  Serial.println("Tryb AP");

  createWebServer();
  httpServer.begin();
  Serial.println("Start Serwera");
  Serial.println("Połącz się z siecią¦ " + config_Wifi_name + "-" + WiFi.macAddress() + " i dokończ konfigurację pod adresem 192.168.4.1");
  Modul_tryb_konfiguracji = 2;
Utrzymaj_serwer:
  httpServer.handleClient();//Ta pętla nigdy się nie skończy
  if (Modul_tryb_konfiguracji == 2) {
    goto Utrzymaj_serwer;
  }
}

void WiFi_up() {
  WiFi.setOutputPower(20.5);
  WiFi.disconnect();
  delay(1000);

  esid = Odczytaj_zakres_eeprom(EEPROM_SSID, MAX_SSID);
  epass = Odczytaj_zakres_eeprom(EEPROM_PASSWORD, MAX_PASSWORD);

  Serial.println("WiFi init");
  Serial.print("SSID: ");
  Serial.print(esid.c_str());
  Serial.print(" PASSWORD: ");
  //Serial.print(epass.c_str()); //wyświetla hasło
  Serial.println("********");
  WiFi.mode(WIFI_STA);
  WiFi.begin(esid.c_str(), epass.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("");
  Serial.print("Adres IP:        ");
  Serial.println(WiFi.localIP());
  Serial.print("Maska podsieci:  ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Brama domyślna:  ");
  Serial.println(WiFi.gatewayIP());
  Serial.println("");
}

void status_func(int status, const char *msg) {
  aktualny_status = status;
}

//*********************************************************************************************************

void Pokaz_zawartosc_eeprom()
{
  EEPROM.begin(EEPROM_SIZE);
  delay(100);
  Serial.println("|--------------------------------------------------------------------------------------------------------------------------------------------|");
  Serial.println("|                                             HEX                                                     |                STRING                |");

  byte  eeprom = 0;
  String  eeprom_string = "";
  String znak = "";

  byte licz_wiersz = 0;

  for (int zmienna_int2 = 0; zmienna_int2 < 4096; ++zmienna_int2)   {
    //ESP.wdtFeed(); // Reset watchdoga
    eeprom = (EEPROM.read(zmienna_int2));
    znak = char(EEPROM.read(zmienna_int2));

    if (znak == "") {
      eeprom_string += " ";
    } else {
      eeprom_string += znak;
    } znak = "";
    if (licz_wiersz == 0) {
      Serial.print("|   ");
    } licz_wiersz++;
    if (licz_wiersz >= 0 && licz_wiersz < 32) {
      ;
      printf("%02X", eeprom);
      Serial.print(" ");
    }
    if (licz_wiersz == 32) {
      printf("%02X", eeprom);
      Serial.print("   |   ");
      Serial.print(eeprom_string);
      Serial.println("   |");
      eeprom_string = "";
      licz_wiersz = 0;
    }
  }

  Serial.println("|--------------------------------------------------------------------------------------------------------------------------------------------|");
  Serial.println("");
  Serial.println("");
}

//****************************************************************************************************************************************

String Odczytaj_zakres_eeprom(int x1, int y1) {
  EEPROM.begin(EEPROM_SIZE);
  delay(10);
  String aux1 = "";

  for (int zmienna_int_3 = x1; zmienna_int_3 < x1 + y1; ++zmienna_int_3) {
    aux1 += char(EEPROM.read(zmienna_int_3));
  }

  return aux1;
}

//****************************************************************************************************************************************

//int Odczytaj_pojedynczy_eeprom(int x2) {
//  EEPROM.begin(EEPROM_SIZE);
//  delay(10);
//  zmienna = "";
//  zmienna += char(EEPROM.read(x2));
//}

//****************************************************************************************************************************************

void Read_auth_param()
{
  custom_Supla_server = Odczytaj_zakres_eeprom(EEPROM_SUPLASRV, MAX_SUPLASRV);
  Location_id_string = Odczytaj_zakres_eeprom(EEPROM_SUPLALOCID, MAX_SUPLALOCID);
  custom_Location_Pass = Odczytaj_zakres_eeprom(EEPROM_SUPLALOCPASS, MAX_SUPLALOCPASS);
}

//****************************************************************************************************************************************

void Odczytaj_parametry_IP()
{
  String eip_address = "";  for (int zmienna_int5 = 4000; zmienna_int5 < 4015; ++zmienna_int5)   {
    eip_address += char(EEPROM.read(zmienna_int5));
  }
  zmienna = eip_address.c_str();
  int IP[4] = {0, 0, 0, 0};
  int Part_IP = 0;

  for ( int zmienna_int6 = 0; zmienna_int6 < zmienna.length(); zmienna_int6++ ) {
    char c = zmienna[zmienna_int6];
    if ( c == '.' ) {
      Part_IP++;
      continue;
    } IP[Part_IP] *= 10;
    IP[Part_IP] += c - '0';
  }

  String eip_mask = "";  for (int zmienna_int7 = 4015; zmienna_int7 < 4030; ++zmienna_int7)   {
    eip_mask += char(EEPROM.read(zmienna_int7));
  }
  zmienna = eip_mask.c_str();
  int MASK[4] = {0, 0, 0, 0};
  int Part_MASK = 0;

  for ( int zmienna_int8 = 0; zmienna_int8 < zmienna.length(); zmienna_int8++ ) {
    char c = zmienna[zmienna_int8];
    if ( c == '.' ) {
      Part_MASK++;
      continue;
    } MASK[Part_MASK] *= 10;
    MASK[Part_MASK] += c - '0';
  }

  String eip_gateway = "";  for (int i = 4030; i < 4045; ++i)   {
    eip_gateway += char(EEPROM.read(i));
  }
  zmienna = eip_gateway .c_str();
  int GW[4] = {0, 0, 0, 0};
  int Part_GATEWAY = 0;

  for ( int zmienna_int9 = 0; zmienna_int9 < zmienna.length(); zmienna_int9++ ) {
    char c = zmienna[zmienna_int9];
    if ( c == '.' ) {
      Part_GATEWAY++;
      continue;
    } GW[Part_GATEWAY] *= 10;
    GW[Part_GATEWAY] += c - '0';
  }

  if (Odczytaj_zakres_eeprom(EEPROM_DHCPEN, MAX_DHCPEN) == "1") {
    DHCP = 1;
  }
  else {
    DHCP = 0;
  }

  if (DHCP == 0) {
    if (IP[0] == 0 && IP[1] < 1 && IP[2] == 0 && IP[3] == 0) {
      Serial.println("Niewłaściwy adres IP");
      DHCP = 1;
    }
    if (IP[0] > 248 && IP[1] >= 0 && IP[2] >= 0 && IP[3] >= 0) {
      Serial.println("Niewłaściwy adres IP");
      DHCP = 1;
    }

    if (MASK[0] == 0 && MASK[1] == 0 && MASK[2] == 0 && MASK[3] == 0) {
      Serial.println("Niewłaściwa MASKA");
      DHCP = 1;
    }

    if (GW[0] == 0 && GW[1] < 1 && GW[2] == 0 && GW[3] == 0) {
      Serial.println("Niewłaściwy adres BRAMY");
      DHCP = 1;
    }

    if (GW[0] >= 248 && GW[1] >= 0 && GW[2] >= 0 && GW[3] >= 0) {
      Serial.println("Niewłaściwy adres BRAMY");
      DHCP = 1;
    }
  }

  if (DHCP == 1) {
    Serial.println("Adresacja IP zostanie pobrana z serwera DHCP po nawiązaniu połczenia WiFi");
    WiFi.begin();
  }
  if (DHCP == 0) {
    Serial.println("Ustawiam statyczny adres IP");
    IPAddress staticIP(IP[0], IP[1], IP[2], IP[3]);
    IPAddress subnet(MASK[0], MASK[1], MASK[2], MASK[3]);
    IPAddress gateway(GW[0], GW[1], GW[2], GW[3]);
    WiFi.config(staticIP, gateway, subnet);
  }
}

//****************************************************************************************************************************************

// Supla.org ethernet layer
int supla_arduino_tcp_read(void *buf, int count) {
  _supla_int_t size = client.available();

  if ( size > 0 ) {
    if ( size > count ) size = count;
    return client.read((uint8_t *)buf, size);
  };

  return -1;
};

int supla_arduino_tcp_write(void *buf, int count) {
  return client.write((const uint8_t *)buf, count);
};

bool supla_arduino_svr_connect(const char *server, int port) {
  return client.connect(server, 2015);
}

bool supla_arduino_svr_connected(void) {
  return client.connected();
}

void supla_arduino_svr_disconnect(void) {
  client.stop();
}

void supla_arduino_eth_setup(uint8_t mac[6], IPAddress *ip) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi_up();
  }
}

void IndexSortDescending(int numb[], int numb_length, int *index)
{

  //int index[numb_length];
  int i, j;

  for (i = 0; i <= numb_length - 1; i++)
  {
    index[i] = i;
  }

  for (i = 0; i <= numb_length - 2; i++)
  {
    for (j = i + 1; j <= numb_length - 1; j++)
    {
      int temp;

      if (numb[index[i]] < numb[index[j]])
      {
        temp = index[i];
        index[i] = index[j];
        index[j] = temp;
      }
    }
  }
  //return index;
}

SuplaDeviceCallbacks supla_arduino_get_callbacks(void) {
  SuplaDeviceCallbacks cb;

  cb.tcp_read = &supla_arduino_tcp_read;
  cb.tcp_write = &supla_arduino_tcp_write;
  cb.eth_setup = &supla_arduino_eth_setup;
  cb.svr_connected = &supla_arduino_svr_connected;
  cb.svr_connect = &supla_arduino_svr_connect;
  cb.svr_disconnect = &supla_arduino_svr_disconnect;
  cb.get_temperature = &get_pressure;
  cb.get_temperature_and_humidity = &get_temperature_and_humidity;
  cb.get_rgbw_value = NULL;
  cb.set_rgbw_value = NULL;

  return cb;
}

//*********************************************************************************************************
