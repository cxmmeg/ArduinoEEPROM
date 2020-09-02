/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <arduino_eeprom_config.h>
#include "crc16.h"
#include "ByteAccessInterface.h"

// supports any type of EEPROM that is organized in single bytes or words
// and has a class that is compatible with the Arduino EEPROMClass (read(), write(), get())

#ifndef ARDUINO_EEPROM_STATIC_DATA_SIZE
#error ARDUINO_EEPROM_STATIC_DATA_SIZE not defined
#endif

#ifndef ARDUINO_EEPROM_WEAR_LEVEL_DATA_SIZE
#error ARDUINO_EEPROM_WEAR_LEVEL_DATA_SIZE not defined
#endif

 // start offset
#ifndef ARDUINO_EEPROM_START_OFFSET
#define ARDUINO_EEPROM_START_OFFSET                         0
#endif

// max length
#ifndef ARDUINO_EEPROM_LENGTH
#define ARDUINO_EEPROM_LENGTH                               (ARDUINO_EEPROM_MAX_LENGTH - ARDUINO_EEPROM_START_OFFSET)
#endif

// addresses will be aligned to the page size
#ifndef ARDUINO_EEPROM_PAGE_SIZE
#define ARDUINO_EEPROM_PAGE_SIZE                            1
#endif

// pass EEPROM object as reference during initialization
#ifndef ARDUINO_EEPROM_PASS_BY_REF
#define ARDUINO_EEPROM_PASS_BY_REF                          0
#endif

// number of copies of static data
// less than 2 will not ensure data integrity
#ifndef ARDUINO_EEPROM_STATIC_DATA_NUM_COPIES
#define ARDUINO_EEPROM_STATIC_DATA_NUM_COPIES               3
#endif
#if ARDUINO_EEPROM_STATIC_DATA_NUM_COPIES > 8
#error Limited to 8
#endif

// number of copies of data stored in the wear leveling area
// if less than 2, previously stored data can be returned if the latest data
// cannot be read
#ifndef ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES
#define ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES           2
#endif

#ifndef ARDUINO_EEPROM_WRITE_ERROR_RETRIES
#define ARDUINO_EEPROM_WRITE_ERROR_RETRIES                  3
#endif

// if the size of the data structures changes with a firmware upgrade, the stored
// data will be rewritten instead of becoming invalid. additional space filled with
// zeros. this requires sizeof(WearLevelData_t) * (ARDUINO_EEPROM_NUM_BACKUPS + 1)
// free RAM when executed
#ifndef ARDUINO_EEPROM_AUTO_RESIZE
//TODO
#define ARDUINO_EEPROM_AUTO_RESIZE                          0
#endif
#if ARDUINO_EEPROM_AUTO_RESIZE
#error not implemeneted
#endif

#ifndef ARDUINO_EEPROM_MAX_LENGTH
#if ARDUINO && defined(E2END)
#define ARDUINO_EEPROM_MAX_LENGTH                           (E2END + 1)
#elif !defined(ARDUINO_EEPROM_MAX_LENGTH)
//||ARDUINO_EEPROM_MAX_LENGTH2 < 0
#error define ARDUINO_EEPROM_MAX_LENGTH = EEPROM.length()
#endif
#endif

// class of the EEPROM
#ifndef ARDUINO_EEPROM_CLASS
#define ARDUINO_EEPROM_CLASS                                ::EEPROMClass
#endif

// EEPROM object used by default
// set to -1 if ARDUINO_EEPROM_PASS_BY_REF is used and no default constructor is required
#ifndef ARDUINO_EEPROM_OBJECT
#define ARDUINO_EEPROM_OBJECT                               EEPROM

#endif

#if (ARDUINO_EEPROM_LENGTH + ARDUINO_EEPROM_START_OFFSET) <= 255
typedef uint8_t eeprom_size_t;
#else
typedef uint16_t eeprom_size_t;
#endif

#ifndef ARDUINO_EEPROM_STATIC_DATA_TYPE
#define ARDUINO_EEPROM_STATIC_DATA_TYPE                     StaticData_t
#endif

