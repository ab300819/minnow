#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{

  if ( is_last_substring ) {
    if ( this->_next_index > 0 ) {
      output.close();
    } else {
      // 如果已经关闭，但是未来的字符串先来，先等待关闭
      this->wait_to_close = true;
    }
  }

  // 已经在等待关闭，且首字符已经到就开始关闭
  if ( this->wait_to_close && first_index == 0 ) {
    output.close();
  }

  if ( data.empty() ) {
    return;
  }

  auto write_result = false;
  if ( this->_next_index == first_index ) {
    // write
    write_result = this->write( first_index, data, output );

  } else if ( this->_pre_index == first_index && ( first_index + data.size() - 1 ) >= this->_next_index ) {

    // duplicate
    auto start_byte = this->_next_index - this->_pre_index;
    auto subStr = data.substr( start_byte, std::string::npos );
    write_result = this->write( this->_next_index, subStr, output );

  } else if ( first_index < this->_next_index && this->_next_index <= ( first_index + data.size() - 1 ) ) {

    // write party
    auto subStr = data.substr( 0, data.size() - ( this->_next_index - first_index ) );
    write_result = this->write( this->_next_index, subStr, output );
  } else if ( this->_next_index < first_index ) {
    // cache
    write_to_buffer( first_index, data, output );

  } else if ( this->_next_index > ( first_index + data.size() - 1 ) ) {
    // drop
  }

  if ( write_result ) {
    this->_pre_index = first_index;
  }

  try_to_write_all( output );

  (void)first_index;
  (void)data;
  (void)is_last_substring;
  (void)output;
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t buffer_size {};
  for ( const auto& pair : this->_buffer ) {
    buffer_size += pair.second.size();
  }
  return buffer_size;
}
bool Reassembler::write( uint64_t index, std::string data, Writer& output )
{
  auto available_capacity = output.available_capacity();
  if ( available_capacity == 0 ) {
    return false;
  }

  auto data_size = data.size();
  if ( data_size <= available_capacity ) {
    output.push( data );
    this->_next_index = index + data_size;
    return true;
  }

  auto fragment_length = data_size - available_capacity;
  auto fragment = data.substr( 0, fragment_length );
  output.push( fragment );

  this->_next_index = index + fragment_length;
  return true;
}
void Reassembler::try_to_write_all( Writer& output )
{
  if ( this->_buffer.empty() ) {
    return;
  }

  for ( auto it = this->_buffer.begin(); it != this->_buffer.end(); ) {
    auto start_index = it->first;
    auto end_index = ( start_index + it->second.size() ) - 1;
    if ( this->_next_index > end_index ) {
      it = this->_buffer.erase( it );
      continue;
    }

    if ( start_index > this->_next_index ) {
      ++it;
      continue;
    }

    auto subStr = it->second.substr( ( this->_next_index - start_index ), std::string::npos );
    bool result = this->write( this->_next_index, subStr, output );
    if ( result ) {
      it = this->_buffer.erase( it );
      this->_pre_index = start_index;
    } else {
      ++it;
    }
  }
}
void Reassembler::write_to_buffer( uint64_t index, std::string data, Writer& output )
{
  if ( this->_buffer.empty() && data.size() <= output.available_capacity() ) {
    this->_buffer[index] = data;
  }

  auto buffer_data_size = 0;
  for ( const auto& pair : this->_buffer ) {
    buffer_data_size += pair.second.size();
  }

  if ( ( data.size() + buffer_data_size ) > output.available_capacity() ) {
    return;
  }

  if ( this->_buffer.contains( index ) ) {
    this->_buffer[index] = this->_buffer[index].size() >= data.size() ? this->_buffer[index] : data;
  }

  for ( auto pair = this->_buffer.begin(); pair != this->_buffer.end(); ) {
    auto start_index = pair->first;
    auto end_index = start_index + pair->second.size() - 1;

    auto current_data_end_index = index + data.size() - 1;
    if ( start_index <= index && current_data_end_index <= end_index ) {
      ++pair;
      continue;
    }

    if ( index <= start_index && current_data_end_index >= end_index ) {
      this->_buffer[index] = data;
      pair = this->_buffer.erase( pair );
      continue;
    }
    this->_buffer[index] = data;
    ++pair;
  }
}
