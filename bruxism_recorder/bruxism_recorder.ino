#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <LinkedList.h>
#include <functional>
#include <sstream>
#include <iomanip>
#include <Int64String.h>

/* EMG Field Start *****/
// EMG's Data pin - ESP32's analog input: EMGSensorInputPin
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "EMGFilters.h"

#define EMGSensorInputPin 34

#define EMGSensorTime 100 // millisecond

EMGFilters myFilter;
SAMPLE_FREQUENCY sampleRate = SAMPLE_FREQ_1000HZ;
NOTCH_FREQUENCY humFreq = NOTCH_FREQ_50HZ;

// static int Threshold = 0;

LinkedList<const char*> emgList = LinkedList<const char*>();
/***** EMG Field End */





/* SDCard Field Start *****/
/*
 * SDCard SPI(VSPI)
 * IO18 - CS
 * IO19 - SCK
 * IO23 - MOSI
 * IO5  - MISO
 */

// #include "FS.h"
// #include "SD.h" // set pin in libraries/SD/src/SDSPI_Macro.h
// #include "SPI.h"

#include "SdFat.h"
SdFs SD;

#define SD_SS   18
#define SD_SCK  19
#define SD_MOSI 23
#define SD_MISO 5

SPIClass SD_SPI(VSPI);
#define SPI_CLOCK SD_SCK_MHZ(10) // need low for esp32
#define SD_CONFIG SdSpiConfig(SD_SS, DEDICATED_SPI, SPI_CLOCK, &SD_SPI)

bool sdError = false;
/***** SDCard Field End */





/* RTC Field Start *****/
// (I2C)
// SCL - pin SCL
// SDA - pin SDA

#include <Wire.h>

#define DS3231_I2C_ADDRESS 104

byte seconds, minutes, hours, day, date, month, year;
byte tMSB, tLSB;
float temp3231;

#include <ESP32Time.h>

ESP32Time rtc;
/***** RTC Field End */





/* LCD Field Start *****/
/*
 * LCD and TouchScreen SPI(HSPI) and Pin
 * IO13 - T_CS
 *
 * IO14 - MISO / T_OUT
 * IO27 - SCK / T_SCK
 * IO16 - MOSI / T_IN
 *
 * IO17 - DC
 * IO25 - RST
 * IO26 - LCD CS
 *
 * IO4  - LCD BL
 * IO35 - T_IRQ
 */
#include <TFT_eSPI.h> // set pin in libraries/TFT_eSPI/User_Setup.h
#include <SPI.h>

#define TIRQ_PIN 35

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite sprInfo = TFT_eSprite(&tft);

class Button;
LinkedList<Button*> buttonList = LinkedList<Button*>();
class Button {
public:
  const char* name;
  int xmin, xmax, ymin, ymax;
  int color;
  std::function<void()> func;
  bool enabled = true;
  int textX, textY;
  int w, h;
  
  Button(const char* _name, int _xmin, int _xmax, int _ymin, int _ymax, int _color, std::function<void()> _func) {
    name = _name;
    xmin = _xmin;
    xmax = _xmax;
    ymin = _ymin;
    ymax = _ymax;
    color = _color;
    func = _func;

    w = xmax - xmin + 1;
    h = ymax - ymin + 1;

    tft.setTextSize(2);
    textX = xmin + (w - tft.textWidth(name))/2;
    textY = ymin + (h - tft.fontHeight())/2;

    addInList();
  }

  bool isPressed(int x, int y) {
    return (xmin <= x && x <= xmax && ymin <= y && y <= ymax);
  }

  void action() {
    func();
  }

  void press(int x, int y) {
    if(enabled && isPressed(x, y)) {
      action();
    }
  }

  void enable() {
    enabled = true;
    
    tft.fillRect(xmin, ymin, w, h, color);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.drawString(name, textX, textY);
  }

