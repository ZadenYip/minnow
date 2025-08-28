#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , current_time_stamp_( 0 )
  , dgram_pending_arp_()
  , arp_table_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip_int = next_hop.ipv4_numeric();

  // arp table contains mapping for next hop
  if ( arp_table_.contains( next_hop_ip_int ) ) {
    send_dgram( dgram, next_hop );
    return;
  }

  if ( dgram_pending_arp_.contains( next_hop_ip_int ) ) {
    DatagramEntry& entry = dgram_pending_arp_.at( next_hop_ip_int );
    // new datagram
    entry.list.push_back( dgram );

    size_t last_query_time = entry.timestamp_;
    if ( current_time_stamp_ - last_query_time > 5000 ) {
      entry.list.pop_front();
      // send arp request
      entry.timestamp_ = current_time_stamp_;
      send_arp_request( next_hop_ip_int );
    }
  } else {
    DatagramEntry entry { current_time_stamp_, { dgram }, next_hop };
    dgram_pending_arp_.insert( { next_hop_ip_int, entry } );
    send_arp_request( next_hop_ip_int );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{

  uint16_t payload_type = frame.header.type;
  switch ( payload_type ) {
    case EthernetHeader::TYPE_IPv4:
      recv_frame_ipv4( frame );
      break;
    case EthernetHeader::TYPE_ARP:
      recv_frame_arp( frame );
      break;
  }
}

void NetworkInterface::recv_frame_ipv4( const EthernetFrame& frame )
{
  const EthernetAddress& dst = frame.header.dst;
  if ( dst != ethernet_address_ && dst != ETHERNET_BROADCAST ) {
    return;
  }
  Parser parser = Parser( frame.payload );
  InternetDatagram dgram = InternetDatagram();
  dgram.parse( parser );
  if ( parser.has_error() ) {
    return;
  }
  datagrams_received_.push( dgram );
}

void NetworkInterface::recv_frame_arp( const EthernetFrame& frame )
{
  Parser parser = Parser( frame.payload );
  ARPMessage arp_msg = ARPMessage();
  arp_msg.parse( parser );
  if ( parser.has_error() ) {
    return;
  }

  // If the ARP request is for us, we need to send an ARP reply
  // 如果 ARP request 是找我们的，则回复ARP reply
  if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
    send_arp_reply( arp_msg.sender_ethernet_address, arp_msg.sender_ip_address );
  }

  // 更新 ARP 表
  ARPEntry entry { current_time_stamp_, arp_msg.sender_ethernet_address, arp_msg.sender_ip_address };
  arp_table_.insert_or_assign( arp_msg.sender_ip_address, entry );

  // If the ARP reply is for us, send the queued datagram
  // 如果 ARP reply 是发给我们的， 则发送队列中的 datagram
  if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY && arp_msg.target_ethernet_address == ethernet_address_
       && dgram_pending_arp_.contains( arp_msg.sender_ip_address ) ) {
    DatagramEntry dgram_entry = dgram_pending_arp_.at( arp_msg.sender_ip_address );
    for ( const InternetDatagram& dgram : dgram_entry.list ) {
      send_dgram( dgram, dgram_entry.next_hop_ );
    }
    dgram_pending_arp_.erase( arp_msg.sender_ip_address );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time_stamp_ += ms_since_last_tick;
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ) {
    if ( current_time_stamp_ - it->second.last_timestamp_ > 30000 ) {
      it = arp_table_.erase( it );
    } else {
      ++it;
    }
  }
}

void NetworkInterface::send_frame( Serializer& payload_srlz,
                                   const uint16_t payload_type,
                                   const EthernetAddress& next_hop ) const
{
  EthernetFrame frame = EthernetFrame();
  frame.header.src = ethernet_address_;
  frame.header.dst = next_hop;
  frame.header.type = payload_type;
  frame.payload = payload_srlz.finish();
  transmit( frame );
}

void NetworkInterface::send_dgram( const InternetDatagram& dgram, const Address& next_hop ) const
{
  const EthernetAddress& dst = arp_table_.at( next_hop.ipv4_numeric() ).ethernet_address_;
  Serializer dgram_srlz = Serializer();
  dgram.serialize( dgram_srlz );
  send_frame( dgram_srlz, EthernetHeader::TYPE_IPv4, dst );
}

void NetworkInterface::send_arp_request( const uint32_t& dst_ip ) const
{
  ARPMessage arp_msg = ARPMessage();
  arp_msg.sender_ethernet_address = ethernet_address_;
  arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
  constexpr EthernetAddress ETHERNET_ZEROS = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  arp_msg.target_ethernet_address = ETHERNET_ZEROS;
  arp_msg.target_ip_address = dst_ip;
  arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
  Serializer arp_msg_srlz = Serializer();
  arp_msg.serialize( arp_msg_srlz );

  send_frame( arp_msg_srlz, EthernetHeader::TYPE_ARP, ETHERNET_BROADCAST );
}

void NetworkInterface::send_arp_reply( const EthernetAddress& dst_ether_address, uint32_t dst_ip_address ) const
{
  ARPMessage arp_msg = ARPMessage();
  arp_msg.sender_ethernet_address = ethernet_address_;
  arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
  arp_msg.target_ethernet_address = dst_ether_address;
  arp_msg.target_ip_address = dst_ip_address;
  arp_msg.opcode = ARPMessage::OPCODE_REPLY;
  Serializer arp_msg_srlz = Serializer();
  arp_msg.serialize( arp_msg_srlz );

  send_frame( arp_msg_srlz, EthernetHeader::TYPE_ARP, arp_msg.target_ethernet_address );
}