#include "tcp_receiver.hh"
#include "debug.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    this->isn_ = message.seqno;
    this->rcv_absolute_ack_seq_ = 0;
  }
  uint64_t stream_seq = message.seqno.unwrap( isn_, rcv_absolute_ack_seq_ ) - ( message.SYN ? 0 : 1 );
  uint64_t old_pushed_bytes = reassembler_.writer().bytes_pushed();
  reassembler_.insert( stream_seq, message.payload, message.FIN );
  uint64_t new_pushed_bytes = reassembler_.writer().bytes_pushed();
  rcv_absolute_ack_seq_ += ( new_pushed_bytes - old_pushed_bytes );
  rcv_absolute_ack_seq_ += ( message.SYN + reassembler_.writer().is_closed() );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage receive_msg_;
  if ( rcv_absolute_ack_seq_ != 0 ) {
    receive_msg_.ackno = isn_.wrap( rcv_absolute_ack_seq_, isn_ );
  }
  uint64_t capacity = reassembler_.writer().available_capacity();
  receive_msg_.window_size = capacity >= UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>( capacity );
  receive_msg_.RST = reassembler_.writer().has_error();
  return { receive_msg_ };
}
