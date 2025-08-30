#include "router.hh"
#include "address.hh"
#include "debug.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  debug( "unimplemented add_route() called" );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{

void Router::Trie::add_route_entry( const uint32_t& route_prefix,
                            const uint8_t& prefix_length,
                            const size_t& interface_num,
                            const std::optional<Address>& next_hop )
{
  TrieNode* current = root.get();
  for ( uint8_t i = 0; i < prefix_length; ++i ) {
    bool bit = get_bit( route_prefix, i );
    if ( current->children_[bit] == nullptr ) {
      current->children_[bit] = std::make_unique<TrieNode>();
    }
    current = current->children_[bit].get();
  }
  current->interface_num_ = interface_num;
  current->next_hop_ = next_hop;
}

std::pair<std::optional<size_t>, std::optional<Address>> Router::Trie::lookup( const uint32_t& target )
{
  TrieNode* current = root.get();
  TrieNode* best_match_node = root.get();

  for ( uint8_t i = 0; i < 32; ++i ) {
    if ( current == nullptr ) {
      break;
    }

    bool bit = get_bit( target, i );
    current = current->children_[bit].get();

    if ( current != nullptr && current->interface_num_.has_value() ) {
      best_match_node = current;
}
  }

  return { best_match_node->interface_num_, best_match_node->next_hop_ };
}