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

void TCPSender::segment_transmit( const TCPSenderMessage& msg, const TransmitFunction& transmit )
{
  transmit( msg );
  window_.next_seq_ = window_.next_seq_ + static_cast<uint32_t>( msg.sequence_length() );
  segment_control_create( msg );
}

TCPSenderMessage TCPSender::segment_get_just_contain_payload() const
{
  TCPSenderMessage msg = TCPSenderMessage();
  msg.seqno = window_.next_seq_;
  msg.payload = this->get_next_payload();
  return msg;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  switch ( kSenderState_ ) {
    case SenderState::CLOSED:
      push_closed_handler( transmit );
      break;
    case SenderState::SYN_SENT:
      break;
    case SenderState::ESTABLISHED:
      push_established_handler( transmit );
      break;
    case SenderState::ESTABLISHED_ZERO_WINDOW:
      push_established_zero_window_handler( transmit );
      break;
    case SenderState::FIN_SENT:
      break;
    default:
      throw std::runtime_error( "Invalid sender state" );
  }
}

void TCPSender::push_closed_handler( const TransmitFunction& transmit )
{
  window_.rcv_window_ -= 1;
  TCPSenderMessage msg = segment_get_just_contain_payload();
  msg.SYN = true;
  if ( segment_after_this_window_has_space( msg ) ) {
    msg.FIN = writer().is_closed();
  }
  segment_transmit( msg, transmit );
  timer_.restart();
  kSenderState_ = SenderState::SYN_SENT;
}

void TCPSender::push_established_handler( const TransmitFunction& transmit )
{
  if ( window_.available_send_space() == 0 ) {
    return;
  }

  if ( reader().has_error() ) {
    TCPSenderMessage msg = make_empty_message();
    transmit( msg );
    kSenderState_ = SenderState::CLOSED;
    return;
  }

  while ( true ) {
    TCPSenderMessage msg = segment_get_just_contain_payload();
    if ( segment_has_next_payload() ) {
      segment_transmit( msg, transmit );
      timer_.start_if_stopped();
    } else {
      // If sender prepare to close, check if the segment can fit in the window
      if ( writer().is_closed() && segment_after_this_window_has_space( msg ) ) {
        msg.FIN = true;
        kSenderState_ = SenderState::FIN_SENT;
      }
      // Don't send empty segments
      if ( msg.sequence_length() != 0 ) {
        segment_transmit( msg, transmit );
        timer_.start_if_stopped();
      }
      break;
    }
  }
}

void TCPSender::push_established_zero_window_handler( const TransmitFunction& transmit )
{
  if ( window_.transmitting_bytes_count() >= 1 ) {
    return;
  }

  window_.rcv_window_ += 1;
  TCPSenderMessage msg = segment_get_just_contain_payload();
  msg.FIN = msg.payload.empty() ? writer().is_closed() : false;
  segment_transmit( msg, transmit );
  timer_.start_if_stopped();
  window_.rcv_window_ -= 1;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg = TCPSenderMessage();
  msg.SYN = false;
  msg.FIN = false;
  msg.RST = reader().has_error();
  msg.seqno = window_.next_seq_;
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_.rcv_window_ = msg.window_size;

  if ( msg.ackno.has_value() ) {
    /**
     * @brief ignore old ack
     *
     */
    if ( msg.ackno.value().raw_value() <= window_.base_.raw_value() ) {
      return;
    }

    uint32_t max_ackno = window_.next_seq_.raw_value();
    if ( msg.ackno.value().raw_value() > max_ackno ) {
      return;
    }
  }

  if ( msg.RST ) {
    writer().set_error();
    return;
  }

  switch ( kSenderState_ ) {
    case SenderState::CLOSED:
      return;
    case SenderState::SYN_SENT:
      receive_syn_sent_handler( msg );
      break;
    case SenderState::ESTABLISHED_ZERO_WINDOW:
      receive_established_zero_window_handler( msg );
      break;
    case SenderState::ESTABLISHED:
    case SenderState::FIN_SENT:
      receive_established_handler( msg );
      break;
    default:
      throw std::runtime_error( "Invalid sender state" );
  }
}

void TCPSender::receive_syn_sent_handler( const TCPReceiverMessage& msg )
{
  if ( msg.ackno.value().raw_value() >= window_.base_.raw_value() + 1 ) {
    kSenderState_ = SenderState::ESTABLISHED;
    receive_established_handler( msg );
  }
}

void TCPSender::receive_established_handler( const TCPReceiverMessage& msg )
{
  if ( window_.rcv_window_ == 0 ) {
    kSenderState_ = SenderState::ESTABLISHED_ZERO_WINDOW;
  }

  if ( msg.RST ) {
    writer().set_error();
    return;
  }
  segment_update_state_for_ack( msg );
  timer_.reset();
  timer_.restart();
}

void TCPSender::receive_established_zero_window_handler( const TCPReceiverMessage& msg )
{
  if ( window_.rcv_window_ > 0 ) {
    kSenderState_ = SenderState::ESTABLISHED;
  }
  segment_update_state_for_ack( msg );
  timer_.reset();
  timer_.restart();
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( retransmit_msgs_.empty() ) {
    timer_.stop();
  } else {
    timer_.tick( ms_since_last_tick );
    if ( timer_.timeout() ) {
      TCPSenderMessage msg = get_timeout_msg();
      transmit( msg );
      if ( kSenderState_ == SenderState::ESTABLISHED_ZERO_WINDOW ) {
        timer_.reset();
      }
      timer_.restart();
    }
  }
}
}
