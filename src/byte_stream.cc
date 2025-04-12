#include "byte_stream.hh"
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  string add_stream = data.substr( 0, available_capacity() );
  stream_.append( add_stream );
  write_byte_num_ += add_stream.size();
}

void Writer::close()
{
  if ( is_closed() ) {
    return;
  }
  closed_ = true;
}

bool Writer::is_closed() const
{
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - stream_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return write_byte_num_;
}

/* The below code is rdt reader functions */

string_view Reader::peek() const
{
  return stream_; // Your code here.
}

void Reader::pop( uint64_t len )
{
  stream_ = stream_.substr( len );
  read_byte_num_ += len;
}

bool Reader::is_finished() const
{
  return closed_ && stream_.size() == 0; // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return stream_.size(); // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return read_byte_num_; // Your code here.
}