  void disable() {
    enabled = false;
    
    tft.fillRect(xmin, ymin, w, h, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.drawString(name, textX, textY);
  }
  
  void invisible() {
      tft.fillRect(xmin, ymin, w, h, TFT_BLACK);
  }
  
  void draw(TFT_eSprite* spr) {
    if(enabled) {
        spr->fillRect(xmin, ymin, w, h, color);
        spr->setTextSize(2);
        spr->setTextColor(TFT_BLACK);
        spr->drawString(name, textX, textY);
    } else {
        spr->fillRect(xmin, ymin, w, h, TFT_DARKGREY);
        spr->setTextSize(2);
        spr->setTextColor(TFT_BLACK);
        spr->drawString(name, textX, textY);
    }
  }

  void addInList() {
   buttonList.add(this);
  }
};

LinkedList<Button*> buttonEnableList;
LinkedList<const char*> buttonDeleteList;
/***** LCD Field End */





/* Bluetooth Field Start *****/
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
int bluetoothConfirmNumber = -1;

long BTPairingShow = 0;
bool BTPairingSuccess = false;
/***** Bluetooth Field End */





/* Extra Field Start *****/
#define FILE_WRITE (O_RDWR | O_CREAT | O_AT_END)
/***** Extra Field End */





bool taskBluetooth, taskEMG, taskSDCard, taskRTC, taskLCD;

void setup() {
 // Serial.begin(115200);





  /***** EMG Init Start */
  myFilter.init(sampleRate, humFreq, true, true, true);
  /* EMG Init End *****/





  /* SDCard Init Start *****/
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS);
  if(!SD.begin(SD_CONFIG)) {
    // SD.initErrorPrint(&Serial);
    sdError = true;
  }
  /***** SDCard Init End */





  /* RTC Init Start *****/
  Wire.begin();
  get3231Date(); // D3231(external RTC) -> RAM

  rtc.setTime(seconds, minutes, hours, date, month, year + 2000); // RAM -> ESP32 RTC(internal RTC)

  // Serial.println(rtc.getTime("%Y%m%d%H%M%S")); // Time debug
  /***** RTC Init End */
  
  
  
  
  
  /* LCD Init Start *****/
  pinMode(TFT_BL, OUTPUT);
  pinMode(TIRQ_PIN, INPUT);

  tft.init();
  setRotation(0);

