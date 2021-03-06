/* This file is part of VoltDB.
 * Copyright (C) 2008-2010 VoltDB Inc.
 *
 * This file contains original code and/or modifications of original code.
 * Any modifications made by VoltDB Inc. are licensed under the following
 * terms and conditions:
 *
 * VoltDB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VoltDB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Copyright (C) 2008 by H-Store Project
 * Brown University
 * Massachusetts Institute of Technology
 * Yale University
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/* Copyright (C) 2017 by S-Store Project
 * Brown University
 * Massachusetts Institute of Technology
 * Portland State University 
 *
 * Author: S-Store Team (sstore.cs.brown.edu)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef HSTORESERIALIZEIO_H
#define HSTORESERIALIZEIO_H

#include <limits>
#include <vector>
#include <string>
#include <exception>
#include <arpa/inet.h>
#include "bytearray.h"
#include "debuglog.h"
#include "common/SQLException.h"

namespace voltdb {


#ifdef __DARWIN_OSSwapInt64 // for darwin/macosx

#define htonll(x) __DARWIN_OSSwapInt64(x)
#define ntohll(x) __DARWIN_OSSwapInt64(x)

#else // unix in general

#undef htons
#undef ntohs
#define htons(x) static_cast<uint16_t>((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#define ntohs(x) static_cast<uint16_t>((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

#ifdef __bswap_64 // recent linux

#define htonll(x) static_cast<uint64_t>(__bswap_constant_64(x))
#define ntohll(x) static_cast<uint64_t>(__bswap_constant_64(x))

#else // unix in general again

#define htonll(x) (((int64_t)(ntohl((int32_t)((x << 32) >> 32))) << 32) | (uint32_t)ntohl(((int32_t)(x >> 32))))
#define ntohll(x) (((int64_t)(ntohl((int32_t)((x << 32) >> 32))) << 32) | (uint32_t)ntohl(((int32_t)(x >> 32))))

#endif // __bswap_64

#endif // unix or mac



/** Abstract class for reading from memory buffers. */
class SerializeInput {
protected:
    /** Does no initialization. Subclasses must call initialize. */
    SerializeInput() : current_(NULL), end_(NULL) {}

    void initialize(const void* data, size_t length) {
        current_ = reinterpret_cast<const char*>(data);
        end_ = current_ + length;
    }

public:
    virtual ~SerializeInput() {};

    // functions for deserialization
    inline char readChar() {
        return readPrimitive<char>();
    }

    inline int8_t readByte() {
        return readPrimitive<int8_t>();
    }

    inline int16_t readShort() {
        int16_t value = readPrimitive<int16_t>();
        return ntohs(value);
    }

    inline int32_t readInt() {
        int32_t value = readPrimitive<int32_t>();
        return ntohl(value);
    }

    inline bool readBool() {
        return readByte();
    }

    inline char readEnumInSingleByte() {
        return readByte();
    }

    inline int64_t readLong() {
        int64_t value = readPrimitive<int64_t>();
        return ntohll(value);
    }

    inline float readFloat() {
        int32_t value = readPrimitive<int32_t>();
        value = ntohl(value);
        float retval;
        memcpy(&retval, &value, sizeof(retval));
        return retval;
    }

    inline double readDouble() {
        int64_t value = readPrimitive<int64_t>();
        value = ntohll(value);
        double retval;
        memcpy(&retval, &value, sizeof(retval));
        return retval;
    }

    /** Returns a pointer to the internal data buffer, advancing the read position by length. */
    const void* getRawPointer(size_t length) {
        const void* result = current_;
        current_ += length;
        // TODO: Make this a non-optional check?
        assert(current_ <= end_);
        return result;
    }

    /** Copy a string from the buffer. */
    inline std::string readTextString() {
        int32_t stringLength = readInt();
        assert(stringLength >= 0);
        return std::string(reinterpret_cast<const char*>(getRawPointer(stringLength)),
                stringLength);
    };

    /** Copy a ByteArray from the buffer. */
    inline ByteArray readBinaryString() {
        int32_t stringLength = readInt();
        assert(stringLength >= 0);
        return ByteArray(reinterpret_cast<const char*>(getRawPointer(stringLength)),
                stringLength);
    };

