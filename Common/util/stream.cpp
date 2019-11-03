//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include "util/stream.h"

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include "util/stdio_compat.h"

namespace AGS {
namespace Common {

// TODO:
// stream cannot raise exceptions
// and implement Has_Errors
// use the stdio_compat functions.


StreamError::StreamError(const AGS::Common::String &what) : std::runtime_error(what.GetCStr()) {}

// --------------------------------------------------------------------------------------------------------------------
// FileStream
// --------------------------------------------------------------------------------------------------------------------

FileStream::FileStream(const String &file_name, FileOpenMode open_mode, FileWorkMode work_mode) 
    : file_name_(file_name), open_mode_(open_mode), work_mode_(work_mode), mode_(File::GetCMode(open_mode, work_mode))
{
    if (mode_.IsEmpty()) {
        throw StreamError("Error determining file open mode");
    }

    file_ = std::fopen(file_name.GetCStr(), mode_.GetCStr());
    if (file_ == nullptr) {
        throw StreamError("Error opening file");
    }
}

FileStream::~FileStream()
{
    if (file_ != nullptr) {
        std::fclose(file_);
    }
    file_ = nullptr;
}

void FileStream::Flush()
{
    auto result = std::fflush(file_);
    if (result != 0) {
        throw StreamError(std::strerror(errno));
    }
}

bool FileStream::EOS() const
{
    return std::feof(file_);
}


file_off_t FileStream::GetPosition() const
{
    auto result = ags_ftell(file_);
    if (result == -1L) {
        // we don't check for the success of GetPosition anywhere, so raise an exception in this case.
        throw StreamError(std::strerror(errno));
    }
    return result;
}


size_t FileStream::Read(void *buffer, size_t buffer_size)
{
    if (buffer_size <= 0) { return 0; }
    if (buffer == nullptr) { return 0; }

    return std::fread(buffer, 1, buffer_size, file_);

}

size_t FileStream::Write(const void *buffer, size_t buffer_size)
{
    if (buffer_size <= 0) { return 0; }
    if (buffer == nullptr) { return 0; }

    return std::fwrite(buffer, 1, buffer_size, file_);
}

void FileStream::Seek(file_off_t offset, StreamSeek origin)
{
    int fseek_origin;
    switch(origin) {
        case kSeekBegin:    fseek_origin = SEEK_SET; break;
        case kSeekCurrent:  fseek_origin = SEEK_CUR; break;
        case kSeekEnd:      fseek_origin = SEEK_END; break;
    }

    if (ags_fseek(file_, offset, fseek_origin) != 0) {
        // we don't check for the success of Seek anywhere, so raise an exception in this case.
        throw StreamError(std::strerror(errno));
    }
}



// --------------------------------------------------------------------------------------------------------------------
// BufferedStream
// --------------------------------------------------------------------------------------------------------------------

const auto BufferStreamSize = 2*512*1024;

BufferedStream::BufferedStream(std::unique_ptr<ICoreStream> stream) :
    stream_(std::move(stream)), bufferPosition_(0), buffer_(0), position_(0), eos_reached_(false)
{
    stream_->Seek(0, kSeekEnd);

    end_ = stream_->GetPosition();

    stream_->Seek(0, kSeekBegin);
}

void BufferedStream::FillBufferFromPosition(file_off_t position)
{
    stream_->Seek(position, kSeekBegin);
    assert(stream_->GetPosition() == position);

    buffer_.resize(BufferStreamSize);
    auto sz = stream_->Read(buffer_.data(), BufferStreamSize);
    assert(sz >= 0);
    buffer_.resize(sz);

    bufferPosition_ = position;
}

bool BufferedStream::EOS() const
{
    return eos_reached_;
}

size_t BufferedStream::Read(void *toBuffer, size_t toSize) 
{
    if (toSize <= 0) { return 0; }
    if (toBuffer == nullptr) { return 0; }

    eos_reached_ = false;

    auto toRemaining = toSize;
    auto to = static_cast<char *>(toBuffer);

    while (toRemaining > 0)
    {
        if (position_ >= end_) { 
            eos_reached_ = true;
            break; 
        }

        if ((position_ < bufferPosition_) || (position_ >= bufferPosition_ + buffer_.size()))
        {
            FillBufferFromPosition(position_);
        }
        if (buffer_.size() <= 0) { break; } // reached EOS
        assert((position_ >= bufferPosition_) && (position_ < bufferPosition_ + buffer_.size()));  // sanity check only, should be checked by above.

        file_off_t bufferOffset = position_ - bufferPosition_;
        assert(bufferOffset >= 0);
        size_t bytesLeft = buffer_.size() - (size_t)bufferOffset;
        size_t chunkSize = std::min<size_t>(bytesLeft, toRemaining);

        std::memcpy(to, buffer_.data() + bufferOffset, chunkSize);

        to += chunkSize;
        position_ += chunkSize;
        toRemaining -= chunkSize;
    }

    return to - (char*)toBuffer;
}

size_t BufferedStream::Write(const void *buffer, size_t size)
{
    stream_->Seek(position_, kSeekBegin);
    auto sz = stream_->Write(buffer, size);
    if (position_ == end_)
        end_ += sz;
    position_ += sz;
    return sz;
}

void BufferedStream::Flush() { stream_->Flush(); }

file_off_t BufferedStream::GetPosition() const
{
    return position_;
}

void BufferedStream::Seek(file_off_t offset, StreamSeek origin)
{
    eos_reached_ = false;

    file_off_t want_pos = -1;
    switch(origin)
    {
        case StreamSeek::kSeekCurrent:  want_pos = position_   + offset; break;
        case StreamSeek::kSeekBegin:    want_pos = 0           + offset; break;
        case StreamSeek::kSeekEnd:      want_pos = end_        + offset; break;
        break;
    }

    position_ = std::min(std::max(want_pos, (file_off_t)0), end_);
}


// --------------------------------------------------------------------------------------------------------------------
// MemoryStream
// --------------------------------------------------------------------------------------------------------------------

MemoryStream::MemoryStream(std::vector<char> &buffer) : buffer_(std::move(buffer)), position_(0) {}

bool MemoryStream::EOS() const { return position_ >= buffer_.size(); }

size_t MemoryStream::Read(void *buffer, size_t size)
{
    if (position_ >= buffer_.size()) { return 0; }
    size_t remaining = buffer_.size() - (size_t)position_;
    if (remaining <= 0) { return 0; }
    assert(remaining > 0);
    // size_t should be okay because we check for negative `remaining` above.
    auto read_sz = std::min<size_t>(remaining, size);
    memcpy(buffer, buffer_.data() + position_, read_sz);
    position_ += read_sz;
    return read_sz;
}

size_t MemoryStream::Write(const void *buffer, size_t size) { return 0; }
void MemoryStream::Flush() { }

file_off_t MemoryStream::GetPosition() const { return position_; }
void MemoryStream::Seek(file_off_t offset, StreamSeek origin)
{
    switch (origin) {
    case kSeekBegin:    position_ = 0 + offset;  break;
    case kSeekCurrent:  position_ = position_ + offset;  break;
    case kSeekEnd:      position_ = buffer_.size() + offset; break;
    }
    if (position_ < 0) { position_ = 0; }
    // end of stream is always one past the last byte.
    if (position_ > buffer_.size()) { position_ = buffer_.size(); }
}



// --------------------------------------------------------------------------------------------------------------------
// CompareStream
// --------------------------------------------------------------------------------------------------------------------

CompareStream::CompareStream(std::unique_ptr<ICoreStream> stream1, std::unique_ptr<ICoreStream> stream2) :
    stream1_(std::move(stream1)), stream2_(std::move(stream2))
{
}

bool CompareStream::EOS() const
{
    auto res1 = stream1_->EOS();
    auto res2 = stream2_->EOS();
    assert(res1 == res2);
    return res1;
}

size_t CompareStream::Read(void *toBuffer, size_t toSize) 
{
    std::vector<char> otherBuf(toSize);
    auto res1 = stream1_->Read(otherBuf.data(), toSize);

    auto res2 = stream2_->Read(toBuffer, toSize);

    assert(res1 == res2);
    assert(memcmp(toBuffer, otherBuf.data(), res1) == 0);

    auto pos1 = stream1_->GetPosition();
    auto pos2 = stream2_->GetPosition();
    assert(pos1 == pos2);

    return res1;
}

size_t CompareStream::Write(const void *buffer, size_t size)
{
    auto res1 = stream1_->Write(buffer, size);
    auto res2 = stream2_->Write(buffer, size);
    assert(res1 == res2);
    return res1;
}

void CompareStream::Flush() 
{ 
    stream1_->Flush();
    stream2_->Flush();
}

file_off_t CompareStream::GetPosition() const
{
    auto res1 = stream1_->GetPosition();
    auto res2 = stream2_->GetPosition();
    assert(res1 == res2);
    return res1;
}

void CompareStream::Seek(file_off_t offset, StreamSeek origin)
{
    stream1_->Seek(offset, origin);
    stream2_->Seek(offset, origin);

    auto res1 = stream1_->GetPosition();
    auto res2 = stream2_->GetPosition();
    assert(res1 == res2);
}



// --------------------------------------------------------------------------------------------------------------------
// DataStream
// --------------------------------------------------------------------------------------------------------------------

DataStream::DataStream(
    std::unique_ptr<ICoreStream> stream, 
    DataEndianess stream_endianess
) : 
    stream_(std::move(stream)), 
    stream_endianess_(stream_endianess) 
{}

bool DataStream::EOS() const { return stream_->EOS(); }
file_off_t DataStream::GetPosition() const { return stream_->GetPosition(); }


bool DataStream::HasErrors() const { return false; };

file_off_t DataStream::GetLength() const {
    auto pos = stream_->GetPosition();

    stream_->Seek(0, AGS::Common::kSeekEnd);
    auto end = stream_->GetPosition();

    stream_->Seek(pos, AGS::Common::kSeekBegin);

    return end;
}

file_off_t DataStream::Seek(file_off_t offset, StreamSeek origin) {
    stream_->Seek(offset, origin);
    return stream_->GetPosition();
}

bool DataStream::Flush() { 
    try {
        stream_->Flush();
    } catch (StreamError) {
        return false;
    }
    return true;
}



// Reading

size_t DataStream::Read(void *buffer, size_t size) { return stream_->Read(buffer, size); }

// return unsigned value or -1
int32_t DataStream::ReadByte() {
    using T = uint8_t;
    T val;
    auto bytesRead = Read(&val, sizeof(T));
    if (bytesRead != sizeof(T)) { return EOF; }
    return val;
}

int8_t DataStream::ReadInt8() { 
    using T = int8_t;
    T val;
    auto bytesRead = Read(&val, sizeof(T));
    if (bytesRead != sizeof(T)) { return EOF; }
    return val;
}

int16_t DataStream::ReadInt16()
{
    using T = int16_t;
    T val;
    auto bytesRead = Read(&val, sizeof(T));
    if (bytesRead != sizeof(T)) { return EOF; }
    ConvertInt16(val);
    return val;
}

int32_t DataStream::ReadInt32()
{
    using T = int32_t;
    T val;
    auto bytesRead = Read(&val, sizeof(T));
    if (bytesRead != sizeof(T)) { return EOF; }
    ConvertInt32(val);
    return val;
}

int64_t DataStream::ReadInt64()
{
    using T = int64_t;
    T val;
    auto bytesRead = Read(&val, sizeof(T));
    if (bytesRead != sizeof(T)) { return EOF; }
    ConvertInt64(val);
    return val;
}

bool DataStream::ReadBool() { return ReadInt8() != 0; }

size_t DataStream::ReadArray(void *buffer, size_t elem_size, size_t count)
{
    return Read(buffer, elem_size * count) / elem_size;
}

size_t DataStream::ReadArrayOfInt8(int8_t *buffer, size_t count) 
{
    using T = int8_t;
    auto result = ReadArray(buffer, sizeof(T), count);
    return result;
}

size_t DataStream::ReadArrayOfInt16(int16_t *buffer, size_t count)
{
    using T = int16_t;
    auto result = ReadArray(buffer, sizeof(T), count);
    if (MustSwapBytes()) {
        for (size_t i = 0; i < count; i++) {
            buffer[i] = BBOp::SwapBytesInt16(buffer[i]);
        }
    }
    return result;
}

size_t DataStream::ReadArrayOfInt32(int32_t *buffer, size_t count)
{
    using T = int32_t;
    auto result = ReadArray(buffer, sizeof(T), count);
    if (MustSwapBytes()) {
        for (size_t i = 0; i < count; i++) {
            buffer[i] = BBOp::SwapBytesInt32(buffer[i]);
        }
    }
    return result;
}

size_t DataStream::ReadArrayOfInt64(int64_t *buffer, size_t count)
{
    using T = int64_t;
    auto result = ReadArray(buffer, sizeof(T), count);
    if (MustSwapBytes()) {
        for (size_t i = 0; i < count; i++) {
            buffer[i] = BBOp::SwapBytesInt64(buffer[i]);
        }
    }
    return result;
}


// Writing

size_t DataStream::Write(const void *buffer, size_t size) { return stream_->Write(buffer, size); }

// return unsigned value or -1
int32_t DataStream::WriteByte(uint8_t val)
{
    using T = uint8_t;
    T ch = val;
    auto bytesWritten = stream_->Write(&ch, sizeof(T));
    return bytesWritten == sizeof(T) ? ch : EOF;
}

size_t DataStream::WriteInt8(int8_t val) 
{
    using T = int8_t;
    T valb = val;
    auto bytesWritten = stream_->Write(&valb, sizeof(T));
    return bytesWritten == sizeof(T) ? 1 : 0;
}

size_t DataStream::WriteInt16(int16_t val)
{
    using T = int16_t;
    T valb = val;
    ConvertInt16(valb);
    auto bytesWritten = stream_->Write(&valb, sizeof(T));
    return bytesWritten == sizeof(T) ? 1 : 0;
}

size_t DataStream::WriteInt32(int32_t val)
{
    using T = int32_t;
    T valb = val;
    ConvertInt32(valb);
    auto bytesWritten = stream_->Write(&valb, sizeof(T));
    return bytesWritten == sizeof(T) ? 1 : 0;
}

size_t DataStream::WriteInt64(int64_t val)
{
    using T = int64_t;
    T valb = val;
    ConvertInt64(valb);
    auto bytesWritten = stream_->Write(&valb, sizeof(T));
    return bytesWritten == sizeof(T) ? 1 : 0;
}

size_t DataStream::WriteBool(bool val) { return WriteInt8(val ? 1 : 0); }

size_t DataStream::WriteByteCount(uint8_t b, size_t count)
{
    size_t writeCount = 0;
    for (size_t i = 0; i < count; i++) {
        auto result = WriteByte(b);
        if (result < 0) { break; }
        writeCount += 1;
    }
    return writeCount;
}

size_t DataStream::WriteArray(const void *buffer, size_t elem_size, size_t count)
{
    return Write(buffer, elem_size * count) / elem_size;
}

size_t DataStream::WriteArrayOfInt8(const int8_t *buffer, size_t count) 
{
    using T = int8_t;
    return WriteArray(buffer, sizeof(T), count);
}

size_t DataStream::WriteArrayOfInt16(const int16_t *buffer, size_t count)
{
    using T = int16_t;
    if (!MustSwapBytes()) {
        return WriteArray(buffer, sizeof(T), count);
    }

    size_t writeCount = 0;
    for (size_t i = 0; i < count; i++) {
        auto valsWritten = WriteInt16(buffer[i]);
        if (valsWritten <= 0) { break; }
        writeCount += 1;
    }
    return writeCount;
}

size_t DataStream::WriteArrayOfInt32(const int32_t *buffer, size_t count)
{
    using T = int32_t;
    if (!MustSwapBytes()) {
        return WriteArray(buffer, sizeof(T), count);
    }

    size_t writeCount = 0;
    for (size_t i = 0; i < count; i++) {
        auto valsWritten = WriteInt32(buffer[i]);
        if (valsWritten <= 0) { break; }
        writeCount += 1;
    }
    return writeCount;
}

size_t DataStream::WriteArrayOfInt64(const int64_t *buffer, size_t count)
{
    using T = int64_t;
    if (!MustSwapBytes()) {
        return WriteArray(buffer, sizeof(T), count);
    }

    size_t writeCount = 0;
    for (size_t i = 0; i < count; i++) {
        auto valsWritten = WriteInt64(buffer[i]);
        if (valsWritten <= 0) { break; }
        writeCount += 1;
    }
    return writeCount;
}



bool DataStream::MustSwapBytes()
{
    return kDefaultSystemEndianess != stream_endianess_;
}

void DataStream::ConvertInt16(int16_t &val)
{
    if (MustSwapBytes()) val = BBOp::SwapBytesInt16(val);
}
void DataStream::ConvertInt32(int32_t &val)
{
    if (MustSwapBytes()) val = BBOp::SwapBytesInt32(val);
}
void DataStream::ConvertInt64(int64_t &val)
{
    if (MustSwapBytes()) val = BBOp::SwapBytesInt64(val);
}


// --------------------------------------------------------------------------------------------------------------------
// AlignedStream
// --------------------------------------------------------------------------------------------------------------------


AlignedStream::AlignedStream(Stream *stream,
    AlignedStreamMode mode,
    ObjectOwnershipPolicy stream_ownership_policy,
    size_t base_alignment
) :
    stream_(stream),
    streamOwnershipPolicy_(stream_ownership_policy),
    mode_(mode), 
    baseAlignment_(base_alignment), 
    maxAlignment_(0), 
    block_(0) 
{}

AlignedStream::~AlignedStream() 
{ 
    AlignedStream::FinalizeBlock(); 

    if (streamOwnershipPolicy_ == kDisposeAfterUse) {
        delete stream_;
        stream_ = nullptr;
    }
}


bool AlignedStream::EOS() const { return stream_->EOS(); };
bool AlignedStream::Flush() { return stream_->Flush(); };
file_off_t AlignedStream::GetLength() const { return stream_->GetLength(); };
file_off_t AlignedStream::GetPosition() const { return stream_->GetPosition(); };

file_off_t AlignedStream::Seek(file_off_t offset, StreamSeek origin) { throw std::logic_error("Not implemented"); }
bool AlignedStream::HasErrors() const { return false; }
void AlignedStream::Reset() { FinalizeBlock(); }


// Read

size_t AlignedStream::Read(void *buffer, size_t size)
{
    ReadPadding(sizeof(int8_t));
    size = stream_->Read(buffer, size);
    block_ += size;
    return size;
}

int32_t AlignedStream::ReadByte()
{
    uint8_t b = 0;
    ReadPadding(sizeof(uint8_t));
    b = stream_->ReadByte();
    block_ += sizeof(uint8_t);
    return b;
}

bool AlignedStream::ReadBool() { 
    return ReadInt8() != 0;
};

int8_t AlignedStream::ReadInt8()
{
    int8_t val = 0;
    ReadPadding(sizeof(int8_t));
    val = stream_->ReadInt8();
    block_ += sizeof(int8_t);
    return val;
}

int16_t AlignedStream::ReadInt16()
{
    int16_t val = 0;
    ReadPadding(sizeof(int16_t));
    val = stream_->ReadInt16();
    block_ += sizeof(int16_t);
    return val;
}

int32_t AlignedStream::ReadInt32()
{
    int32_t val = 0;
    ReadPadding(sizeof(int32_t));
    val = stream_->ReadInt32();
    block_ += sizeof(int32_t);
    return val;
}

int64_t AlignedStream::ReadInt64()
{
    int64_t val = 0;
    ReadPadding(sizeof(int64_t));
    val = stream_->ReadInt64();
    block_ += sizeof(int64_t);
    return val;
}

size_t AlignedStream::ReadArray(void *buffer, size_t elem_size, size_t count)
{
    ReadPadding(elem_size);
    count = stream_->ReadArray(buffer, elem_size, count);
    block_ += count * elem_size;
    return count;
}

size_t AlignedStream::ReadArrayOfInt8(int8_t *buffer, size_t count)
{
    ReadPadding(sizeof(int8_t));
    count = stream_->ReadArrayOfInt8(buffer, count);
    block_ += count * sizeof(int8_t);
    return count;
}

size_t AlignedStream::ReadArrayOfInt16(int16_t *buffer, size_t count)
{
    ReadPadding(sizeof(int16_t));
    count = stream_->ReadArrayOfInt16(buffer, count);
    block_ += count * sizeof(int16_t);
    return count;
}

size_t AlignedStream::ReadArrayOfInt32(int32_t *buffer, size_t count)
{
    ReadPadding(sizeof(int32_t));
    count = stream_->ReadArrayOfInt32(buffer, count);
    block_ += count * sizeof(int32_t);
    return count;
}

size_t AlignedStream::ReadArrayOfInt64(int64_t *buffer, size_t count)
{
    ReadPadding(sizeof(int64_t));
    count = stream_->ReadArrayOfInt64(buffer, count);
    block_ += count * sizeof(int64_t);
    return count;
}

// Write

size_t AlignedStream::Write(const void *buffer, size_t size)
{
    WritePadding(sizeof(int8_t));
    size = stream_->Write(buffer, size);
    block_ += size;
    return size;
}

int32_t AlignedStream::WriteByte(uint8_t b)
{
    WritePadding(sizeof(uint8_t));
    b = stream_->WriteByte(b);
    block_ += sizeof(uint8_t);
    return b;
}

size_t AlignedStream::WriteByteCount(uint8_t b, size_t count) { 
    WritePadding(sizeof(uint8_t));
    auto bytesWritten = stream_->WriteByteCount(b, count); 
    block_ += bytesWritten;
    return bytesWritten;
};

size_t AlignedStream::WriteInt8(int8_t val)
{
    WritePadding(sizeof(int8_t));
    size_t size = stream_->WriteInt8(val);
    block_ += sizeof(int8_t);
    return size;
}

size_t AlignedStream::WriteBool(bool val)
{
    return WriteInt8(val ? 1 : 0);
}

size_t AlignedStream::WriteInt16(int16_t val)
{
    WritePadding(sizeof(int16_t));
    size_t size = stream_->WriteInt16(val);
    block_ += sizeof(int16_t);
    return size;
}

size_t AlignedStream::WriteInt32(int32_t val)
{
    WritePadding(sizeof(int32_t));
    size_t size = stream_->WriteInt32(val);
    block_ += sizeof(int32_t);
    return size;
}

size_t AlignedStream::WriteInt64(int64_t val)
{
    WritePadding(sizeof(int64_t));
    size_t size = stream_->WriteInt64(val);
    block_ += sizeof(int64_t);
    return size;
}

size_t AlignedStream::WriteArray(const void *buffer, size_t elem_size, size_t count)
{
    WritePadding(elem_size);
    count = stream_->WriteArray(buffer, elem_size, count);
    block_ += count * elem_size;
    return count;
}

size_t AlignedStream::WriteArrayOfInt8(const int8_t *buffer, size_t count)
{
    WritePadding(sizeof(int8_t));
    count = stream_->WriteArrayOfInt8(buffer, count);
    block_ += count * sizeof(int8_t);
    return count;
}

size_t AlignedStream::WriteArrayOfInt16(const int16_t *buffer, size_t count)
{
    WritePadding(sizeof(int16_t));
    count = stream_->WriteArrayOfInt16(buffer, count);
    block_ += count * sizeof(int16_t);
    return count;
}

size_t AlignedStream::WriteArrayOfInt32(const int32_t *buffer, size_t count)
{
    WritePadding(sizeof(int32_t));
    count = stream_->WriteArrayOfInt32(buffer, count);
    block_ += count * sizeof(int32_t);
    return count;
}

size_t AlignedStream::WriteArrayOfInt64(const int64_t *buffer, size_t count)
{
    WritePadding(sizeof(int64_t));
    count = stream_->WriteArrayOfInt64(buffer, count);
    block_ += count * sizeof(int64_t);
    return count;
}


// Padding

void AlignedStream::ReadPadding(size_t next_type)
{
    if (next_type == 0)
    {
        return;
    }

    // The next is going to be evenly aligned data type,
    // therefore a padding check must be made
    if (next_type % baseAlignment_ == 0)
    {
        int pad = block_ % next_type;
        // Read padding only if have to
        if (pad)
        {
            // We do not know and should not care if the underlying stream
            // supports seek, so use read to skip the padding instead.
            for (size_t i = next_type - pad; i > 0; --i)
                stream_->ReadByte();
            block_ += next_type - pad;
        }

        maxAlignment_ = std::max(maxAlignment_, next_type);
        // Data is evenly aligned now
        if (block_ % LargestPossibleType == 0)
        {
            block_ = 0;
        }
    }
}

void AlignedStream::WritePadding(size_t next_type)
{
    if (next_type == 0)
    {
        return;
    }

    // The next is going to be evenly aligned data type,
    // therefore a padding check must be made
    if (next_type % baseAlignment_ == 0)
    {
        int pad = block_ % next_type;
        // Write padding only if have to
        if (pad)
        {
            stream_->WriteByteCount(0, next_type - pad);
            block_ += next_type - pad;
        }

        maxAlignment_ = std::max(maxAlignment_, next_type);
        // Data is evenly aligned now
        if (block_ % LargestPossibleType == 0)
        {
            block_ = 0;
        }
    }
}

void AlignedStream::FinalizeBlock()
{
    // Force the stream to read or write remaining padding to match the alignment
    if (mode_ == kAligned_Read)
    {
        ReadPadding(maxAlignment_);
    }
    else if (mode_ == kAligned_Write)
    {
        WritePadding(maxAlignment_);
    }

    maxAlignment_ = 0;
    block_ = 0;
}

} // namespace Common
} // namespace AGS