#ifndef ARDUINO_EEPROM_WEAR_LEVEL_DATA_TYPE
#define ARDUINO_EEPROM_WEAR_LEVEL_DATA_TYPE                 WearLevelData_t
#endif

// interface for data streaming. adds 1486-1620 bytes code (atmega328p)
// if disabled, it does not add any extra code compared to using pointers
#ifndef ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE
#define ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE             0
#endif

#if (ESP8266 || ESP32) && !defined(ARDUINO_EEPROM_IGNORE_ESP_DETECTION)
// the EEPROM object from ESP32/ESP8266 is not supported
// if EEPROM.begin() and EEPROM.commit()/end() is called manually, it can be used but since changing a single byte
// requires to rewrite the entire 4096 byte flash sector, wear leveling is not working
// if an external eeprom is used, set ARDUINO_EEPROM_IGNORE_ESP_DETECTION=1 to skip this check
#error MCU not supported
#endif

#ifndef ARDUINO_EEPROM_DEBUG
#define ARDUINO_EEPROM_DEBUG                                0
#endif

// extensive checks of EEPROM boundaries
#ifndef ARDUINO_EEPROM_ASSERT
#define ARDUINO_EEPROM_ASSERT                               0
#endif
#if ARDUINO_EEPROM_ASSERT && !_MSC_VER
#error Microsoft Visual Studio required
#endif

#define ARDUINO_EEPROM_ALIGN_ADDR(address)                  (((address + pageSize - 1) / pageSize) * pageSize)
#define ARDUINO_EEPROM_ALIGN_LEN(len)                       ARDUINO_EEPROM_ALIGN_ADDR(len)

// enable debug dump functions
#ifndef ARDUINO_EEPROM_HAVE_DUMP
#define ARDUINO_EEPROM_HAVE_DUMP                            0
#endif

#ifndef _BV
#define _BV(bit)                                            (1<<bit)
#endif

#ifndef max
#define max(a,b)                                            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)                                            (((a) < (b)) ? (a) : (b))
#endif

#if _MSC_VER
#pragma pack(push, 1)
#endif

#if _MSC_VER && DEBUG
#define _getStaticDataOffset(index)                         (staticDataOffset + (index * ARDUINO_EEPROM_ALIGN_LEN(staticDataBlockSize)))
#endif

class ArduinoEEPROMBase {
public:
    using EEPROMClass = ARDUINO_EEPROM_CLASS;
    using EEPROMSizeType = eeprom_size_t;
    using CRCType = uint16_t;

    enum class DataTypeEnum : uint8_t {
        STATIC_DATA = 0x01,
        WEAR_LEVEL_DATA = 0x02,
        ALL = STATIC_DATA|WEAR_LEVEL_DATA,
    };

    typedef struct __attribute__((packed)) {
        CRCType crc;
        uint32_t cycleId;
    } DataBlockHeader_t;

    typedef struct __attribute__((packed)) {
        CRCType crc;
        uint16_t staticDataSize;
        uint16_t wearLevelSize;
    } Header_t;

    typedef struct {
        struct {
            uint8_t valid;
            uint8_t validBits;
            uint8_t copies;
            uint32_t writeCycles; // equals max. cycleId
            EEPROMSizeType size;
        } staticData;
        struct {
            bool valid;
            uint32_t writeCycles;
            uint32_t cycleId;
            EEPROMSizeType size;
        } wearLevelData;
    } BasicInfo_t;

    template <bool _Test, class _Ty1, class _Ty2>
    struct conditional {
        using type = _Ty1;
    };
    template <class _Ty1, class _Ty2>
    struct conditional<false, _Ty1, _Ty2> {
        using type = _Ty2;
    };

    using DataBlockSizeType = typename conditional<((ARDUINO_EEPROM_STATIC_DATA_SIZE > 255) || ARDUINO_EEPROM_WEAR_LEVEL_DATA_SIZE > 255), uint16_t, uint8_t>::type;

    static constexpr EEPROMSizeType pageSize = ARDUINO_EEPROM_PAGE_SIZE;

