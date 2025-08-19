#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <string_view>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , retransmit_msgs_( std::list<TCPSenderMessage>() )
    , window_( isn )
    , is_syn_sent_( false )
    , timer_( initial_RTO_ms )
    , kSenderState_( SenderState::CLOSED )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

  class Timer
  {
    friend class TCPSender;

  private:
    uint64_t initial_RTO_ms_;
    uint64_t passed_time_;
    uint64_t RTO_ms_;
    bool is_running_;

  public:
    Timer( uint64_t initial_RTO_ms )
      : initial_RTO_ms_( initial_RTO_ms ), passed_time_( 0 ), RTO_ms_( initial_RTO_ms ), is_running_( false ) {};
    void tick( uint64_t ms_since_last_tick );
    bool timeout();
    void reset();
    void restart();
    void start_if_stopped();
    void stop();
  };
  class TCPSenderWindow
  {
    friend class TCPSender;

  private:
    Wrap32 base_;
    Wrap32 next_seq_;
    uint16_t rcv_window_;

  public:
    TCPSenderWindow( Wrap32 isn ) : base_( isn ), next_seq_( isn ), rcv_window_( 1 ) {};
    uint16_t transmitting_bytes_count() const
    {
      return static_cast<uint16_t>( next_seq_.raw_value() - base_.raw_value() );
    }
    uint16_t available_send_space() const { return rcv_window_ - transmitting_bytes_count(); }
  };

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  std::list<TCPSenderMessage> retransmit_msgs_;
  TCPSenderWindow window_;
  bool is_syn_sent_;
  Timer timer_;
  enum class SenderState
  {
    CLOSED,
    ESTABLISHED,
    ESTABLISHED_ZERO_WINDOW,
    SYN_SENT,
    FIN_SENT,
  } kSenderState_;
  /**
   * @brief 还没有处理成segment待处理的字节数量
   * the bytes that have not yet been processed into segments
   * @return uint16_t
   */
  uint16_t pending_processed2segment_bytes() const;
  std::string_view get_next_payload() const;
  TCPSenderMessage get_retransmit_msg() const;
  TCPSenderMessage get_timeout_msg() const;
  bool segment_has_next_payload();
  bool segment_after_this_window_has_space( const TCPSenderMessage& current_msg ) const
  {
    return window_.available_send_space() > current_msg.payload.size();
  }
  void segment_transmit( const TCPSenderMessage& msg, const TransmitFunction& transmit );
  TCPSenderMessage segment_get_just_contain_payload() const;
  void segment_update_state_for_ack( const TCPReceiverMessage& msg );
  void segment_control_remove_for_ack( const TCPReceiverMessage& msg );
  void segment_control_create( const TCPSenderMessage& msg );
  void push_closed_handler( const TransmitFunction& transmit );
  void push_established_handler( const TransmitFunction& transmit );
  void push_established_zero_window_handler( const TransmitFunction& transmit );
  void receive_syn_sent_handler( const TCPReceiverMessage& msg );
  void receive_established_handler( const TCPReceiverMessage& msg );
  void receive_established_zero_window_handler( const TCPReceiverMessage& msg );
};
