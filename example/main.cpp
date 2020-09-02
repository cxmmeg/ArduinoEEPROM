/**
 * Author: sascha_lammers@gmx.de
 */

#include "ArduinoEEPROM.h"

ArduinoEEPROM AE;

void setup()
{
    Serial.begin(115200);
    AE.begin();

    StaticData_t staticData;

    if (AE.readStaticData(staticData)) {
        staticData.myLong++;
    } else {
        StaticData_t tmp = {100, 100000,  "ab"};
        memcpy(&staticData, &tmp, sizeof(tmp));
        AE.eraseAndInitialize(ArduinoEEPROM::DataTypeEnum::ALL);
    }

    AE.writeStaticData(staticData);

    AE.writeWearLevelData(WearLevelData());

    Serial.print("Counter ");
    Serial.println((double)staticData.myLong, 0);

    //AE.dump(Serial);

}

void loop()
{
    WearLevelData data;
    if (AE.readWearLevelData(data)) {
        AE.writeWearLevelData(data);
        // AE.dump(Serial);
    }
    Serial.println("\ndelay 30s...\n");
    delay(30 * 1000UL);
}
