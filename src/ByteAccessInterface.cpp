/**
 * Author: sascha_lammers@gmx.de
 */

#include "ByteAccessInterface.h"

#if ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE

ByteAccessReference &ByteAccessReference::operator=(uint8_t data)
{
    _data.write(_ptr, data);
    return *this;
}

ByteAccessReference &ByteAccessReference::operator=(const ByteAccessReference &ref)
{
    return *this = *ref;
}

uint8_t ByteAccessReference::operator*() const
{
    return _data.read(_ptr);
}

ByteAccessReference::operator uint8_t() const
{
    return _data.read(_ptr);
}

uint8_t ByteAccessPointer::operator*() const
{
    return _data.read(_ptr);
}

ByteAccessInterface::operator ByteAccessPointer()
{
    return ByteAccessPointer(*this, _data);
}

#endif
