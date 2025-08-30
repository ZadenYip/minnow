#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

  class Trie
  {
    struct TrieNode
    {
      std::optional<size_t> interface_num_ = std::nullopt;
      std::optional<Address> next_hop_ = std::nullopt;
      std::unique_ptr<TrieNode> children_[2] = { nullptr, nullptr };
    };

  public:
    Trie() : root( std::make_unique<TrieNode>() ) {}
    void add_route_entry( const uint32_t& route_prefix,
                          const uint8_t& prefix_length,
                          const size_t& interface_num,
                          const std::optional<Address>& next_hop );

    std::pair<std::optional<size_t>, std::optional<Address>> lookup( const uint32_t& target );

  private:
    std::unique_ptr<TrieNode> root;
    bool get_bit( const uint32_t& route_prefix, const uint8_t& bit_index ) const
    {
      return ( route_prefix >> ( 31 - bit_index ) ) & 0x01;
    }
  };

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
  Trie routing_table_ {};
  void handle_incoming_datagram( InternetDatagram& dgram );
  void handle_interface_rcv_dgram( NetworkInterface& interface );
};
