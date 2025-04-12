#pragma once

#include "byte_stream.hh"
#include <cstdint>
#include <map>
#include <string>

struct ReassemblePacket
{
  std::string data;
  bool is_last_substring;
};
class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );
  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_;
  uint64_t reassembled_first_index_ { 0 };    // The index of the first byte that has not yet been reassembled
  std::map<int, ReassemblePacket> buffer_ {}; // Buffer for reassembling packets

  void handle_match_first_index( const std::string& data, bool is_last_substring );
  void buffer_in_assembler( const uint64_t first_index,
                            const std::string& data,
                            bool is_last_substring ); // remain one byte for triggring push bytes into stream
  void pushed_new_bytes( const std::string& data, bool is_last_substring );
  bool is_capacity_exhausted() const { return output_.writer().available_capacity() == 0; }

  bool trim_to_fit( uint64_t& first_index, std::string& data, bool& is_lasting_substring ) const;
  bool is_inserted_out_of_range( const uint64_t index, const std::string& data ) const;
  bool is_match_first_index( const uint64_t first_index ) const { return reassembled_first_index_ == first_index; }
  bool is_already_buffered( const uint64_t first_index, const std::string& data ) const;
  void merge_overlap_from( const uint64_t first_index );
  void buffer_insertion( const uint64_t first_index, const std::string& data, bool is_last_substring );
  void push_buffered_data( const uint64_t index );
  int find_the_contain_packet( uint64_t first_index ) const;
  uint64_t calculate_max_index( uint64_t index, const std::string& data ) const;
};
