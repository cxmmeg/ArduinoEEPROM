/**
 * Author: sascha_lammers@gmx.de
 */

#include "ArduinoEEPROM.h"

#if ARDUINO_EEPROM_DEBUG
#include <debug_helper.h>
static bool __init = DEBUG_HELPER_INIT();
#define _debug_printf(...)                          debug_printf(__VA_ARGS__)
#define _debug_printf_P(...)                        debug_printf_P(__VA_ARGS__)
#define _debug_print_result(result)					debug_print_result(result)
#else
#undef _debug_printf
#undef _debug_printf_P
#undef _debug_print_result
#define _debug_printf(...)							;
#define _debug_printf_P(...)						;
#define _debug_print_result(result)                 result
#endif

#if ARDUINO_EEPROM_ASSERT
#include <assert.h>

ArduinoEEPROMBase::ASSERT_DATA_TYPE ArduinoEEPROMBase::_assertDataType = ArduinoEEPROMBase::ASSERT_DATA_TYPE::NONE;

class AssertClass {
public:

    bool in(ArduinoEEPROMBase::ASSERT_DATA_TYPE type, int offset, int size, int start, int length)
    {
        bool result = (offset >= start && offset + size <= start + length);
        if (!result) {
            _snwprintf_s(_buffer, sizeof(_buffer), 128, L"type %u, %d >= %d && %d <= %d\n", (int)type, offset, start, offset + size, start + length);
        }
        return result;
    }

    bool inType(ArduinoEEPROMBase::ASSERT_DATA_TYPE type, int offset, int size, int start, int length)
    {
        if (ArduinoEEPROMBase::_assertDataType == type) {
            return in(type, offset, size, start, length);
        }
        else {
            return false;
        }
    }

    ArduinoEEPROMBase::ASSERT_DATA_TYPE _type;
    wchar_t _buffer[128];
};

AssertClass assertClass;


#define __ASSERT(...)                           _ASSERT_EXPR(__VA_ARGS__, assertClass._buffer)
#define __ASSERT_ST_DATA(ofs, size)             (assertClass.inType(ArduinoEEPROMBase::ASSERT_DATA_TYPE::STATIC_DATA, ofs, size, staticDataOffset, staticDataLength))
#define __ASSERT_WL_DATA(ofs, size)             (assertClass.inType(ArduinoEEPROMBase::ASSERT_DATA_TYPE::WEAR_LEVEL_DATA, ofs, size, wearLevelDataOffset, wearLevelDataLength))

// detect eeprom area by size of type...
#define __ASSERT_DATA(ofs, size)                __ASSERT(__ASSERT_ST_DATA(ofs, size) || __ASSERT_WL_DATA(ofs, size))

#define __ASSERT_SET_DATA_TYPE(type)            ArduinoEEPROMBase::_assertDataType = ArduinoEEPROMBase::ASSERT_DATA_TYPE::type

#else

#define __ASSERT(...)                           ;
#define __ASSERT_DATA(...)                      ;
#define __ASSERT_SET_DATA_TYPE(...)             ;

#endif

#if ARDUINO_EEPROM_HAVE_DUMP
#if _MSC_VER
#define Serial_printf_P Serial.printf_P
#else
#include "helpers.h"
#endif
#endif

void ArduinoEEPROMBase::begin()
{
#if ARDUINO_EEPROM_AUTO_RESIZE
#endif
}

void ArduinoEEPROMBase::eraseAndInitialize(DataTypeEnum type) const
{
    if ((uint8_t)type & (uint8_t)DataTypeEnum::STATIC_DATA) {
        __ASSERT_SET_DATA_TYPE(STATIC_DATA);
        _eraseAndInitialize(staticDataOffset, staticDataTypeSize, staticDataCopies);
    }
    if ((uint8_t)type & (uint8_t)DataTypeEnum::WEAR_LEVEL_DATA) {
        __ASSERT_SET_DATA_TYPE(WEAR_LEVEL_DATA);
        _eraseAndInitialize(wearLevelDataOffset, wearLevelDataTypeSize, wearLevelNumBlocks);
    }
}

