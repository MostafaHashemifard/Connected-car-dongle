#define TINY_GSM_MODEM_SIM868
#define DEBUG 1
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <TinyGsmClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <ArduinoJson.h>

SoftwareSerial gsmSerial(27, 25); //RX TX
SoftwareSerial obdSerial(13, 12); //RX TX
TinyGsm modem(gsmSerial);
StaticJsonBuffer<200> jsonBuffer;
Adafruit_MPU6050 mpu;
//const int rs = 19, en = 5, d4 = 18, d5 = 4, d6 = 15, d7 = 2;
const int rs = 19, en = 5, d4 = 18, d5 = 4, d6 = 14, d7 = 17;
const int statusLed = 15;
const int buzzer = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

char t[32];
char deviceID[12] = "CC01";
char rxData[20];
char rxIndex = 0;
int vehicleSpeed = 0;
int vehicleRPM = 0;
String message;
String result;
String tmp;

String engineTemp;
String batVoltage;
String engineRpm;
String vechSpeed;
String fuelLevel;
String location;
String acc;
String rot;
String temperature;

String obdResponsePurify(String input)
{
  int i;
  for (i = 0; i < input.length(); i++)
  {
    if (isDigit(input[i]) || input[i] == 'A' || input[i] == 'B' || input[i] == 'C' || input[i] == 'D' || input[i] == 'E' || input[i] == 'F' || input[i] == '.')
      continue;
    else
      input[i] = '*';
  }
  input.replace("*", "");
  return input;
}

String removeBackSlashRs(String input)
{
  /*
    int i;
    for(i = 0; i < input.length(); i++)
    {
    if(input[i] == '\r')
      input[i] = '';
    }
    return input;
  */
}

String readSerialData(bool keepOK, int device)
{
  String output;
  int tryCnt = 0;
  if (device == 1)
  {
    while (gsmSerial.available() == 0)
    {
      if (tryCnt == 100)
        break;
      delay(100);
      tryCnt++;
    }
    //while (gsmSerial.available() != 0)
    //Serial.write(gsmSerial.read());
    //output += gsmSerial.readString();
    output = gsmSerial.readString();
    output.trim();
    if (keepOK)
    {
      if (output.indexOf("OK") == -1)
        return "NA";
      output = output.substring(output.indexOf("OK"), output.indexOf("OK") + 2);
    }
  }
  else
  {
    while (obdSerial.available() == 0)
    {
      if (tryCnt == 100)
        break;
      delay(100);
      tryCnt++;
    }
    //while (obdSerial.available() != 0)
    //output += obdSerial.readString();
    output = obdSerial.readString();
    output.trim();
  }
  output.replace("\r", "");
  return output;
}

String certainExploder(String input, int number)
{
  int pos, it = 1;;
  String output;
  while (true)
  {
    pos = input.indexOf(",");
    if (pos == -1)
      break;
    output = input.substring(0, pos);
    input = input.substring(pos + 1, input.length());
    if (it == number)
      break;
    it++;
  }
  return output;
}

void logger(int x, int y, int dly, bool needsClear, String msg)
{
  if (needsClear)
    lcd.clear();
  lcd.setCursor(x, y);
  lcd.print(msg);
  if (dly != 0)
    delay(dly);
}

void mayday()
{
  logger(0, 0, 0, 1, "MAYDAY DETECTED!");
  logger(0, 1, 0, 0, "RESETING SIM868!");
  delay(10000);
  gsmSerial.println("AT");
  delay(200);
  logger(0, 2, 500, 0, readSerialData(0, 1));
  gsmSerial.println("AT+CFUN=1,1");
  delay(3000);
  logger(0, 3, 500, 0, readSerialData(0, 1));
  //
  lcd.clear();
}
int mpuSetup()
{
  message = "";
  if (!mpu.begin())
    return 0;
  else
  {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  }
  return 1;
}

int simSetup()
{
  gsmSerial.begin(9600);
  delay(100);
  //
  logger(0, 0, 0, 1, "AT");
  gsmSerial.println("AT");
  delay(1000);
  logger(0, 1, 200, 0, readSerialData(0, 1));
  //
  while (true)
  {
    tmp = "";
    gsmSerial.println("AT+CGREG?");
    delay(500);
    tmp = gsmSerial.readString();
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print(tmp);
    if (tmp.indexOf("1") != -1 || tmp.indexOf("5") != -1)
      break;
  }
  delay(200);
  //
  //gsmSerial.println("AT+CMEE=2;+CFUN=1;+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"mtnirancell\";+SAPBR=2,1");
  gsmSerial.println("ATE0;+CMEE=2;+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"mtnirancell\";+SAPBR=1,1");
  delay(5000);
  logger(0, 0, 1000, 1, readSerialData(0, 1));
  delay(1000);
  gsmSerial.println("AT+SAPBR=2,1");
  delay(1000);
  logger(0, 0, 1000, 1, readSerialData(0, 1));
  //
  gsmSerial.println("AT+CGNSPWR=1;+CGNSSEQ=\"RMC\"");
  delay(200);
  logger(0, 0, 1000, 1, readSerialData(0, 1));
  //
  return 1;
}

