/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

typedef struct __attribute__((packed)) {
    uint8_t myByte;
    uint32_t myLong;
    char myText[3];
} StaticData_t;

class WearLevelData { // example for packed class
public:
    WearLevelData() = default;
    struct __attribute__((packed)) {
        uint8_t _myByte;
        uint32_t _myLong;
    };
};

#define ARDUINO_EEPROM_STATIC_DATA_SIZE                     sizeof(StaticData_t)
#define ARDUINO_EEPROM_WEAR_LEVEL_DATA_SIZE                 sizeof(WearLevelData)
#define ARDUINO_EEPROM_LENGTH                               1024
#define ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE             0

template<class StaticDataType, class WearLevelDataType> class ArduinoEEPROMTpl;

using ArduinoEEPROM = ArduinoEEPROMTpl<StaticData_t, WearLevelData>;