void ArduinoEEPROMBase::getBasicInfo(BasicInfo_t &info) const
{
    uint8_t copiesBitset = ~0;
    info.staticData.writeCycles = _getStaticDataCycleIdAndBitset(copiesBitset, nullptr);
    info.staticData.validBits = copiesBitset;
    info.staticData.valid = 0;
    for(uint8_t i = 0; i < staticDataCopies; i++) {
        if (copiesBitset & 0x01) {
            info.staticData.valid++;
        }
        copiesBitset >>= 1;
    }
    info.staticData.copies = staticDataCopies;
    info.staticData.size = staticDataTypeSize;

    uint32_t cycleId = 0;
    auto offset = _getWearLevelOffset(cycleId);

    info.wearLevelData.valid = (offset != INVALID_OFFSET);
    info.wearLevelData.cycleId = cycleId;
    info.wearLevelData.writeCycles = cycleId / wearLevelNumBlocks;
    info.wearLevelData.size = wearLevelDataTypeSize;
}

uint8_t ArduinoEEPROMBase::isStaticDataModified(ConstByteAccessPointer data, uint8_t copiesBitset) const
{
    __ASSERT_SET_DATA_TYPE(STATIC_DATA);
    uint8_t result = 0;
    for (uint8_t i = 0; i < staticDataCopies; i++) {
        if (copiesBitset & _BV(i)) {
            if (_compareDataBlock(_getStaticDataOffset(i), data, staticDataTypeSize)) {
                result |= _BV(i);
            }
        }
    }
    _debug_printf_P(PSTR("result=%02x\n"), result);
    return result;
}

uint8_t ArduinoEEPROMBase::readStaticData(ByteAccessPointer data, uint8_t copiesBitset) const
{
    __ASSERT_SET_DATA_TYPE(STATIC_DATA);
    DataBlockHeader_t header;
    for (uint8_t i = 0; i < staticDataCopies; i++) {
        if (copiesBitset & _BV(i)) {
            if (_readDataBlock(_getStaticDataOffset(i), data, staticDataTypeSize, header)) {
                _debug_printf_P(PSTR("result=\n"), _BV(i));
                return _BV(i);
            }
        }
    }
    _debug_printf_P(PSTR("result=0\n"));
    return 0;
}

uint8_t ArduinoEEPROMBase::writeStaticData(ConstByteAccessPointer data, uint8_t copiesBitset) const
{
__ASSERT_SET_DATA_TYPE(STATIC_DATA);
uint8_t result = 0;
auto cycleId = _getStaticDataCycleId();
if (cycleId++ == (uint32_t)~0) {
    _debug_printf_P(PSTR("cycleId=~0\n"));
    return false;
}
for (uint8_t i = 0; i < staticDataCopies; i++) {
    if (copiesBitset & _BV(i)) {
        if (_writeDataBlock(_getStaticDataOffset(i), cycleId, data, staticDataTypeSize)) {
            result |= _BV(i);
        }
    }
}
_debug_printf_P(PSTR("result=%02x\n"), result);
return result;
}

bool ArduinoEEPROMBase::isWearLevelDataModified(ConstByteAccessPointer data) const
{
    __ASSERT_SET_DATA_TYPE(WEAR_LEVEL_DATA);
    uint32_t cycleId = 0;
    auto offset = _getWearLevelOffset(cycleId);
    if (offset == INVALID_OFFSET) {
        _debug_printf_P(PSTR("result=1\n"));
        return true;
    }
    return _debug_print_result(_compareDataBlock(offset, data, wearLevelDataTypeSize) != 0);
}

uint8_t ArduinoEEPROMBase::_compareDataBlock(EEPROMSizeType offset, ConstByteAccessPointer data, DataBlockSizeType size) const
{
    DataBlockHeader_t header;
    __ASSERT_DATA(offset, sizeof(header));
    offset = _eepromRead(offset, ByteAccessArray(&header), sizeof(header));
    if (header.cycleId == 0) {
        return 1;
    }

    CRCType crc = crc16_update(_dataBlockHeaderCrc(header), data, size);
    if (header.crc != crc) {
        return 2;
    }

    __ASSERT_DATA(offset, size);
    for (EEPROMSizeType i = 0; i < size; i++) {
        if (_eeprom.read(offset++) != *data++) {
            return 3;
        }
    }
    return 0;
}

