#include "reassembler.hh"
#include <iostream>
#include <list>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  std::cout << "first_index: " << first_index << ", end_index: " << first_index + data.size() - 1 << '\n';

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

  std::cout << "init next index: " << this->_next_index << '\n';
  auto first_index_end = first_index + data.size() - 1;
  if ( this->_next_index == first_index ) {
    // write
    this->write( first_index, data, output );

  } else if ( this->_next_index < first_index ) {
    // cache
    this->write_to_buffer( first_index, data, output );
  } else if ( this->_next_index > first_index ) {

    if ( this->_next_index > first_index_end ) {
      // drop
    } else if (this->_next_index<= first_index_end) {
      // write party
      auto subStr = data.substr( this->_next_index - first_index, string::npos );
      this->write( this->_next_index, subStr, output );
    }
  }

  for ( const auto& item : this->_buffer ) {
    std::cout << "pre buffer start: " << item.first << " end:" << item.second.size() + item.first - 1 << '\n';
  }
  std::cout << "pre next index: " << this->_next_index << '\n';
  try_to_write_all( output );
  std::cout << "after next index: " << this->_next_index << '\n';

  for ( const auto& item : this->_buffer ) {
    std::cout << "after buffer start: " << item.first << " end:" << item.second.size() + item.first - 1 << '\n';
  }

  (void)first_index;
  (void)data;
  (void)is_last_substring;
  (void)output;
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t buffer_size {};
  for ( const auto& item : this->_buffer ) {
    buffer_size += item.second.size();
  }
  return buffer_size;
}
void Reassembler::write( uint64_t index, std::string data, Writer& output )
{
  auto available_capacity = output.available_capacity();
  if ( available_capacity == 0 ) {
    return;
  }

  auto data_size = data.size();
  if ( data_size <= available_capacity ) {

    output.push( data );
    this->_next_index = index + data_size;

  } else {

    auto fragment = data.substr( 0, available_capacity );

    output.push( fragment );
    this->_next_index = index + fragment.size();
  }
}
void Reassembler::try_to_write_all( Writer& output )
{
  if ( this->_buffer.empty() ) {
    return;
  }

  std::list<uint64_t> keys_to_remove {};
  for ( const auto& item : this->_buffer ) {
    auto item_start = item.first;
    auto item_end = ( item_start + item.second.size() ) - 1;

    if ( this->_next_index < item_start ) {
      continue;
    }

    if ( this->_next_index > item_end ) {
      keys_to_remove.push_back( item_start );
      continue;
    }

    auto sub_str = item.second.substr( ( this->_next_index - item_start ), std::string::npos );
    this->write( this->_next_index, sub_str, output );
    keys_to_remove.push_back( item_start );
  }

  for ( auto key : keys_to_remove ) {
    this->_buffer.erase( key );
  }
}
void Reassembler::write_to_buffer( uint64_t index, std::string data, Writer& output )
{
  if ( this->_buffer.empty() && data.size() <= output.available_capacity() ) {
    this->_buffer[index] = data;
    return;
  }

  auto current_size = this->bytes_pending();
  if ( current_size + data.size() > output.available_capacity() ) {
    return;
  }

  // 找出已存在的元素
  auto exist_item = this->_buffer.find( index );
  if ( exist_item != this->_buffer.end() ) {
    auto item_end = index + exist_item->second.size() - 1;
    // 已存在元素，且小于当前值
    if ( item_end >= ( index + data.size() - 1 ) ) {
      return;
    }
    this->write_to_buffer( item_end + 1, data.substr( item_end + 1 - index, string::npos ), output );
    return;
  }

  auto new_data_start = index;
  auto new_data_end = index + data.size() - 1;

  std::map<uint64_t, std::string> new_buffer {};
  bool is_inserted = false;
  for ( const auto& item : this->_buffer ) {
    auto item_start = item.first;
    auto item_end = item_start + item.second.size() - 1;

    if ( new_data_start > new_data_end ) {
      break;
    }

    //<> []
    if ( new_data_end < item_start ) {
      new_buffer[new_data_start] = split_string( index, new_data_start, item_start - 1, data );
      is_inserted = true;
      break;
    }

    //[<>] [<]> []<>
    if ( new_data_start >= item_start && item_end >= new_data_start ) {
      if ( new_data_end <= item_end ) {
        is_inserted = true;
        break;
      }
      new_data_start = item_end + 1;
      continue;
    }

    // <[>] <[]>
    if ( new_data_start < item_start ) {
      if ( new_data_end > item_end ) {
        new_buffer[new_data_start] = split_string( index, new_data_start, item_start - 1, data );
        new_data_start = item_end + 1;
        continue;
      } else {
        is_inserted = true;
        break;
      }
    }
  }

  if ( !is_inserted && new_data_start <= new_data_end ) {
    new_buffer[new_data_start] = data.substr( new_data_start - index, string::npos );
  }

  if ( new_buffer.empty() ) {
    return;
  }

  this->_buffer.insert( new_buffer.begin(), new_buffer.end() );
}

std::string Reassembler::split_string( uint64_t index_start, uint64_t start, uint64_t end, std::string data )
{
  if ( start > end ) {
    return {};
  }

  return end > ( index_start + data.size() - 1 ) ? data.substr( start - index_start, string::npos )
                                                 : data.substr( start - index_start, end - start + 1 );
}
