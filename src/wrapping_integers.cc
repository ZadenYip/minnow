#include "wrapping_integers.hh"
#include "debug.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point + n };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t low_bits = static_cast<uint64_t>( raw_value_ - zero_point.raw_value_ );
  uint64_t high_bits_upper = ( checkpoint + ( 1UL << 31 ) ) & ( UINT64_MAX - UINT32_MAX );
  uint64_t high_bits_lower = ( checkpoint - ( 1UL << 31 ) ) & ( UINT64_MAX - UINT32_MAX );
  uint64_t upper = high_bits_upper | low_bits;
  uint64_t lower = high_bits_lower | low_bits;
  uint64_t distance_upper = max( checkpoint, upper ) - min( checkpoint, upper );
  uint64_t distance_lower = max( checkpoint, lower ) - min( checkpoint, lower );
  return distance_upper < distance_lower ? upper : lower;
}