uint8_t ArduinoEEPROMBase::readWearLevelData(ByteAccessPointer data, uint8_t maxReadCopies) const
{
    __ASSERT_SET_DATA_TYPE(WEAR_LEVEL_DATA);
    uint32_t maxCycleId = ~0UL;
    uint32_t cycleId = 0;
    for (uint8_t i = 0; i < maxReadCopies; i++) {
        auto offset = _getWearLevelOffset(cycleId, maxCycleId);
        if (offset == INVALID_OFFSET) {
            _debug_printf_P(PSTR("invalid offset, cycleId=%lu\n"), (unsigned long)cycleId);
            break;
        }

        DataBlockHeader_t header;
        if (_readDataBlock(offset, data, wearLevelDataTypeSize, header)) {
            _debug_printf_P(PSTR("result=%u\n"), i + 1);
            return i + 1;
        }

        // if a read error occured, read the previous block
        if (header.cycleId == 1) {
            break; // first block reached
        }
        maxCycleId = header.cycleId - 1;
        _debug_printf_P(PSTR("maxCycleId=%lu\n"), (unsigned long)maxCycleId);
    }
    _debug_printf_P(PSTR("result=0\n"));
    return 0;
}

uint8_t ArduinoEEPROMBase::writeWearLevelData(ConstByteAccessPointer data)
{
    uint8_t result = 0;
    uint32_t maxCycleId = ~0UL;
    uint32_t cycleId = 0;
    auto offset = _getWearLevelOffset(cycleId, maxCycleId);
    if (offset == INVALID_OFFSET) {
        _debug_printf_P(PSTR("invalid offset, cycleId=%lu\n"), (unsigned long)cycleId);
        return 0;
    }

    for (uint8_t i = 0; i < wearLevelDataCopies; i++) {
        offset += ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);
        if (offset > wearLevelDataLastStartOffset) {
            offset = wearLevelDataOffset;
            _debugCycleCount++;
        }
        cycleId++;
        _debug_printf_P(PSTR("offset=%u, cycleId=%lu\n"), offset, (unsigned long)cycleId);
        if (_writeDataBlock(offset, cycleId, data, wearLevelDataTypeSize)) {
            result++;
        }
    }

    _debug_printf_P(PSTR("result=%u\n"), result);
    return result;
}

#if ARDUINO_EEPROM_HAVE_DUMP

void ArduinoEEPROMBase::dumpOffsets(Print &output) const
{
    Serial_printf_P(
        PSTR(
            "page size:        %u\n"
            "start:            %u:%u\n"
            "static:           %u:%u (copies %u, size %u/%u/%u)\n"
            "end:              %u\n"
            "wear level:       %u:%u (blocks %u, cycles %u, copies %u, size %u/%u/%u)\n"
            "end:              %u\n"
            "unused:           %u:%u\n"
            "eeprom end:       %u\n"
        ),
        pageSize,
        startOffset, eepromLength,
        staticDataOffset, staticDataLength,
        staticDataCopies, staticDataTypeSize, staticDataBlockSize, ARDUINO_EEPROM_ALIGN_LEN(staticDataBlockSize),
        staticDataOffset + staticDataLength - 1,
        wearLevelDataOffset, wearLevelDataLength,
        wearLevelNumBlocks, wearLevelNumCycles, wearLevelDataCopies, wearLevelDataTypeSize, wearLevelBlockSize, ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize),
        wearLevelDataOffset + wearLevelDataLength - 1,
        wearLevelDataOffset + wearLevelDataLength,
        eepromUnusedBytes,
        startOffset + eepromLength - 1
    );
}