  xTaskCreatePinnedToCore(TaskLCD, "TaskLCD", 1024*2, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
  /***** LCD Init End */
  




  /* Bluetooth Init Start *****/
  SerialBT.enableSSP();
  SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.register_callback(BTCallback);
  SerialBT.begin("Bruxism Recorder");
  /***** Bluetooth Init End */

  // Serial.println("init completely");
}

void loop() {}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

/* EMG Function Start *****/
void TaskEMG(void *pvParameters) {
  (void) pvParameters;

  taskEMG = true;

  long t = millis();

  for(;;) {
    if(!taskEMG) vTaskDelete(NULL);

    t += EMGSensorTime;

    int emg = readEMG();
    String value = rtc.getTime("%Y%m%d%H%M%S") + to_format(rtc.getMillis(), 3) + ":" + emg;
    emgList.add(copy_str(value));

    vTaskDelay(EMGSensorTime - (millis() - t));
  }
}

const char* copy_str(String& str) {
  int len = str.length();
  char *buffer = new char[len + 1];   //we need extra char for NUL
  memcpy(buffer, str.c_str(), len + 1);
  
  return buffer;
}

int readEMG() {
  int Value = analogRead(EMGSensorInputPin);

  // filter processing
  int DataAfterFilter = myFilter.update(Value);

  int envlope = sq(DataAfterFilter);
  // any value under threshold will be set to zero
  // envlope = (envlope > Threshold) ? envlope : 0;
  
  return envlope;
}
/***** EMG Function End */





/* SDCard Function Start */
void TaskSDCard(void *pvParameters) {
  taskSDCard = true;

  createDir(SD, getLogDirectoryPath());
  String filePath = getLogFilePath();

  const String newLine = String("\n");

  for(;;) {
    if(!taskSDCard) vTaskDelete(NULL);

    while(emgList.size() != 0) {
      const char* value = emgList.get(0);
      String str = value + newLine;
      bool success = writeFile(SD, filePath.c_str(), str.c_str());
      emgList.remove(0);
      delete value;
    }
  }
}

String getLogFilePath() {
  return String("/bruxismLog/bruxismLog-" + rtc.getTime("%Y%m%d") + ".txt");
}

const char* getLogDirectoryPath() {
  return "/bruxismLog/";
}

String listDir(SdFs &fs, const char * dirname, uint8_t levels){
//    Serial.printf("Listing directory: %s\n", dirname);

    FsFile root = fs.open(dirname);
    if(!root){
        // Serial.println("Failed to open directory");
        return String();
    }
    if(!root.isDirectory()){
        // Serial.println("Not a directory");
        return String();
    }

    String str;

    FsFile file = root.openNextFile();
    while(file){
        char name[255];
        file.getName(name, 255);
        if(file.isDirectory()){
            str += 'D';
            str += name;
            str += '/';
            if(levels){
                str += listDir(fs, name, levels -1);
            }
        } else {
            str += 'F';
            str += name;
            str += ':';
            str += int64String(file.size());
            str += '/';
        }
        file = root.openNextFile();
    }

    return str;
}

void createDir(SdFs &fs, const char * path){
    // Serial.printf("Creating Dir: %s\n", path);
    if(fs.exists(path)) {
      // Serial.println("file exists");
    } else {
      if(fs.mkdir(path)){
          // Serial.println("Dir created");
      } else {
          // Serial.println("mkdir failed");
      }
    }
}

void readFile(SdFs &fs, const char * path){ // need modify
    // Serial.printf("Reading file: %s\n", path);

    FsFile file = fs.open(path);
    if(!file){
        // Serial.println("Failed to open file for reading");
        return;
    }

    // Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

bool writeFile(SdFs &fs, const char * path, const char * message){
//    Serial.printf("Writing file: %s\n", path);
    FsFile file = fs.open(path, FILE_WRITE);
    if(!file){
        // Serial.println("Failed to open file for writing");
        return false;
    }
    if(file.print(message)){
        // Serial.println("File written");
        file.close();
        return true;
    } else {
        // Serial.println("Write failed");
        file.close();
        return false;
    }
}
/* SDCard Function End */





/* LCD Function Start */
const int
  xmin1_1 = 0,
  xmax1_1 = tft.width() - 1,//239,
  
  xmin2_1 = 0,
  xmax2_1 = tft.width()/2 - 1,//119,
  xmin2_2 = tft.width()/2,//120,
  xmax2_2 = tft.width() - 1;//239;
void TaskLCD(void *pvParameters) {
  backlightOn();

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);

  Button* startButton;
  Button* stopButton;

  long measurementTime = 0;
  bool measurement = false;

  startButton = new Button("Start", xmin2_1, xmax2_1, 260, 319, TFT_DARKGREEN, [&startButton, &stopButton, &measurementTime, &measurement](){
    stopButton->enable();
    startButton->disable();
    
    xTaskCreatePinnedToCore(TaskEMG, "TaskEMG", 1024*2, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
    xTaskCreatePinnedToCore(TaskSDCard, "TaskSDCard", 1024*10, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
    
    measurementTime = millis();
    measurement = true;
  });
  startButton->enable();
  stopButton = new Button("Stop", xmin2_2, xmax2_2, 260, 319, TFT_BLUE, [&startButton, &stopButton, &measurementTime, &measurement](){
    startButton->enable();
    stopButton->disable();
    
    taskEMG = false;
    taskSDCard = false;
    
    measurement = false;
  });
  stopButton->disable();

  bool sleep = false;
  Button sleepButton = Button("Sleep", xmin1_1, xmax1_1, 140, 259, TFT_NAVY, [&sleep](){
    sleep = true;
    backlightOff();
  });
  sleepButton.enable();
  
  sprInfo.setTextSize(1);
  sprInfo.createSprite(240, sprInfo.fontHeight() * 5);

  bool pressed = false;
  bool pressedPrevious = false;
  uint16_t x, y;
  for(;;) {
    // Information
    sprInfo.fillSprite(TFT_BLACK);
    sprInfo.setTextSize(1);
    sprInfo.setTextColor(TFT_WHITE);
    sprInfo.setCursor(0, 0);
    sprInfo.print(rtc.getTime("Date Time: %Y-%m-%d %H:%M:%S\n"));
    if(measurement) {
      int t = (millis() - measurementTime)/1000;
      int hour = (t/(60*60));
      int minute = (t/60)%60;
      int second = (t%60);
      sprInfo.println(String("Measurement Time: ") + hour + "h " + minute + "m " + second + "s");
    }
    if(sdError) {
      sprInfo.println("SDCard Error");
    }
    if(bluetoothConfirmNumber != -1) { // BT pairing confirm number
      sprInfo.println(String("Bluetooth Pairing Confirm: ") + bluetoothConfirmNumber);
    }
    if(BTPairingShow > millis()) { // BT pairing result
      sprInfo.println(String("Bluetooth Pairing Result(") + ((BTPairingShow - millis())/1000) + "): " + BTPairingSuccess);
    }
    sprInfo.pushSprite(0, 0);
    
    // Touch Process
    pressedPrevious = pressed;
    pressed = false;
    if (!digitalRead(TIRQ_PIN)) {
      if(tft.getTouch(&x, &y)) {
        pressed = true;
      }
    }

    if(sleep && pressed) { // screen on
      backlightOn();
    }

    if(pressedPrevious && !pressed) { // released
        if(sleep) {
          sleep = false;
        } else {
          buttonPressedCheck(x, y);
        }
    }

    // button list process
    for(int i = buttonEnableList.size() - 1; 0 <= i; --i) {
      buttonEnableList.get(i)->enable();
      buttonEnableList.remove(i);
    }
    for(int i = buttonDeleteList.size() - 1; 0 <= i; --i) {
      deleteButton(buttonDeleteList.get(i));
      buttonDeleteList.remove(i);
    }

  }
}

void buttonPressedCheck(int x, int y) {
  int size = buttonList.size();
  for(int i = 0; i < size; ++i) {
    buttonList.get(i)->press(x, y);
  }
}

void deleteButton(const char* name) {
  int size = buttonList.size();
  for(int i = size - 1; 0 <= i; --i) {
    Button* button = buttonList.get(i);
    if(String(button->name).equals(name)) {
      button->invisible();
      buttonList.remove(i);
      delete button;
    }
  }
}

void setRotation(uint8_t rotation) {
  // binary invert_y invert_x rotate
  // rotation_index[] = {rotation[0].calDate[4], rotation[1].calDate[4], ...}
  uint16_t const rotation_index[] = {6, 3, 0, 5};//{6, 3, 0, 5}
  uint16_t const calData[4] = { 200, 3600, 400, 3500 }; // calData excluding index 4
  uint8_t const calRotation = 0; // rotation when you calibrate bringing calData

  if(!(0 <= rotation && rotation <= 3)) return;

  if(rotation%2 == calRotation%2) {
    uint16_t parameters[] = {calData[0], calData[1], calData[2], calData[3], rotation_index[rotation]};
    tft.setTouch(parameters);
  } else {
    uint16_t parameters[] = {calData[2], calData[3], calData[0], calData[1], rotation_index[rotation]};
    tft.setTouch(parameters);
  }

  tft.setRotation(rotation);
}

void backlightOn() { digitalWrite(TFT_BL, HIGH); }
void backlightOff() { digitalWrite(TFT_BL, LOW); }
/***** LCD Function End */



  

/* Bluetooth Function Start *****/
int btConnected = 0;
TaskHandle_t btHandle;
void TaskBluetooth(void *pvParameters) {
  (void) pvParameters;

  taskBluetooth = true;

  byte flag = 0;
  String s;
  for(;;) {
    if(!taskBluetooth) vTaskDelete(NULL);
    
    if (SerialBT.available()) {
      byte b = SerialBT.read();

      if(!flag) {
        if(b == '1') { // read file
          flag = '1';
          // Serial.println("bluetooth read file");
        } else if(b == '2') { // read directory
          flag = '2';
          // Serial.println("bluetooth read directory");
        }
      } else {
        if(b != '*') { // '*'
          s += char(b);
        } else {
          if(flag == '1') {
            fileSendBT(s.c_str());
            flag = 0;
            s = String();
          } else if(flag == '2') {
            sendListDirBT(s.c_str());
            flag = 0;
            s = String();
          }
        }
      }
    }
  }
}

void fileSendBT(const char* fileName) {
    FsFile file = SD.open(fileName, O_RDONLY);
    if(!file) {
      // SD.errorPrint(&Serial, F("open failed"));
      return;
    }
    SerialBT.print(file.size());
    SerialBT.write(':');
    while (file.available()) {
      if(!taskBluetooth) break;
      
      int b = file.read();
      SerialBT.write(b);
    }
    file.close();
}

void sendListDirBT(const char* dirName) {
  String str = listDir(SD, dirName, 0);

  // Serial.print(str.length());
  // Serial.write(':');
  // Serial.println(str.c_str());
  
  SerialBT.print(str.length());
  SerialBT.write(':');
  SerialBT.print(str.c_str());
}

void TaskBluetoothRequest(void *pvParameters) {
  (void) pvParameters;

  Button* bluetoothAcceptButton, *bluetoothDenyButton;
  
  bluetoothAcceptButton = new Button("Accept", xmin2_1, xmax2_1, 90, 139, TFT_BLUE, [&bluetoothAcceptButton, &bluetoothDenyButton](){
    SerialBT.confirmReply(true);
    bluetoothConfirmNumber = -1;
    bluetoothAcceptButton->disable();
    bluetoothDenyButton->disable();
  });
  buttonEnableList.add(bluetoothAcceptButton);
  
  bluetoothDenyButton = new Button("Deny", xmin2_2, xmax2_2, 90, 139, TFT_RED, [&bluetoothAcceptButton, &bluetoothDenyButton](){
    SerialBT.confirmReply(false);
    bluetoothConfirmNumber = -1;
    bluetoothAcceptButton->disable();
    bluetoothDenyButton->disable();
  });
  buttonEnableList.add(bluetoothDenyButton);

  vTaskDelete(NULL);
}

void TaskBluetoothResult(void *pvParameters) {
  (void) pvParameters;
  
  buttonDeleteList.add("Accept");
  buttonDeleteList.add("Deny");

  BTPairingShow = millis() + 10 * 1000;

  vTaskDelete(NULL);
}

void BTConfirmRequestCallback(uint32_t numVal) {
  bluetoothConfirmNumber = numVal;
  xTaskCreatePinnedToCore(TaskBluetoothRequest, "TaskBluetoothRequest", 1024, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}

void BTAuthCompleteCallback(boolean success) {
  if(success) {
    // Serial.println("Pairing success!!");
    BTPairingSuccess = true;
  } else {
    // Serial.println("Pairing failed, rejected by user!!");
    BTPairingSuccess = false;
  }
  
  xTaskCreatePinnedToCore(TaskBluetoothResult, "TaskBluetoothResult", 1024, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}

void BTCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    // Serial.println("Client Connected");
    ++btConnected;
    // Serial.println(btConnected);

    if(btConnected == 1) {
      while(SerialBT.read() >= 0);
      SerialBT.flush();
      xTaskCreatePinnedToCore(TaskBluetooth, "TaskBluetooth", 1024*2, NULL, 1, &btHandle, ARDUINO_RUNNING_CORE);
    }
  }
 
  if(event == ESP_SPP_CLOSE_EVT ){
    // Serial.println("Client disconnected");
    --btConnected;
    // Serial.println(btConnected);

    if(btConnected == 0) {
      taskBluetooth = false;
    }
  }
}
/***** Bluetooth Function End */





/* RTC Function Start *****/
void set3231Date(byte _year, byte _month, byte _date, byte _hours, byte _minutes, byte _seconds, byte _day) {
  // Sun1, Mon2, Tue3, Wed4, Thu5, Fri6, Sat7
  // 2021-3-5, 12:30 PM, 0s, Friday
  // set3231(21, 3, 5, 12, 30, 0, 6);
 
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(_seconds));
  Wire.write(decToBcd(_minutes));
  Wire.write(decToBcd(_hours));
  Wire.write(decToBcd(_day));
  Wire.write(decToBcd(_date));
  Wire.write(decToBcd(_month));
  Wire.write(decToBcd(_year));
  Wire.endTransmission();
}

byte decToBcd(byte val) {
  return ( (val/10*16) + (val%10) );
}
 
bool get3231Date() {
  // send request to receive data starting at register 0
  Wire.beginTransmission(DS3231_I2C_ADDRESS); // 104 is DS3231 device address
  Wire.write(0x00); // start at register 0
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7); // request seven bytes
 
  if(Wire.available()) {
    seconds = Wire.read(); // get seconds
    minutes = Wire.read(); // get minutes
    hours   = Wire.read();   // get hours
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); //temp month
    year    = Wire.read();
       
    seconds = (((seconds & B11110000)>>4)*10 + (seconds & B00001111)); // convert BCD to decimal
    minutes = (((minutes & B11110000)>>4)*10 + (minutes & B00001111)); // convert BCD to decimal
    hours   = (((hours & B00110000)>>4)*10 + (hours & B00001111)); // convert BCD to decimal (assume 24 hour mode)
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000)>>4)*10 + (date & B00001111)); // 1-31
    month   = (((month & B00010000)>>4)*10 + (month & B00001111)); //msb7 is century overflow
    year    = (((year & B11110000)>>4)*10 + (year & B00001111));

    return true;
  }
  else {
    //oh noes, no data!
    return false;
  }
}
 
bool get3231Temp() { // temperature
  //temp registers (11h-12h) get updated automatically every 64s
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x11);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 2);
 
  if(Wire.available()) {
    tMSB = Wire.read(); //2's complement int portion
    tLSB = Wire.read(); //fraction portion
   
    temp3231 = (tMSB & B01111111); //do 2's math on Tmsb
    temp3231 += ( (tLSB >> 6) * 0.25 ); //only care about bits 7 & 8

    return true;
  }
  else {
    //error! no data!
    return false;
  }
}

String to_format(const int number, const int length) {
    std::stringstream ss;
    ss << std::setw(length) << std::setfill('0') << number;
    return String(ss.str().c_str());
}
/***** RTC Function End */
