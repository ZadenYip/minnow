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
  debug( "unimplemented recv_frame called" );
  (void)frame;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
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
