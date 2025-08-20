#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), stream_start_(0) {}

void Writer::push( string data )
{
  uint64_t add_num = min(available_capacity(), data.size());
  buffer_.append(data, 0, add_num);
  write_byte_num_ += add_num;
}

void Writer::close()
{
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( buffer_.size() - stream_start_ );
}

uint64_t Writer::bytes_pushed() const
{
  return write_byte_num_;
}

/* The below code is rdt reader functions */

string_view Reader::peek() const
{
  return string_view(buffer_.data() + stream_start_, buffer_.size() - stream_start_);
}

void Reader::pop( uint64_t len )
{
  stream_start_ += len;
  if (stream_start_ >= buffer_.size() / 2) {
    buffer_.erase(0, stream_start_);
    stream_start_ = 0;
  }
  read_byte_num_ += len;
}

bool Reader::is_finished() const
{
  return closed_ && buffer_.size() - stream_start_ == 0 ; // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size() - stream_start_; // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return read_byte_num_; // Your code here.
}
