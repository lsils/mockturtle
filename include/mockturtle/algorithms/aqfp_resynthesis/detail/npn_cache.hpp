#pragma once

#include <kitty/kitty.hpp>

#include <tuple>
#include <vector>

namespace mockturtle
{

/*! \brief Cache for mapping an N-input truthtable to the corresponding NPN class and the associated NPN transformation. */
template<uint32_t N = 4u>
class npn_cache
{
  using npn_info = std::tuple<uint64_t, uint32_t, std::vector<uint8_t>>;

public:
  npn_cache() : arr( 1ul << ( 1ul << N ) ), has( 1ul << ( 1ul << N ), false )
  {
    static_assert( N <= 4u, "N is too high! Try increasing this limit if memory is not a problem." );
  }

  npn_info operator()( uint64_t tt )
  {
    if ( has[tt] )
    {
      return arr[tt];
    }

    kitty::dynamic_truth_table dtt( N );
    dtt._bits[0] = tt;

    auto tmp = kitty::exact_npn_canonization( dtt );

    has[tt] = true;
    return ( arr[tt] = { std::get<0>( tmp )._bits[0] & 0xffff, std::get<1>( tmp ), std::get<2>( tmp ) } );
  }

private:
  std::vector<npn_info> arr;
  std::vector<bool> has;
};

} // namespace mockturtle
