#include "OOCSI.h"
#include "Wire.h"
#include "Math.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/* The basic use of the gyroscope was deployed following the tutorial for GY-521 
 * https://olivertechnologydevelopment.wordpress.com/2017/08/17/esp8266-sensor-series-gy-521-imu-part-1/
 * The pieces of code (some modified) I have marked with comments *Gyroscope* and *Gyroscope end*
 */



/* Gyroscope */
const uint8_t MPU_addr=0x69; // I2C address of the MPU-6050
 
const float MPU_GYRO_250_SCALE = 131.0;
const float MPU_GYRO_500_SCALE = 65.5;
const float MPU_GYRO_1000_SCALE = 32.8;
const float MPU_GYRO_2000_SCALE = 16.4;
const float MPU_ACCL_2_SCALE = 16384.0;
const float MPU_ACCL_4_SCALE = 8192.0;
const float MPU_ACCL_8_SCALE = 4096.0;
const float MPU_ACCL_16_SCALE = 2048.0;
  
struct rawdata {
int16_t AcX;
int16_t AcY;
int16_t AcZ;
int16_t Tmp;
int16_t GyX;
int16_t GyY;
int16_t GyZ;
};

/*Some modification*/
struct scaleddata{
float AcXOld;
float AcYOld;
float AcZOld;
float AcXNew;
float AcYNew;
float AcZNew;
float Tmp;
float GyX;
float GyY;
float GyZ;
};
 
bool checkI2c(byte addr);
void mpu6050Begin(byte addr);
rawdata mpu6050Read(byte addr, bool Debug);
void setMPU6050scales(byte addr,uint8_t Gyro,uint8_t Accl);
void getMPU6050scales(byte addr,uint8_t &Gyro,uint8_t &Accl);
scaleddata convertRawToScaled(byte addr, rawdata data_in,bool Debug);
/*Gyroscope end*/

const char* message = "def";
float difference;
float absDiff;
boolean counter = false;
String minS;
boolean allow = false;
boolean triggered = false;
int minT;
unsigned long currentMillis;
unsigned long ending = 120000; 

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp; 

//Hardcoding credentials for a personal hotspot
const char* ssid = "Natalia"; 
const char* password = "golnata03"; 

//setting up OOCSI
const char* OOCSIName = "E5_sensor"; 
const char* hostserver = "oocsi.id.tue.nl";
OOCSI oocsi = OOCSI();

//Variables for the decetralization of the world
int lock[5] = {0, 0, 0, 0, 0};
int standardarray[5] = {1, 1, 1, 1, 1};
int position1 = 1;
int position2 = 1;
int position3 = 4;
int position0 = 0;
int number = 5;
int alive = -2;
int dead = -1;

void setup() {  
  Serial.begin(9600);
  oocsi.connect(OOCSIName, hostserver, ssid, password, receiveOOCSI);
  oocsi.subscribe("Erilea"); 
  
  Wire.begin();
  
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
+
  // Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(3600);  //GMT +1 Amsterdam local time

  /*Gyroscope */
  mpu6050Begin(MPU_addr);
  /*Gyroscope end*/
  
}

void loop() {
  check_time(); 

  //waiting for signal
  oocsi.check();  
}

//Call this method once it is our module time to measure
void measure() {
  //For debugging
  Serial.println("MEASURE");

  int previousMillis = millis();

  //timeout after 2 minutes
  while (millis() - previousMillis < ending){
    check_time();

    /*Gyroscope*/
    rawdata next_sample;
    setMPU6050scales(MPU_addr,0b00000000,0b00010000);
    next_sample = mpu6050Read(MPU_addr, true);
    convertRawToScaled(MPU_addr, next_sample,true);
    scaleddata convertRawToScaled(byte addr, rawdata data_in,bool Debug); 
    /*Gyroscope end*/
    
    if (triggered){
      message = "triggered";
      lock[position0] = number;
      sendOOCSI();
      check_alive();
      return;
      } 
    }
  
  for (int i = 0; i < 5; i++) {
    lock[i] = 0;
    }
    message = "NOT triggered";
    sendOOCSI();
}