    static constexpr EEPROMSizeType startOffset = ARDUINO_EEPROM_ALIGN_ADDR(ARDUINO_EEPROM_START_OFFSET);
    static constexpr EEPROMSizeType eepromLength = (ARDUINO_EEPROM_LENGTH / ARDUINO_EEPROM_PAGE_SIZE) * ARDUINO_EEPROM_PAGE_SIZE;
    static constexpr DataBlockSizeType staticDataTypeSize = ARDUINO_EEPROM_STATIC_DATA_SIZE;
    static constexpr DataBlockSizeType wearLevelDataTypeSize = ARDUINO_EEPROM_WEAR_LEVEL_DATA_SIZE;
    static constexpr DataBlockSizeType dataBlockHeaderSize = sizeof(DataBlockHeader_t);

    static constexpr uint8_t staticDataCopies = ARDUINO_EEPROM_STATIC_DATA_NUM_COPIES;
    static constexpr uint8_t wearLevelDataCopies = ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES;

#if ARDUINO_EEPROM_AUTO_RESIZE
    static constexpr EEPROMSizeType headerOffset = ARDUINO_EEPROM_ALIGN_ADDR(startOffset);
    static constexpr uint8_t headerNumBlocks = 2;
    static constexpr EEPROMSizeType headerLength = ARDUINO_EEPROM_ALIGN_ADDR(sizeof(Header_t) * headerNumBlocks);
#else
    static constexpr EEPROMSizeType headerOffset = ARDUINO_EEPROM_ALIGN_ADDR(startOffset);
    static constexpr uint8_t headerNumBlocks = 0;
    static constexpr EEPROMSizeType headerLength = 0;
#endif

    static constexpr EEPROMSizeType staticDataOffset = ARDUINO_EEPROM_ALIGN_ADDR(headerOffset + headerLength);
    static constexpr EEPROMSizeType staticDataBlockSize = staticDataTypeSize + dataBlockHeaderSize;
    static constexpr EEPROMSizeType staticDataLength = ARDUINO_EEPROM_ALIGN_LEN(staticDataBlockSize) * staticDataCopies;

    static constexpr EEPROMSizeType wearLevelDataOffset = ARDUINO_EEPROM_ALIGN_ADDR(staticDataOffset + staticDataLength);
    static constexpr EEPROMSizeType wearLevelDataMaxLength = eepromLength - (wearLevelDataOffset - startOffset);
    static constexpr EEPROMSizeType wearLevelBlockSize = (wearLevelDataTypeSize + dataBlockHeaderSize);

    static constexpr int32_t _wearLevelNumCycles = wearLevelDataMaxLength / (ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize) * wearLevelDataCopies);
    using WearLevelCyclesType = typename conditional<(_wearLevelNumCycles > 255), uint16_t, uint8_t>::type;
    static constexpr WearLevelCyclesType wearLevelNumCycles = (WearLevelCyclesType)_wearLevelNumCycles;
    // NOTE: wearLevelNumBlocks is NOT wearLevelNumCycles * wearLevelDataCopies
    static constexpr EEPROMSizeType wearLevelNumBlocks = wearLevelDataMaxLength / ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);
    static constexpr EEPROMSizeType wearLevelDataLength = wearLevelNumBlocks * ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);
    static constexpr EEPROMSizeType eepromUnusedBytes = eepromLength - ((wearLevelDataOffset - startOffset) + wearLevelDataLength);
    static constexpr EEPROMSizeType wearLevelDataLastStartOffset = wearLevelDataOffset + wearLevelDataLength - ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);

    static constexpr EEPROMSizeType INVALID_OFFSET = ~0;

    static_assert(_wearLevelNumCycles > ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES, "Data does not fit into EEPROM");

    static_assert(startOffset + eepromLength <= ARDUINO_EEPROM_MAX_LENGTH, "EEPROM size exceeded");

private:
#if ARDUINO_EEPROM_PASS_BY_REF
    EEPROMClass &_eeprom;

public:
#if ARDUINO_EEPROM_OBJECT != -1
    ArduinoEEPROMBase() : ArduinoEEPROMBase(ARDUINO_EEPROM_OBJECT) {
    }
#endif
    ArduinoEEPROMBase(EEPROMClass &eeprom) : _eeprom(eeprom)
