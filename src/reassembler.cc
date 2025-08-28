#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <asm-generic/errno.h>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string>
#include <sys/types.h>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_capacity_exhausted() ) {
    return;
  }

  if ( is_inserted_out_of_range( first_index, data ) ) {
    return;
  }

  if ( data.empty() && !is_last_substring ) {
    return;
  }

  if ( is_already_buffered( first_index, data ) ) {
    return;
  }

  trim_to_fit( first_index, data, is_last_substring );

  /* Push bytes into the stream */
  if ( is_match_first_index( first_index ) ) {
    handle_match_first_index( data, is_last_substring );
    return;
  }

  buffer_in_assembler( first_index, data, is_last_substring );
}

bool Reassembler::is_inserted_out_of_range( const uint64_t index, const string& data ) const
{
  const uint64_t assembled_max_index = reassembled_first_index_ + writer().available_capacity() - 1;
  const uint64_t max_index = calculate_max_index( index, data );
  /* [...data...max_index] [--range--] [min_index...data...] */
  return max_index < reassembled_first_index_ || index > assembled_max_index;
}

inline uint64_t Reassembler::calculate_max_index( uint64_t index, const std::string& data ) const
{
  return index + ( data.size() == 0 ? 0 : data.size() - 1 );
}

bool Reassembler::is_already_buffered( const uint64_t first_index, const std::string& data ) const
{
  const uint64_t max_index = calculate_max_index( first_index, data );
  const int packet_index = find_the_contain_packet( first_index );
  if ( packet_index == -1 ) {
    return false;
  }

  auto it = buffer_.find( packet_index );
  const uint64_t max_packet_index = calculate_max_index( packet_index, it->second.data );
  return static_cast<uint64_t>( it->first ) <= first_index && max_index <= max_packet_index;
}

void Reassembler::buffer_insertion( const uint64_t first_index, const string& data, bool is_last_substring )
{
  const int packet_index = find_the_contain_packet( first_index );
  if ( packet_index < 0 ) {
    buffer_.insert( { first_index, ReassemblePacket { data, is_last_substring } } );
    merge_overlap_from( first_index );
    return;
  }

  /* Discard old_data if it's fully contained in new_data with the same start index */
  auto it = buffer_.find( packet_index );
  ReassemblePacket& packet = it->second;
  if ( static_cast<uint64_t>( packet_index ) == first_index ) {
    if ( packet.data.size() < data.size() ) {
      buffer_.erase( it );
    }
  }

  buffer_.insert( { first_index, ReassemblePacket { data, is_last_substring } } );
  merge_overlap_from( std::min<uint64_t>( packet_index, first_index ) );
}

/**
 * @brief find
 *
 * @param byte_index - the index of the byte to be found
 * @return int - the index of containing the byte_index of the packet
 */
int Reassembler::find_the_contain_packet( uint64_t byte_index ) const
{

  auto it = buffer_.find( byte_index );
  if ( it != buffer_.end() ) {
    return it->first;
  }

  it = buffer_.lower_bound( byte_index );
  if ( it == buffer_.begin() ) {
    return -1;
  }
  --it;
  const uint64_t& prev_index = it->first;
  const ReassemblePacket& packet = it->second;
  const uint64_t prev_max_index = calculate_max_index( prev_index, packet.data );
  if ( prev_max_index >= byte_index ) {
    return prev_index;
  }

  return -1;
}

void Reassembler::pushed_new_bytes( const string& data, bool is_last_substring )
{
  /* Buffer bytes */
  const ReassemblePacket packet = ReassemblePacket { data, is_last_substring };
  buffer_insertion( reassembled_first_index_, data, is_last_substring );
  push_buffered_data( reassembled_first_index_ );
}

void Reassembler::buffer_in_assembler( uint64_t first_index, const string& data, bool is_last_substring )
{
  buffer_insertion( first_index, data, is_last_substring );
}

void Reassembler::merge_overlap_from( const uint64_t first_index )
{
  auto it = buffer_.lower_bound( first_index );
  while ( it != buffer_.end() ) {
    auto next_it = std::next( it );
    if ( next_it == buffer_.end() ) {
      return;
    }
    const uint64_t& index_1 = it->first;
    const uint64_t& index_2 = next_it->first;
    ReassemblePacket& packet_1 = it->second;
    const ReassemblePacket& packet_2 = next_it->second;
    const uint64_t max_index_1 = calculate_max_index( index_1, packet_1.data );
    if ( max_index_1 >= index_2 ) {
      uint64_t new_start_index = max_index_1 - index_2 + 1;
      if ( new_start_index < packet_2.data.size() ) {
        packet_1.data = packet_1.data + packet_2.data.substr( new_start_index );
        packet_1.is_last_substring = packet_2.is_last_substring;
      }
      buffer_.erase( next_it );
    } else {
      return;
    }
  }
}

void Reassembler::push_buffered_data( const uint64_t start_index )
{
  auto it = buffer_.find( start_index );
  if ( it == buffer_.end() ) {
    throw std::out_of_range( "Index not found in buffer" );
  }
  while ( it != buffer_.end() ) {
    const uint64_t index = it->first;
    const ReassemblePacket& packet = it->second;
    output_.writer().push( packet.data );
    reassembled_first_index_ += packet.data.size();

    if ( packet.is_last_substring ) {
      output_.writer().close();
    }

    auto next_it = std::next( it );
    const uint64_t next_index = next_it->first;
    if ( next_it != buffer_.end() && ( index + packet.data.size() ) == next_index ) {
      buffer_.erase( it );
      it = next_it;
    } else {
      buffer_.erase( it );
      it = buffer_.end();
    }
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( const auto& [index, packet] : buffer_ ) {
    count += packet.data.size();
  }
  return count;
}

/**
 * @return is trimed the end of data
 */
bool Reassembler::trim_to_fit( uint64_t& first_index, std::string& data, bool& is_last_substring ) const
{
  uint64_t fit_max_index = reassembled_first_index_ + output_.writer().available_capacity() - 1;
  uint64_t max_index = calculate_max_index( first_index, data );
  bool is_suitable = reassembled_first_index_ <= first_index && max_index <= fit_max_index;
  if ( !is_suitable ) {
    uint64_t trimed_first_index = std::max<uint64_t>( first_index, reassembled_first_index_ );
    uint64_t trimed_max_index = std::min<uint64_t>( max_index, fit_max_index );
    data = data.substr( trimed_first_index - first_index, trimed_max_index - trimed_first_index + 1 );
    first_index = trimed_first_index;
  }
  bool is_trimed_end = max_index > fit_max_index;
  is_last_substring = is_last_substring && !is_trimed_end;
  return max_index > fit_max_index;
}

void Reassembler::handle_match_first_index( const string& data, bool is_last_substring )
{
  buffer_insertion( reassembled_first_index_, data, is_last_substring );
  push_buffered_data( reassembled_first_index_ );
}