void ArduinoEEPROMBase::dump(Print &output, DataTypeEnum type) const
{
    uint8_t copies = ~0;
    uint8_t valid = 0;
    uint32_t cycleId = _getStaticDataCycleIdAndBitset(copies);
    char buf[staticDataCopies + 1];
    uint8_t i = 0;
    for (; i < staticDataCopies; i++) {
        if (copies & _BV(i)) {
            buf[i] = '1';
            valid++;
        }
        else {
            buf[i] = 'x';
        }
    }
    buf[i] = 0;

    dumpOffsets(output);

    Serial_printf_P(PSTR("Static data: %u / %u (%s)\n"), valid, staticDataCopies, buf);

    cycleId = 0;
    auto offset = _getWearLevelOffset(cycleId);
    Serial_printf_P(PSTR("Wear level: offset = %u, cycle id = %lu\n"),
        offset,
        (unsigned long)cycleId
    );
    Serial_printf_P(PSTR("_debugCycleCount=%lu\n"), (unsigned long)_debugCycleCount);
    //Serial_printf_P(PSTR("size=%u,data=%u,overhead=%u,redundant=%u,unused=%u\n"),
    //    eepromLength,
    //    staticDataTypeSize + (wearLevelDataTypeSize * wearLevelNumCycles),
    //    (sizeof(DataBlockHeader_t) * (staticDataCopies + wearLevelNumBlocks)) + (staticDataOffset - startOffset),
    //    (staticDataTypeSize * (staticDataCopies - 1)) + (wearLevelDataTypeSize * (wearLevelDataCopies - 1) * (wearLevelNumBlocks / wearLevelDataCopies)),
    //    eepromUnusedBytes
    //);

    if ((uint8_t)type & (uint8_t)DataTypeEnum::STATIC_DATA) {
        __ASSERT_SET_DATA_TYPE(STATIC_DATA);
        Serial.println(F("Ofs   CRC  CycleId  Status"));
        EEPROMSizeType offset = staticDataOffset;
        DataBlockHeader_t header;
        for (uint8_t i = 0; i < staticDataCopies; i++) {
            auto result = _validateEepromDataBlockCrc(offset, staticDataTypeSize, header);
            Serial_printf_P(PSTR("%04x: %04x %08lx %s\n"), offset, header.crc, (unsigned long)header.cycleId, header.cycleId ? (result ? "GOOD" : "BAD") : "EMPTY");
            offset += ARDUINO_EEPROM_ALIGN_LEN(staticDataBlockSize);
        }
    }

    if ((uint8_t)type & (uint8_t)DataTypeEnum::WEAR_LEVEL_DATA) {
        __ASSERT_SET_DATA_TYPE(WEAR_LEVEL_DATA);
        Serial.println(F("Ofs Cycle CRC  CycleId  Status"));

        EEPROMSizeType offset = wearLevelDataOffset;
        DataBlockHeader_t header;
        uint8_t n = 1;
        while (offset <= wearLevelDataLastStartOffset) {
            Serial_printf_P(PSTR("%u/%u "), n++, wearLevelNumBlocks);
            auto result = _validateEepromDataBlockCrc(offset, wearLevelDataTypeSize, header);
            auto cycle = (unsigned)(((header.cycleId - 1) / wearLevelDataCopies) % (wearLevelNumCycles));
            Serial_printf_P(PSTR("ofs=%04x "), offset);
            if (!result) {
                Serial_printf_P(PSTR("cycle=%u cycleId=%lu invalid data, error\n"), cycle, (unsigned long)(header.cycleId));
            }
            else {
                Serial_printf_P(PSTR("cycle=%u cycleId=%lu %s\n"), cycle, (unsigned long)header.cycleId, header.cycleId ? (result ? "GOOD" : "BAD") : "EMPTY");
            }
            offset += ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);
        }
    }
}

void ArduinoEEPROMBase::dumpBasicInfo(Print &output, const BasicInfo_t &info) const
{
    Serial_printf_P(PSTR("valid=%u/%u, write cyles=%lu, size=%u\n"), info.staticData.valid, info.staticData.copies, (unsigned long)info.staticData.writeCycles, info.staticData.size);
    Serial_printf_P(PSTR("valid=%u, write cycles=%lu, cycle id=%lu, size=%u\n"), info.wearLevelData.valid, (unsigned long)info.wearLevelData.writeCycles, (unsigned long)info.wearLevelData.cycleId, info.wearLevelData.size);
}

#endif