#else
    static constexpr EEPROMClass &_eeprom = ARDUINO_EEPROM_OBJECT;

public:
    ArduinoEEPROMBase()
#endif
    {
        _debugCycleCount = 0;
    }

    void begin();

    // clear EEPROM and set all data to zero
    // CRC checks will success
    // any write operation to an uninitialized area will fail
    // any read operation will fail until data has been written once
    void eraseAndInitialize(DataTypeEnum type) const;

    // return basic information
    void getBasicInfo(BasicInfo_t &info) const;

    // returns 0 if the data from positions set in copiesBitset has not been modified, otherwise it returns a
    // bitset with the positions that have been modified. data is compared byte by byte to avoid checksum
    // collisions
    uint8_t isStaticDataModified(ConstByteAccessPointer data, uint8_t copiesBitset = ~0) const;

    // read from positions set in copiesBitset and return the position as bitset
    uint8_t readStaticData(ByteAccessPointer data, uint8_t copiesBitset = ~0) const;

    // write to positions set in copiesBitset and return the positions as bitset that were successful
    // the data area must be intialized before any write attempt succeeds
    uint8_t writeStaticData(ConstByteAccessPointer data, uint8_t copiesBitset = ~0) const;

    // returns true if data has been changed or any error occurred
    // data is compared byte by byte to avoid checksum collisions
    bool isWearLevelDataModified(ConstByteAccessPointer data) const;

    // read data from the wear leveling area. if a read attempt fails, read available redundant data
    // if ARDUINO_EEPROM_WEAR_LEVEL_DATA_NUM_COPIES is 1, increasing maxReadCopies will read the previously
    // data stored data if available
    // returns 0 for failure or the number of the copy or previously stored data if maxReadCopies
    // exceeds wearLevelDataCopies
    uint8_t readWearLevelData(ByteAccessPointer data, uint8_t maxReadCopies = wearLevelDataCopies) const;

    // write data to the wear leveling area and return number of successfully written copies (up to wearLevelDataCopies)
    // the data area must be intialized before any write attempt succeeds
    uint8_t writeWearLevelData(ConstByteAccessPointer data);

#if ARDUINO_EEPROM_HAVE_DUMP
    // debug output
    void dumpOffsets(Print &output) const;
    void dump(Print &output, DataTypeEnum type = DataTypeEnum::ALL) const;
    void dumpBasicInfo(Print &output, const BasicInfo_t &info) const;
#endif

private:
    void _eraseAndInitialize(EEPROMSizeType offset, DataBlockSizeType size, EEPROMSizeType numBlocks) const;

    // returns the maximum cycle id, including 0 for initialized areas without any written data
    // copiesBitset as input sets which copies to check, use ~0 for all
    // copiesBitset as output indicates which copies match the maximum cycle id and can be used to read
    // data or write/refresh invalid data (invert bitset, copiesBitset ^ 0xff)
    // the array cycleIds[] contains the cycle id for each copy, starting from 0. ~0 indicates read errors
    uint32_t _getStaticDataCycleIdAndBitset(uint8_t &copiesBitset, uint32_t *cycleIds = nullptr) const;

    // returns ~0 on failure or the cycleid, 0 if there is not data stored
    uint32_t _getStaticDataCycleId() const;

#if !_MSC_VER || !DEBUG
    // get offset for the data
    // index specifies the copy, 0 to staticDataCopies - 1
    EEPROMSizeType _getStaticDataOffset(uint8_t index) const;
