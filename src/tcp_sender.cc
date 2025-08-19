#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return window_.transmitting_bytes_count();
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  uint8_t count = 0;
  uint64_t RTO_ms = timer_.RTO_ms_;
  while ( initial_RTO_ms_ != RTO_ms ) {
    RTO_ms /= 2;
    count++;
  }
  return count;
}

bool TCPSender::segment_has_next_payload()
{
  TCPSenderMessage msg = segment_get_just_contain_payload();
  return pending_processed2segment_bytes() != msg.payload.size()
         && window_.available_send_space() != msg.payload.size();
}


void TCPSender::push( const TransmitFunction& transmit )
{
  debug( "unimplemented push() called" );
  (void)transmit;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  debug( "unimplemented make_empty_message() called" );
  return {};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  debug( "unimplemented receive() called" );
  (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
}