    /** Copy the next length bytes from the buffer to destination. */
    inline void readBytes(void* destination, size_t length) {
        ::memcpy(destination, getRawPointer(length), length);
    };

    /** Move the read position back by bytes. Warning: this method is
    currently unverified and could result in reading before the
    beginning of the buffer. */
    // TODO(evanj): Change the implementation to validate this?
    void unread(size_t bytes) {
        current_ -= bytes;
    }

    size_t numBytesNotYetRead() {
    	return (end_ - current_);
    }

private:
    template <typename T>
    T readPrimitive() {
        T value;
        ::memcpy(&value, current_, sizeof(value));
        current_ += sizeof(value);
        return value;
    }

    // Current read position.
    const char* current_;
    // End of the buffer. Valid byte range: current_ <= validPointer < end_.
    const char* end_;

    // No implicit copies
    SerializeInput(const SerializeInput&);
    SerializeInput& operator=(const SerializeInput&);
};

/** Abstract class for writing to memory buffers. Subclasses may optionally support resizing. */
class SerializeOutput {
protected:
    SerializeOutput() : buffer_(NULL), position_(0), capacity_(0) {}

    /** Set the buffer to buffer with capacity. Note this does not change the position. */
    void initialize(void* buffer, size_t capacity) {
        buffer_ = reinterpret_cast<char*>(buffer);
        assert(position_ <= capacity);
        capacity_ = capacity;
    }
    void setPosition(size_t position) {
        this->position_ = position;
    }
public:
    virtual ~SerializeOutput() {};

    /** Returns a pointer to the beginning of the buffer, for reading the serialized data. */
    const char* data() const { return buffer_; }

    /** Returns the number of bytes written in to the buffer. */
    size_t size() const { return position_; }

    // functions for serialization
    inline void writeChar(char value) {
        writePrimitive(value);
    }

    inline void writeByte(int8_t value) {
        writePrimitive(value);
    }

    inline void writeShort(int16_t value) {
        writePrimitive(static_cast<uint16_t>(htons(value)));
    }

    inline void writeInt(int32_t value) {
        writePrimitive(htonl(value));
    }

    inline void writeBool(bool value) {
        writeByte(value ? int8_t(1) : int8_t(0));
    };

    inline void writeLong(int64_t value) {
        writePrimitive(htonll(value));
    }

    inline void writeFloat(float value) {
        int32_t data;
        memcpy(&data, &value, sizeof(data));
        writePrimitive(htonl(data));
    }

    inline void writeDouble(double value) {
        int64_t data;
        memcpy(&data, &value, sizeof(data));
        writePrimitive(htonll(data));
    }

    inline void writeEnumInSingleByte(int value) {
        assert(std::numeric_limits<int8_t>::min() <= value &&
                value <= std::numeric_limits<int8_t>::max());
        writeByte(static_cast<int8_t>(value));
    }

    inline size_t writeCharAt(size_t position, char value) {
        return writePrimitiveAt(position, value);
    }

    inline size_t writeByteAt(size_t position, int8_t value) {
        return writePrimitiveAt(position, value);
    }

    inline size_t writeShortAt(size_t position, int16_t value) {
        return writePrimitiveAt(position, htons(value));
    }

    inline size_t writeIntAt(size_t position, int32_t value) {
        return writePrimitiveAt(position, htonl(value));
    }

    inline size_t writeBoolAt(size_t position, bool value) {
        return writePrimitiveAt(position, value ? int8_t(1) : int8_t(0));
    }

    inline size_t writeLongAt(size_t position, int64_t value) {
        return writePrimitiveAt(position, htonll(value));
    }

    inline size_t writeFloatAt(size_t position, float value) {
        int32_t data;
        memcpy(&data, &value, sizeof(data));
        return writePrimitiveAt(position, htonl(data));
    }

    inline size_t writeDoubleAt(size_t position, double value) {
        int64_t data;
        memcpy(&data, &value, sizeof(data));
        return writePrimitiveAt(position, htonll(data));
    }