#endif

    // get offset for data in the wear leveling area
    // returns the offset for cycleId>=startCycleId and cycleId < maxCycleId that
    // has the highest cycle id. startCycleId is set to this value
    // on failure it returns INVALID_OFFSET
    EEPROMSizeType _getWearLevelOffset(uint32_t &startCycleId, uint32_t maxCycleId = ~0UL) const;

    // create CRC of header
    uint16_t _dataBlockHeaderCrc(const DataBlockHeader_t &header) const;

    // read data from offset
    // return false if the cycle id is 0 or the data is invalid
    // header contains CRC and cycleId afterwards
    bool _readDataBlock(EEPROMSizeType offset, ByteAccessPointer data, DataBlockSizeType size, DataBlockHeader_t &header) const;

    // write data to offset and re-read it to validate data integrity
    // on failure it repeats this process ARDUINO_EEPROM_WRITE_ERROR_RETRIES times before it returns false
    bool _writeDataBlock(EEPROMSizeType offset, uint32_t cycleId, ConstByteAccessPointer data, DataBlockSizeType size) const;

    // read data from eeprom
    EEPROMSizeType _eepromRead(EEPROMSizeType offset, ByteAccessPointer data, DataBlockSizeType size) const;

    // fill eeprom with zeros
    EEPROMSizeType _eepromClear(EEPROMSizeType offset, DataBlockSizeType size) const;

    // write data to eeprom. if data = nullptr, it writes zeros
    // the update() method of the EEPROM class is used
    EEPROMSizeType _eepromWrite(EEPROMSizeType offset, ConstByteAccessPointer data, DataBlockSizeType size) const;

#if ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE
    // crc_update using ByteAccessInterface
    uint16_t crc16_update(uint16_t crc, ConstByteAccessPointer data, size_t len) const;
#endif

    // returns if a data block is valid
    // header contains CRC and cycleId afterwards
    bool _validateEepromDataBlockCrc(EEPROMSizeType offset, DataBlockSizeType size, DataBlockHeader_t &header) const;

    // returns 0 if the data is identical
    // compares the header crc and data byte by byte
    uint8_t _compareDataBlock(EEPROMSizeType offset, ConstByteAccessPointer data, DataBlockSizeType size) const;

#if ARDUINO_EEPROM_ASSERT
public:
    enum class ASSERT_DATA_TYPE {
        NONE,
        STATIC_DATA,
        WEAR_LEVEL_DATA,
    };
    static ASSERT_DATA_TYPE _assertDataType;
#endif
    uint32_t _debugCycleCount;
};

// convenient way for using data classes/structures
// compiler optimizations should remove any extra code

template<class StaticDataType, class WearLevelDataType>
class ArduinoEEPROMTpl : public ArduinoEEPROMBase
{
public:
    using ArduinoEEPROMBase::ArduinoEEPROMBase;
    using ArduinoEEPROMBase::isStaticDataModified;
    using ArduinoEEPROMBase::readStaticData;
    using ArduinoEEPROMBase::writeStaticData;
    using ArduinoEEPROMBase::isWearLevelDataModified;
    using ArduinoEEPROMBase::readWearLevelData;
    using ArduinoEEPROMBase::writeWearLevelData;

    static_assert(sizeof(StaticDataType) >= staticDataTypeSize, "sizeof(StaticDataType) < staticDataTypeSize");
    static_assert(sizeof(WearLevelDataType) >= wearLevelDataTypeSize, "sizeof(WearLevelDataType) < wearLevelDataTypeSize");

    inline uint8_t isStaticDataModified(const StaticDataType &data)
    {
        return ArduinoEEPROMBase::isStaticDataModified(ConstByteAccessArray(&data));
    }

    inline uint8_t readStaticData(StaticDataType &data, uint8_t copiesBitset = ~0)
    {
        return ArduinoEEPROMBase::readStaticData(ByteAccessArray(&data), copiesBitset);
    }

    inline uint8_t writeStaticData(const StaticDataType &data, uint8_t copiesBitset = ~0)
    {
        return ArduinoEEPROMBase::writeStaticData(ConstByteAccessArray(&data), copiesBitset);
    }

    inline bool isWearLevelDataModified(const WearLevelDataType &data)
    {
        return ArduinoEEPROMBase::isWearLevelDataModified(ConstByteAccessArray(&data));
    }

    inline uint8_t readWearLevelData(WearLevelDataType &data, uint8_t maxReadCopies = wearLevelDataCopies)
    {
        return ArduinoEEPROMBase::readWearLevelData(ByteAccessArray(&data), maxReadCopies);
    }

    inline uint8_t writeWearLevelData(const WearLevelDataType &data)
    {
        return ArduinoEEPROMBase::writeWearLevelData(ConstByteAccessArray(&data));
    }
};

#if _MSC_VER
#pragma pack(pop)
#endif
