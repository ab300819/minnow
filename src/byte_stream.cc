#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity )
  , buffer_()
  , is_close_( false )
  , has_error_( false )
  , bytes_pushed_( 0 )
  , bytes_popped_( 0 )
{}

void Writer::push( string data )
{
  for ( auto c : data ) {
    if ( available_capacity() > 0 ) {
      this->buffer_.push( c );
      this->bytes_pushed_++;
    }
  }
  (void)data;
}

void Writer::close()
{
  this->is_close_ = true;
}

void Writer::set_error()
{
  this->has_error_ = true;
}

bool Writer::is_closed() const
{
  return this->is_close_;
}

uint64_t Writer::available_capacity() const
{
  auto current_size = buffer_.size();
  return current_size < capacity_ ? capacity_ - current_size : 0;
}

uint64_t Writer::bytes_pushed() const
{
  return this->bytes_pushed_;
}

string_view Reader::peek() const
{
  if ( buffer_.empty() ) {
    return {};
  }

  return { &buffer_.front(), 1 };
}

bool Reader::is_finished() const
{
  return this->is_close_ && this->buffer_.empty();
}

bool Reader::has_error() const
{
  return this->has_error_;
}

void Reader::pop( uint64_t len )
{
  for ( uint64_t i = 0; i < len; i++ ) {
    buffer_.pop();
    this->bytes_popped_++;
  }
  (void)len;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return this->bytes_popped_;
}