    // this explicitly accepts char* and length (or ByteArray)
    // as std::string's implicit construction is unsafe!
    inline void writeBinaryString(const void* value, size_t length) {
        int32_t stringLength = static_cast<int32_t>(length);
        assureExpand(length + sizeof(stringLength));

        // do a newtork order conversion
        int32_t networkOrderLen = htonl(stringLength);

        char* current = buffer_ + position_;
        memcpy(current, &networkOrderLen, sizeof(networkOrderLen));
        current += sizeof(stringLength);
        memcpy(current, value, length);
        position_ += sizeof(stringLength) + length;
    }

    inline void writeBinaryString(const ByteArray &value) {
        writeBinaryString(value.data(), value.length());
    }

    inline void writeTextString(const std::string &value) {
        writeBinaryString(value.data(), value.size());
    }

    inline void writeBytes(const void *value, size_t length) {
        assureExpand(length);
        memcpy(buffer_ + position_, value, length);
        position_ += length;
    }

    inline void writeZeros(size_t length) {
        assureExpand(length);
        memset(buffer_ + position_, 0, length);
        position_ += length;
    }

    /** Reserves length bytes of space for writing. Returns the offset to the bytes. */
    size_t reserveBytes(size_t length) {
        assureExpand(length);
        size_t offset = position_;
        position_ += length;
        return offset;
    }

    /** Copies length bytes from value to this buffer, starting at
    offset. Offset should have been obtained from reserveBytes. This
    does not affect the current write position.  * @return offset +
    length */
    inline size_t writeBytesAt(size_t offset, const void *value, size_t length) {
        assert(offset + length <= position_);
        memcpy(buffer_ + offset, value, length);
        return offset + length;
    }

    static bool isLittleEndian() {
        static const uint16_t s = 0x0001;
        uint8_t byte;
        memcpy(&byte, &s, 1);
        return byte != 0;
    }

    std::size_t position() {
        return position_;
    }

protected:

    /** Called when trying to write past the end of the
    buffer. Subclasses can optionally resize the buffer by calling
    initialize. If this function returns and size() < minimum_desired,
    the program will crash.  @param minimum_desired the minimum length
    the resized buffer needs to have. */
    virtual void expand(size_t minimum_desired) = 0;

private:
    template <typename T>
    void writePrimitive(T value) {
        assureExpand(sizeof(value));
        memcpy(buffer_ + position_, &value, sizeof(value));
        position_ += sizeof(value);
    }

    template <typename T>
    size_t writePrimitiveAt(size_t position, T value) {
        return writeBytesAt(position, &value, sizeof(value));
    }

    inline void assureExpand(size_t next_write) {
        size_t minimum_desired = position_ + next_write;
        if (minimum_desired > capacity_) {
            expand(minimum_desired);
        }
        assert(capacity_ >= minimum_desired);
    }

    // Beginning of the buffer.
    char* buffer_;

    // No implicit copies
    SerializeOutput(const SerializeOutput&);
    SerializeOutput& operator=(const SerializeOutput&);

protected:
    // Current write position in the buffer.
    size_t position_;
    // Total bytes this buffer can contain.
    size_t capacity_;
};

/** Implementation of SerializeInput that references an existing buffer. */
class ReferenceSerializeInput : public SerializeInput {
public:
    ReferenceSerializeInput(const void* data, size_t length) {
        initialize(data, length);
    }

    // Destructor does nothing: nothing to clean up!
    virtual ~ReferenceSerializeInput() {}
};

/** Implementation of SerializeInput that makes a copy of the buffer. */
class CopySerializeInput : public SerializeInput {
public:
    CopySerializeInput(const void* data, size_t length) :
            bytes_(reinterpret_cast<const char*>(data), static_cast<int>(length)) {
        initialize(bytes_.data(), static_cast<int>(length));
    }

    // Destructor frees the ByteArray.
    virtual ~CopySerializeInput() {}

private:
    ByteArray bytes_;
};

/** Implementation of SerializeOutput that references an existing buffer. */
class ReferenceSerializeOutput : public SerializeOutput {
public:
    ReferenceSerializeOutput() : SerializeOutput() {
    }
    ReferenceSerializeOutput(void* data, size_t length) : SerializeOutput() {
        initialize(data, length);
    }