int obdSetup()
{
  message = "";
  obdSerial.begin(9600);
  delay(100);
  //
  obdSerial.println("ATZ");
  delay(200);
  logger(0, 0, 500, 1, readSerialData(0, 2));
  //
  obdSerial.println("ATSP0");
  delay(200);
  logger(0, 0, 500, 1, readSerialData(0, 2));
  //
  obdSerial.println("0100");
  delay(8000);
  logger(0, 0, 500, 1, readSerialData(0, 2));
  return 1;
}

int httpInit()
{
  logger(0, 0, 0, 1, "HTTPINIT");
  gsmSerial.println("AT+HTTPINIT;+HTTPPARA=\"CID\",1;+HTTPPARA=\"TIMEOUT\",30;+HTTPPARA=\"URL\",\"http://78.157.34.102:3000/data\";+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(200);
  tmp = "";
  tmp = readSerialData(0, 1);
  if (tmp.indexOf("ERROR") != -1 || tmp.indexOf("DEACT") != -1)
    return 0;
  else
    logger(0, 1, 1000, 0, tmp);
  //
  return 1;
}

int httpSend(String outputData)
{
  int tryCnt = 0;
  logger(0, 0, 0, 1, "HTTPDATA");
  gsmSerial.println("AT+HTTPDATA=" + String(outputData.length() + 50) + ",2000");
  delay(200);
  tmp = "";
  tmp = readSerialData(0, 1);
  if (tmp.indexOf("ERROR") != -1 || tmp.indexOf("DEACT") != -1)
    return 0;
  else
    logger(0, 1, 200, 0, tmp);
  //
  logger(0, 0, 0, 1, "outputData");
  gsmSerial.println(outputData);
  delay(1000);
  tmp = "";
  tmp = readSerialData(0, 1);
  if (tmp.indexOf("ERROR") != -1 || tmp.indexOf("DEACT") != -1)
    return 0;
  else
    logger(0, 1, 200, 0, tmp);
  //
  delay(1000);
  logger(0, 0, 0, 1, "HTTPACTION");
  gsmSerial.println("AT+HTTPACTION=1");
  while (gsmSerial.available() == 0)
  {
    tmp = "";
    tmp = gsmSerial.readString();
    lcd.setCursor(0, 2);
    lcd.print(tmp);
    if (tmp.indexOf("ERROR") != -1 || tmp.indexOf("DEACT") != -1)
      return 0;
    else if (tmp.indexOf(",") != -1)
      break;
    if (tryCnt == 300)
      break;
    delay(100);
    tryCnt++;
  }
  delay(500);
  //logger(0, 1, 200, 0, readSerialData(0, 1));
  //
  //logger(0, 0, 0, 1, "HTTPREAD");
  //gsmSerial.println("AT+HTTPREAD");
  //delay(1000);
  //logger(0, 1, 200, 0, gsmSerial.readString());
  //
  return 1;
}

int httpTerm()
{
  logger(0, 0, 0, 1, "HTTPTERM");
  gsmSerial.println("AT+HTTPTERM");
  delay(500);
  digitalWrite(buzzer, HIGH);
  delay(200);
  digitalWrite(buzzer, LOW);
  delay(200);
  digitalWrite(buzzer, HIGH);
  delay(200);
  digitalWrite(buzzer, LOW);
  delay(200);
  logger(0, 1, 200, 0, gsmSerial.readString());
  //
  return 1;
}

int fillLocationData()
{
  String helper;
  //
  gsmSerial.println("AT+CGNSINF");
  delay(200);
  helper = readSerialData(0, 1);
  location = certainExploder(helper, 4) + "," + certainExploder(helper, 5);
  if (location == ",")
  {
    logger(0, 0, 0, 1, "Location failed");
    logger(0, 1, 0, 0, "AT+CGNSPWR=0");
    gsmSerial.println("AT+CGNSPWR=0");
    delay(200);
    logger(0, 2, 200, 0, readSerialData(0, 1));
    //
    logger(0, 0, 0, 1, "Location failed");
    logger(0, 1, 0, 0, "AT+CGNSPWR=1");
    gsmSerial.println("AT+CGNSPWR=1");
    delay(200);
    logger(0, 2, 200, 0, readSerialData(0, 1));
    //
    logger(0, 0, 0, 1, "Location failed");
    logger(0, 1, 0, 0, "CGNSSEQ");
    gsmSerial.println("AT+CGNSSEQ=\"RMC\"");
    delay(200);
    logger(0, 2, 200, 0, readSerialData(0, 1));
    //
    helper = "";
    location = "";
    gsmSerial.println("AT+CGNSINF");
    delay(200);
    helper = readSerialData(0, 1);
    location = certainExploder(helper, 4) + "," + certainExploder(helper, 5);
    if (location == ",")
      location = "NA,NA";
  }
  lcd.clear();
  gsmSerial.flush();
  return 1;
}

int fillVechData()
{
  String res;
  engineTemp = vechSpeed = engineRpm = batVoltage = "";
  lcd.clear();
  //
  obdSerial.println("0105");
  delay(200);
  engineTemp = readSerialData(0, 2);
  //if(engineTemp.indexOf("41") == -1)
  //engineTemp = "NA";
  //else
  //{
  engineTemp.trim();
  engineTemp.replace(" ", "");
  engineTemp = engineTemp.substring(4, engineTemp.length());
  engineTemp = obdResponsePurify(engineTemp);
  //}
  //obdSerial.flush();
  //
  obdSerial.println("010D");
  delay(200);
  vechSpeed = readSerialData(0, 2);
  //if(vechSpeed.indexOf("41") == -1)
  //vechSpeed = "NA";
  //else
  //{
  vechSpeed.trim();
  vechSpeed.replace(" ", "");
  vechSpeed = vechSpeed.substring(4, vechSpeed.length());
  vechSpeed = obdResponsePurify(vechSpeed);
  //}
  //obdSerial.flush();
  //
  obdSerial.println("010C");
  delay(200);
  engineRpm = readSerialData(0, 2);
  //if(engineRpm.indexOf("41") == -1)
  //engineRpm = "NA";
  //else
  //{
  engineRpm.trim();
  engineRpm.replace(" ", "");
  engineRpm = engineRpm.substring(4, engineRpm.length());
  engineRpm = obdResponsePurify(engineRpm);
  //}
  //obdSerial.flush();
  //
  obdSerial.println("ATRV");
  delay(200);
  batVoltage = readSerialData(0, 2);
  //if(batVoltage.indexOf("V") == -1)
  //batVoltage = "NA";
  //else
  //{
  batVoltage.trim();
  batVoltage.replace(" ", "");
  batVoltage = batVoltage.substring(4, batVoltage.length());
  batVoltage = obdResponsePurify(batVoltage);
  //}
  //obdSerial.flush();
  //
  obdSerial.println("012F");
  delay(200);
  fuelLevel = readSerialData(0, 2);
  //if(batVoltage.indexOf("V") == -1)
  //batVoltage = "NA";
  //else
  //{
  fuelLevel.trim();
  fuelLevel.replace(" ", "");
  fuelLevel = fuelLevel.substring(4, fuelLevel.length());
  fuelLevel = obdResponsePurify(fuelLevel);
  //}
  //
  return 1;
}

int fillSensorsData()
{
  acc = rot = "";
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  //
  acc = String(a.acceleration.x, 2) + "," + String(a.acceleration.y, 2) + "," + String(a.acceleration.z, 2);
  rot = String(g.gyro.x, 2) + "," + String(g.gyro.y, 2) + "," + String(g.gyro.z, 2);
  temperature = String(temp.temperature);
  return 1;
}

void setup()
{
  pinMode(statusLed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  
  lcd.begin(20, 4);
  lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("looking mpu");
    delay(3000);

beginLbl:
  /*MPU INIT*/
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  if (!mpuSetup())
    goto beginLbl;
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  /*MPU INIT END*/

  /*GSM INIT*/
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  if (!simSetup())
  {
    lcd.setCursor(0, 0);
    lcd.print(message);
    delay(3000);
    goto beginLbl;
  }
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  /*GSM INIT END*/

  /*OBD INIT*/
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  if (!obdSetup())
    goto beginLbl;
  digitalWrite(statusLed, HIGH);
  delay(500);
  digitalWrite(statusLed, LOW);
  delay(500);
  /*OBD INIT END*/

  /*JSON INIT*/
  DynamicJsonBuffer jsonBuffer;
  /*JSON INIT END*/
  logger(0, 0, 500, 1, "Setup done");
  lcd.clear();
  digitalWrite(statusLed, HIGH);
}

void loop()
{
first:
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& object = jsonBuffer.createObject();
  String sendtoserver;
  //
  engineTemp = "";
  batVoltage = "";
  engineRpm = "";
  vechSpeed = "";
  fuelLevel = "";
  location = "";
  acc = "";
  rot = "";
  temperature = "";
  /***
    StaticJsonBuffer<256> jsonBuffer1;
    JsonObject& root = jsonBuffer1.createObject();
    root["city"] = "Paris";
    JsonObject& weather = root.createNestedObject("weather");
    weather["temp"] = 14.2;
    weather["cond"] = "cloudy";
    root.prettyPrintTo(Serial);
  **/
  lcd.clear();
  lcd.print("LETS BEGIN");
  delay(500);
  lcd.clear();
  if (!httpInit())
  {
    mayday();
    goto first;
  }
  fillLocationData();
  fillSensorsData();
  fillVechData();
  //
  object.set("devId", deviceID);
  object.set("loc", location);
  object.set("acc", acc);
  object.set("rot", rot);
  object.set("et", engineTemp);
  object.set("bv", batVoltage);
  object.set("er", engineRpm);
  object.set("vs", vechSpeed);
  object.set("fl", fuelLevel);
  object.set("tmp", temperature);
  //
  object.prettyPrintTo(sendtoserver);
  //
  if (!httpSend(sendtoserver))
  {
    mayday();
    goto first;
  }
  delay(500);
  if (!httpTerm())
  {
    mayday();
    goto first;
  }
  delay(1000);
}