//method for checking the time and (dis)allowing based on the minutes
void check_time(){
  timeClient.forceUpdate();
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
 
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.print("HOUR: ");
  Serial.println(timeStamp); 
  minS = timeStamp.substring(4,6);
  minT = minS.toInt();


  //allowing trigger for odd minutes only
  if (minT == 1 || minT == 3 || minT == 5 || minT == 7 || minT == 9){
    allow = true;
    Serial.print("ALLOW LOOP" );
    Serial.println(allow);
  } else {
    allow = false;
     Serial.print("DISALLOW LOOP" );
    Serial.println(allow);
  }
}



void check_alive() {
  for (int i = position0 + 1; i < 5; i++) {
    int startingtime = millis();
    while (millis() < startingtime + 10000) {
      oocsi.check();
      delay(50);
    }
    if (lock[i] == 0) {
      lock[i] = dead;
      if (reset()) {
        for (int j = 0; j < 5; j++) {
          lock[j] = 0;
        }
        sendOOCSI();
        return;
      }
      sendOOCSI();
    }
    else {
      return;
    }
  }
}

//resetting method
bool reset() {
  int count = 0;
  for (int i = 0; i < 5; i++) {
    if (lock[i] == -1) {
      count++;
    }
  }
  if (count >= 2) {
    return true;
  }
  else {
    return false;
  }
}

void sendOOCSI() {
  //sending to the Erilea communication channel
  oocsi.newMessage("Erilea");
  oocsi.addIntArray("eA", lock, 5);
  oocsi.sendMessage();
  oocsi.check(); 

  //sending to a personal channel for debugging
  /* oocsi.newMessage("testSZ"); //ghthtthttt
  oocsi.addString("message", message);
  oocsi.sendMessage();
  oocsi.check(); */
  
}

void receiveOOCSI() {
  Serial.println("RECEIVE OOCSI");
  oocsi.getIntArray("eA", standardarray, lock, 5);
  //route 1
  if (lock[0] == 4) { 
    Serial.println("SENSOR 4");
    if (lock[position1] == 0) {
      for (int i = 0; i < position1; i++) {
        if (lock[i] == 0 || lock[i] == alive) {
          return;
        }
      }
      lock[position1] = alive;
      sendOOCSI();
      position0 = position1;
      measure();
    }
  }  //route 2
  else if (lock[0] == 1) {
    if (lock[position2] == 0) {
      for (int i = 0; i < position2; i++) {
        if (lock[i] == 0 || lock[i] == alive) {
          return;
        }
      }
      lock[position2] = alive;
      sendOOCSI();
      position0 = position2;
      measure();
    }
  } //route 3
  else if (lock[0] == 2) {
    if (lock[position3] == 0) {
      for (int i = 0; i < position3; i++) {
        if (lock[i] == 0 || lock[i] == alive) {
          return;
        }
      }
      lock[position3] = alive;
      sendOOCSI();
      position0 = position3;
      measure();
    }
  } 
}

/*Gyroscope*/
void mpu6050Begin(byte addr){
// This function initializes the MPU-6050 IMU Sensor
// It verifys the address is correct and wakes up the
// MPU.
  if (checkI2c(addr)){
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0); // set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
     
    delay(30); // Ensure gyro has enough time to power up
  }
}

bool checkI2c(byte addr){
// We are using the return value of
// the Write.endTransmisstion to see if
// a device did acknowledge to the address.
  Serial.println(" ");
  Wire.beginTransmission(addr);
   
  if (Wire.endTransmission() == 0) {
    Serial.print(" Device Found at 0x");
    Serial.println(addr,HEX);
    return true;
  } else {
    Serial.print(" No Device Found at 0x");
    Serial.println(addr,HEX);
    return false;
  }
}

rawdata mpu6050Read(byte addr, bool Debug){
// This function reads the raw 16-bit data values from
// the MPU-6050
 
  rawdata values;
   
  Wire.beginTransmission(addr);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr,14,true); // request a total of 14 registers
  values.AcX=Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  values.AcY=Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  values.AcZ=Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  values.Tmp=Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  values.GyX=Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  values.GyY=Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  values.GyZ=Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
  
   if(Debug){
    Serial.print(" GyX = "); Serial.print(values.GyX);
    Serial.print(" | GyY = "); Serial.print(values.GyY);
    Serial.print(" | GyZ = "); Serial.print(values.GyZ);
    Serial.print(" | Tmp = "); Serial.print(values.Tmp);
    Serial.print(" | AcX = "); Serial.print(values.AcX);
    Serial.print(" | AcY = "); Serial.print(values.AcY);
    Serial.print(" | AcZ = "); Serial.println(values.AcZ);
  }
   
  return values;
}
 