void ArduinoEEPROMBase::_eraseAndInitialize(EEPROMSizeType offset, DataBlockSizeType size, EEPROMSizeType numBlocks) const
{
    DataBlockHeader_t header;

    header.cycleId = 0;
    header.crc = _dataBlockHeaderCrc(header);;
    for (WearLevelCyclesType i = 0; i < size; i++) {
        header.crc = _crc16_update(header.crc, 0);
    }
#if ARDUINO_EEPROM_PAGE_SIZE > 1
    // extra space till next page
    uint8_t extra = ARDUINO_EEPROM_ALIGN_LEN(size + sizeof(header)) - (size + sizeof(header));
#endif
    for (uint16_t i = 0; i < numBlocks; i++) {
        __ASSERT_DATA(offset, sizeof(header));
        offset = _eepromWrite(offset, ConstByteAccessArray(&header), sizeof(header));
        __ASSERT_DATA(offset, size);
        offset = _eepromClear(offset, size);
#if ARDUINO_EEPROM_PAGE_SIZE > 1
        offset += extra;
#endif
    }
}

uint32_t ArduinoEEPROMBase::_getStaticDataCycleIdAndBitset(uint8_t &copiesBitset, uint32_t *cycleIds) const
{
    uint32_t maxCycleId = 0;
    uint32_t cycleIdsTmp[staticDataCopies];
    DataBlockHeader_t header;

    __ASSERT_SET_DATA_TYPE(STATIC_DATA);

    if (!cycleIds) {
        cycleIds = cycleIdsTmp;
    }

    memset(cycleIds, 0xff, sizeof(cycleIdsTmp));

    for (uint8_t i = 0; i < staticDataCopies; i++) {
        if (copiesBitset & _BV(i)) {
            if (_validateEepromDataBlockCrc(_getStaticDataOffset(i), staticDataTypeSize, header)) {
                cycleIds[i] = header.cycleId;
                maxCycleId = max(maxCycleId, header.cycleId);
            }
        }
    }

    copiesBitset = 0;
    for (uint8_t i = 0; i < staticDataCopies; i++) {
        if (maxCycleId == cycleIds[i]) {
            copiesBitset |= _BV(i);
        }
    }
    return maxCycleId;
}

uint32_t ArduinoEEPROMBase::_getStaticDataCycleId() const
{
    uint8_t copiesBitset = ~0;
    auto cycleId = _getStaticDataCycleIdAndBitset(copiesBitset, nullptr);
    if (copiesBitset) {
        return cycleId;
    }
    return ~0;
}

#if !_MSC_VER || !DEBUG
ArduinoEEPROMBase::EEPROMSizeType ArduinoEEPROMBase::_getStaticDataOffset(uint8_t index) const
{
    return staticDataOffset + (index * ARDUINO_EEPROM_ALIGN_LEN(staticDataBlockSize));
}
#endif

ArduinoEEPROMBase::EEPROMSizeType ArduinoEEPROMBase::_getWearLevelOffset(uint32_t &cycleId, uint32_t maxCycleId) const
{
    auto lastOffset = INVALID_OFFSET;
    DataBlockHeader_t header;
    __ASSERT_SET_DATA_TYPE(WEAR_LEVEL_DATA);
    __ASSERT(cycleId == 0 || maxCycleId != ~0);

    EEPROMSizeType offset = wearLevelDataOffset;

    while (offset <= wearLevelDataLastStartOffset) {
        __ASSERT_DATA(offset, sizeof(header));
        _eepromRead(offset, ByteAccessArray(&header), sizeof(header));
        if (header.cycleId >= cycleId && header.cycleId < maxCycleId) {
            if (_validateEepromDataBlockCrc(offset, wearLevelDataTypeSize, header)) {
                lastOffset = offset;
                cycleId = max(cycleId, header.cycleId);
            }
            else {
                _debug_printf_P(PSTR("%04x: error\n"), offset);
            }
        }
        offset += ARDUINO_EEPROM_ALIGN_LEN(wearLevelBlockSize);
    }
    return lastOffset;
}

uint16_t ArduinoEEPROMBase::_dataBlockHeaderCrc(const DataBlockHeader_t &header) const
{
    return ::crc16_update(&header.cycleId, sizeof(header) - sizeof(header.crc));
}

