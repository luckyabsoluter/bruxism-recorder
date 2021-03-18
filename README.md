# bruxism-recorder

# Features
1. Bruxism Log(EMG signal)
2. Real Time Clock
3. Bluetooth Log Transfer
4. MicroSD Slot(Fat16, Fat32, exFat, ...) (See SdFat Library to see more information)
5. Log Visualization

# Parts
1. Board: ESPDuino-32
2. EMG: Gravity EMG Sensor by OYMotion
3. LCD: ILI9341 2.4inch 320x240 SPI Module
4. RTC: DS3231 I2C Module
5. Storage: MicroSD Card Adaptor Module
6. Connecter: Jumper Wire

# Pin Connection
See comments in `bruxismrecorder/bruxism_recorder.ino`.

# Libraries
Unzip `libraries.zip` to `Document/Arduino/`.

# Required Program
1. Arduino IDE

2. Arduino core for the ESP32 for Arduino IDE

https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/windows.md

Do not use `arduino-esp32 1.0.5`. It has bluetooth issue.
