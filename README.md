# ArduinoEEPROM

EEPROM library with data integrity check, redundancy and wear leveling

## Features

* Can increase write cycles from ~100,000 to several million depending on EEPROM and data size
* Store rarely and frequently written data in separate areas
* Store data redundantly (2 or more copies per set)
* Data integrity checks
* Automatic rewrite if data structures are changed
* A few damaged EEPROM cells do not affect functionality
* EEPROM start offset and length can be adjusted

## Storage types

### Static data

Static data is rarely changed data that is stored in the beginning of the EEPROM. Multiple copies can be stored for redundancy. Each copy is stored with a header that contains the number of copy and a checksum, to identify the latest version and verify it's content.

### Wear leveling data

For data that is frequently changed and stored, the wear leveling area can distribute write cycles amonst EEPROM cells to increase its lifetime. Multiple copies can be stored for redundandy as well. The remaining part of the EEPROM is split into blocks containing a header with the cycle id and a checksum. The cycle id is a unique id that increases everytime a block is written to identify the recent version. Once the last block is written, it will start from the beginning, overwriting older versions.

### Example for write cycles

768 byte reserved for wear leveling data
11 byte of data ( + 6 byte header)
2 copies (total of 34 byte)
The wear leveling area will be divided into 45 blocks and 3 byte empty space at the end
22 copies can be stored before one of the older copies gets overwritten
The number of write cycles is reduced by 22.5 and the life of a EEPROM cell with 100,000 write cycles is extended to 2.25 million. This calculation assumes that the 11 byte data change every time and that the pattern does not repeat itself. Most likely this isn't true and not all bytes need to be written at all times.


## Static data and wear leveling data

Static data is stored at the beginning of the EEPROM. In the example, StaticData_t is used to define the structure of the static data. ARDUINO_EEPROM_STATIC_DATA_NUM_COPIES sets how many copies are stored for redundancy.

The area after the static data is called wear leveling area and is split into blocks of its structure. The class WearLevelData is used to define the structure in the example. Each block can have multiple copies set by ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES.

```
+------------------------+
+ Header #1              +
+ Static data copy #1    +
+ Header #2              +
+ Static data copy #2    +
+ Header #3              +
+ Static data copy #3    +
+------------------------+
+ Header #1              +
+ Wear Leveling Data #1  +
+ Header #1              +
+ Wear Lev. Data Copy #1 +
+ Header #2              +
+ Wear Leveling Data #2  +
+ Header #2              +
+ Wear Lev. Data Copy #2 +
....
+------------------------+
```


## Usage

Copy `example/arduino_eeprom_config.h` to your include directoriy and adjust the structure.

```
#include <ArduinoEEPROM.h>

ArduinoEEPROM myEEPROM; // does not occupy any global RAM

void setup()
{
    Serial.begin(115200);
    myEEPROM.begin();

    StaticData_t staticData = {0, 0, "Text"};
    if (!myEEPROM.readStaticData(staticData)) {
        staticData.myLong++;
        myEEPROM.writeStaticData(staticData);
    }

    WearLevelData wearLevelData;
    if (!mySecureStorage.readStaticData(wearLevelData)) {
        wearLevelData.myLong++;
        myEEPROM.writeWearLevelData(wearLevelData);
    }
}
```