bool ArduinoEEPROMBase::_readDataBlock(EEPROMSizeType offset, ByteAccessPointer data, DataBlockSizeType size, DataBlockHeader_t &header) const
{
#if ARDUINO_EEPROM_DEBUG
    auto tmp = offset;
#endif

    __ASSERT_DATA(offset, sizeof(header));
    offset = _eepromRead(offset, ByteAccessArray(&header), sizeof(header));
    if (header.cycleId == 0) {
        return false;
    }
    __ASSERT_DATA(offset, size);
    offset = _eepromRead(offset, data, size);

    _debug_printf_P(PSTR("_readDataBlock ofs=%u, crc=%04x, id=%u\n"), tmp, header.crc, header.cycleId);

    return header.crc == crc16_update(_dataBlockHeaderCrc(header), data, size);
}

bool ArduinoEEPROMBase::_writeDataBlock(EEPROMSizeType offset, uint32_t cycleId, ConstByteAccessPointer data, DataBlockSizeType size) const
{
    DataBlockHeader_t header;

    header.cycleId = cycleId;
    header.crc = crc16_update(_dataBlockHeaderCrc(header), data, size);

    _debug_printf_P(PSTR("_writeDataBlock ofs=%u, crc=%04x, id=%u\n"), offset, header.crc, header.cycleId);

#if ARDUINO_EEPROM_WRITE_ERROR_RETRIES
    for (uint8_t i = 0; i < ARDUINO_EEPROM_WRITE_ERROR_RETRIES; i++)
#endif
    {
        __ASSERT_DATA(offset, sizeof(header));
        auto ofsTmp = _eepromWrite(offset, ByteAccessArray(&header), sizeof(header));
        __ASSERT_DATA(ofsTmp, size);
        _eepromWrite(ofsTmp, data, size);

        DataBlockHeader_t hdrTmp;
        if (_validateEepromDataBlockCrc(offset, size, hdrTmp)) {
            return true;
        }
    }

    return false;
}

ArduinoEEPROMBase::EEPROMSizeType ArduinoEEPROMBase::_eepromRead(EEPROMSizeType offset, ByteAccessPointer data, DataBlockSizeType size) const
{
    __ASSERT_DATA(offset, size);
    while (size--) {
        *data++ = _eeprom.read(offset++);
    }
    return offset;
}

ArduinoEEPROMBase::EEPROMSizeType ArduinoEEPROMBase::_eepromClear(EEPROMSizeType offset, DataBlockSizeType size) const
{
    __ASSERT_DATA(offset, size);
    while (size--) {
        _eeprom.update(offset++, 0);
    }
    return offset;
}

ArduinoEEPROMBase::EEPROMSizeType ArduinoEEPROMBase::_eepromWrite(EEPROMSizeType offset, ConstByteAccessPointer data, DataBlockSizeType size) const
{
    __ASSERT_DATA(offset, size);
    while (size--) {
        _eeprom.update(offset++, *data++);
    }
    return offset;
}

#if ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE
uint16_t ArduinoEEPROMBase::crc16_update(uint16_t crc, ConstByteAccessPointer data, size_t len) const
{
    while (len--) {
        crc = ::crc16_update(crc, *data++);
    }
    return crc;
}
#endif

bool ArduinoEEPROMBase::_validateEepromDataBlockCrc(EEPROMSizeType offset, DataBlockSizeType size, DataBlockHeader_t &header) const
{
#if ARDUINO_EEPROM_DEBUG
    auto tmp = offset;
#endif
    __ASSERT_DATA(offset, sizeof(header));
    offset = _eepromRead(offset, ByteAccessArray(&header), sizeof(header));
    CRCType crc = _dataBlockHeaderCrc(header);
    __ASSERT_DATA(offset, size);
    while (size--) {
        crc = ::crc16_update(crc, _eeprom.read(offset++));
    }
    _debug_printf_P(PSTR("_validateEepromDataBlockCrc ofs=%u, crc=%04x, eeprom.crc=%04x, id=%u\n"), tmp, header.crc, crc, header.cycleId);
    return crc == header.crc;
}