void setMPU6050scales(byte addr,uint8_t Gyro,uint8_t Accl){
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // write to register starting at 0x1B
  Wire.write(Gyro); // Self Tests Off and set Gyro FS to 250
  Wire.write(Accl); // Self Tests Off and set Accl FS to 8g
  Wire.endTransmission(true);
}
 
void getMPU6050scales(byte addr,uint8_t &Gyro,uint8_t &Accl){
  Wire.beginTransmission(addr);
  Wire.write(0x1B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(addr,2,true); // request a total of 14 registers
  Gyro = (Wire.read()&(bit(3)|bit(4)))>>3;
  Accl = (Wire.read()&(bit(3)|bit(4)))>>3;
}
 
  
 
scaleddata convertRawToScaled(byte addr, rawdata data_in, bool Debug){
 
  scaleddata values;
  rawdata valuesOld;
  
  float scale_value = 0.0;
  byte Gyro, Accl;
   
  getMPU6050scales(MPU_addr, Gyro, Accl);
  
   if(Debug){
     Serial.print("Gyro Full-Scale = ");
  }
 
  switch (Gyro){
    case 0:
      scale_value = MPU_GYRO_250_SCALE;
      if(Debug){
        Serial.println("±250 °/s");
      }
      break;
    case 1:
      scale_value = MPU_GYRO_500_SCALE;
      if(Debug){
        Serial.println("±500 °/s");
      }
      break;
    case 2:
      scale_value = MPU_GYRO_1000_SCALE;
      if(Debug){
        Serial.println("±1000 °/s");
      }
      break;
    case 3:
      scale_value = MPU_GYRO_2000_SCALE;
      if(Debug){
        Serial.println("±2000 °/s");
      }
      break;
    default:
    break;
  }
 
  values.GyX = (float) data_in.GyX / scale_value;
  values.GyY = (float) data_in.GyY / scale_value;
  values.GyZ = (float) data_in.GyZ / scale_value;
  scale_value = 0.0;

  if(Debug){
    Serial.print("Accl Full-Scale = ");
  }
  
  switch (Accl){
    case 0:
      scale_value = MPU_ACCL_2_SCALE;
      if(Debug){
        Serial.println("±2 g");
      }
      break;
    case 1:
      scale_value = MPU_ACCL_4_SCALE;
      if(Debug){
        Serial.println("±4 g");
      }
      break;
    case 2:
      scale_value = MPU_ACCL_8_SCALE;
      if(Debug){
        Serial.println("±8 g");
      }
      break;
    case 3:
      scale_value = MPU_ACCL_16_SCALE;
      if(Debug){
        Serial.println("±16 g");
      }
      break;
    default:
    break;
  }

 /*Modified tutorial*/
  if( valuesOld.AcX == NULL || counter == false) {
    values.AcXOld=(float) data_in.AcX / scale_value;
    counter = true;
  } else if ( counter == true ) {
    values.AcXNew=(float) data_in.AcX / scale_value;
    difference = values.AcXOld - values.AcXNew;

    //since the abs() function does not work do it manually
    if ( difference < 0 ) {
      absDiff = -difference;
    } else {
      absDiff = difference;
    }
    
    Serial.println(" ");
    Serial.print(" absDiff: "); 
    Serial.println(absDiff);
  }

  if( absDiff > 0.30 && allow ){ //trigger only on these conditions
    
      values.AcXOld = values.AcXNew; 
      Serial.print(" if difference  : ");
      Serial.print(message);
      triggered = true;
      
      /*message = "triggered"; //for debugging
      oocsi.addString("message", "paper");
      oocsi.sendMessage();
      oocsi.check(); */
      
    } else {
      triggered = false; 
      Serial.print(" else difference  : ");
    }

    /*End of modification*/

  values.Tmp = (float) data_in.Tmp / 340.0 + 36.53;
   
  if(Debug){
    Serial.print(" AcXOld = "); Serial.print(values.AcXOld);
    Serial.print(" AcXNew = "); Serial.print(values.AcXNew);
    Serial.print(" difference "); Serial.println(difference);
  }

  return values;
}
/*Gyroscope end*/
