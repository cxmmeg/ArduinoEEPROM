/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino.h>

#if ARDUINO_EEPROM_HAVE_BYTEARRAY_INTERFACE

// streaming support for reading and writing

class ByteAccessInterface;
class ByteAccessPointer;
using ConstByteAccessPointer = ByteAccessPointer;

class ByteAccessReference {
public:
    ByteAccessReference(ByteAccessInterface &data, uint8_t *ptr) : _data(data), _ptr(ptr) {}

    ByteAccessReference &operator=(const ByteAccessReference &ref);
    ByteAccessReference &operator=(uint8_t data);
    uint8_t operator*() const;
    operator uint8_t() const;

private:
    ByteAccessInterface &_data;
    uint8_t *_ptr;
};

class ByteAccessPointer
{
public:
    ByteAccessPointer(ByteAccessInterface &data, uint8_t *ptr) : _data(data), _ptr(ptr) {}

    ByteAccessReference operator[](const uint16_t index) {
         return ByteAccessReference(_data, _ptr + index);
    }

    uint8_t operator*() const;

    ByteAccessReference operator*() {
        return ByteAccessReference(_data, _ptr);
    }

    ByteAccessPointer operator++(int) {
        auto tmp = *this;
        ++_ptr;
        return tmp;
    }
    ByteAccessPointer operator--(int) {
        auto tmp = *this;
        --_ptr;
        return tmp;
    }

    ByteAccessPointer &operator++() {
        ++_ptr;
        return *this;
    }
    ByteAccessPointer &operator--() {
        --_ptr;
        return *this;
    }

protected:
    ByteAccessInterface &_data;
    uint8_t *_ptr;
};

// if only a single class is required, implementing read/write into ByteAccessInterface saves 134 byte (atmega328p)
// virtual/abstract classes cannot be optimized very well

class ByteAccessInterface
{
public:
    ByteAccessInterface(const void *data) : _data(reinterpret_cast<uint8_t *>(const_cast<void *>(data))) {}

    operator ByteAccessPointer();

    virtual uint8_t read(const uint8_t *ptr) = 0;                 // index = (_ptr - _data)
    virtual void write(uint8_t *ptr, uint8_t data) = 0;           // index = (_ptr - _data)

protected:
    uint8_t *_data;
};

class ByteAccessArray : public ByteAccessInterface {
public:
    using ByteAccessInterface::ByteAccessInterface;

    uint8_t read(const uint8_t *ptr) {
        return *ptr;
    }

    void write(uint8_t *ptr, uint8_t data) {
        *ptr = data;
    }
};

using ConstByteAccessArray = ByteAccessArray;

#else

using ByteAccessPointer = uint8_t *;
using ConstByteAccessPointer = const uint8_t *;

#if _MSC_VER && DEBUG

#define ByteAccessArray(ptr)        (reinterpret_cast<uint8_t *>(ptr))
#define ConstByteAccessArray(ptr)   ByteAccessArray(const_cast<void *>(reinterpret_cast<const void *>(ptr)))

#else

class ByteAccessArray
{
public:
    ByteAccessArray(void *ptr) : _ptr(reinterpret_cast<ByteAccessPointer>(ptr)) {}

    operator ByteAccessPointer() {
        return _ptr;
    }

private:
    ByteAccessPointer _ptr;
};

class ConstByteAccessArray
{
public:
    ConstByteAccessArray(const void *ptr) : _ptr(reinterpret_cast<ConstByteAccessPointer>(ptr)) {}

    operator ConstByteAccessPointer() {
        return _ptr;
    }

private:
    ConstByteAccessPointer _ptr;
};

#endif

#endif