    /** Set the buffer to buffer with capacity and sets the position. */
    void initializeWithPosition(void* buffer, size_t capacity, size_t position) {
        setPosition(position);
        initialize(buffer, capacity);
    }

    size_t remaining() {
        return capacity_ - position_;
    }

    // Destructor does nothing: nothing to clean up!
    virtual ~ReferenceSerializeOutput() {}

protected:
    /** Reference output can't resize the buffer: Frowny-Face. */
    virtual void expand(size_t minimum_desired) {
        throw SQLException(SQLException::volt_output_buffer_overflow,
            "Output from SQL stmt overflowed output/network buffer of 10mb. "
            "Try a \"limit\" clause or a stronger predicate.");
    }
};

/** Implementation of SerializeOutput that makes a copy of the buffer. */
class CopySerializeOutput : public SerializeOutput {
public:
    // Start with something sizeable so we avoid a ton of initial
    // allocations.
    static const int INITIAL_SIZE = 8388608;

    CopySerializeOutput() : bytes_(INITIAL_SIZE) {
        initialize(bytes_.data(), INITIAL_SIZE);
    }

    // Destructor frees the ByteArray.
    virtual ~CopySerializeOutput() {}

    void reset() {
        setPosition(0);
    }

    int remaining() {
        return bytes_.length() - static_cast<int>(position());
    }

protected:
    /** Resize this buffer to contain twice the amount desired. */
    virtual void expand(size_t minimum_desired) {
        size_t next_capacity = (bytes_.length() + minimum_desired) * 2;
        assert(next_capacity < static_cast<size_t>(std::numeric_limits<int>::max()));
        bytes_.copyAndExpand(static_cast<int>(next_capacity));
        initialize(bytes_.data(), next_capacity);
    }

private:
    ByteArray bytes_;
};

/*
* A serialize output class that falls back to allocating a 50 meg buffer
* if the regular allocation runs out of space. The topend is notified when this occurs.
*/
class FallbackSerializeOutput : public ReferenceSerializeOutput {
public:
    FallbackSerializeOutput() :
        ReferenceSerializeOutput(), fallbackBuffer_(NULL) {
    }

    /** Set the buffer to buffer with capacity and sets the position. */
    void initializeWithPosition(void* buffer, size_t capacity, size_t position) {
        if (fallbackBuffer_ != NULL) {
            char *temp = fallbackBuffer_;
            fallbackBuffer_ = NULL;
            delete []temp;
        }
        setPosition(position);
        initialize(buffer, capacity);
    }

    // Destructor frees the fallback buffer if it is allocated
    virtual ~FallbackSerializeOutput() {
        delete []fallbackBuffer_;
    }

    /** Expand once to a fallback size, and if that doesn't work abort */
    virtual void expand(size_t minimum_desired){
    	  /*
    	* Leave some space for message headers and such, almost 50 megabytes
    	*/
    	    size_t maxAllocationSize = ((1024 * 1024 *50) - (1024 * 32));
    	    if (fallbackBuffer_ != NULL || minimum_desired > maxAllocationSize) {
    	        if (fallbackBuffer_ != NULL) {
    	            char *temp = fallbackBuffer_;
    	            fallbackBuffer_ = NULL;
    	            delete []temp;
    	        }
    	        throw SQLException(SQLException::volt_output_buffer_overflow,
    	            "Output from SQL stmt overflowed output/network buffer of 50mb (-32k for message headers). "
    	            "Try a \"limit\" clause or a stronger predicate.");
    	    }
    	    fallbackBuffer_ = new char[maxAllocationSize];
    	    ::memcpy(fallbackBuffer_, data(), position_);
    	    setPosition(position_);
    	    initialize(fallbackBuffer_, maxAllocationSize);
    	    //XXX ExecutorContext::getExecutorContext()->getTopend()->fallbackToEEAllocatedBuffer(fallbackBuffer_, maxAllocationSize);
    }
private:
    char *fallbackBuffer_;
};



}
#endif
