#include <catch.hpp>

#include <mockturtle/networks/xag.hpp>
#include <mockturtle/views/cost_view.hpp>

using namespace mockturtle;

template<class Ntk>
struct and_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    (void)fanin_costs;
    if ( ntk.is_and( n ) )
      tot_cost += 1; /* add dissipate cost */
    return 0;        /* return accumulate cost */
  }
};

template<class Ntk>
struct gate_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    (void)fanin_costs;
    if ( ntk.is_pi( n ) == false )
      tot_cost += 1; /* add dissipate cost */
    return 0;        /* return accumulate cost */
  }
};

template<class Ntk>
struct supp_cost
{
public:
  using cost_t = kitty::partial_truth_table;
  cost_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<cost_t> const& fanin_costs = {} ) const
  {
    cost_t _c( ntk.num_pis() );
    if ( ntk.is_pi( n ) )
    {
      set_bit( _c, ntk.pi_index( n ) );
    }
    std::for_each( std::begin( fanin_costs ), std::end( fanin_costs ), [&_c]( cost_t c ) { _c |= c; } );
    if ( !ntk.is_pi( n ) )
      tot_cost += kitty::count_ones( _c );
    return _c; /* return accumulate cost */
  }
};

template<class Ntk>
struct level_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 0 : *std::max_element( std::begin( fanin_costs ), std::end( fanin_costs ) ) + 1;
    tot_cost = std::max( tot_cost, _cost ); /* update dissipate cost */
    return _cost;                           /* return accumulate cost */
  }
};

template<class Ntk>
struct adp_cost
{
public:
  using cost_t = uint32_t;
  uint32_t operator()( Ntk const& ntk, node<Ntk> const& n, uint32_t& tot_cost, std::vector<uint32_t> const& fanin_costs = {} ) const
  {
    uint32_t _cost = ntk.is_pi( n ) ? 1 : *std::max_element( std::begin( fanin_costs ), std::end( fanin_costs ) ) + 1;
    if ( !ntk.is_pi( n ) )
      tot_cost += _cost; /* sum of level over each node */
    return _cost;        /* return accumulate cost */
  }
};

template<typename Ntk>
void test_cost_view()
{
  CHECK( is_network_type_v<Ntk> );
  CHECK( !has_cost_v<Ntk> );

  using cost_ntk = cost_view<Ntk, and_cost<Ntk>>;

  CHECK( is_network_type_v<cost_ntk> );
  CHECK( has_cost_v<cost_ntk> );

  using cost_cost_ntk = cost_view<cost_ntk, and_cost<cost_ntk>>;

  CHECK( is_network_type_v<cost_cost_ntk> );
  CHECK( has_cost_v<cost_cost_ntk> );
};

TEST_CASE( "create different cost views", "[cost_view]" )
{
  test_cost_view<xag_network>();
}

TEST_CASE( "compute cost cost for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_and( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_and( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, level_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 3 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 3 );

}

TEST_CASE( "compute AND costs for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_xor( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, and_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 2 );

}

TEST_CASE( "compute gate costs for xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( a, f1 );
  const auto f3 = xag.create_xor( b, f1 );
  const auto f4 = xag.create_and( f2, f3 );
  xag.create_po( f4 );

  cost_view cost_xag( xag, gate_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 4 );
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 1 );
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 4 );

}

TEST_CASE( "compute support numer of xag network", "[cost_view]" )
{
  xag_network xag;
  const auto a = xag.create_pi();
  const auto b = xag.create_pi();
  const auto c = xag.create_pi();
  const auto d = xag.create_pi();
  const auto f1 = xag.create_xor( a, b );
  const auto f2 = xag.create_and( b, c );
  const auto f3 = xag.create_xor( f1, f2 );
  const auto f4 = xag.create_and( f3, d );
  xag.create_po( f4 );

  cost_view cost_xag( xag, supp_cost<xag_network>() );
  CHECK( cost_xag.get_cost() == 11 ); // 4 + 3 + 2 + 2
  CHECK( cost_xag.get_cost( xag.get_node( a  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( b  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( c  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( d  ) ) == 0 );
  CHECK( cost_xag.get_cost( xag.get_node( f1 ) ) == 2 ); // 2
  CHECK( cost_xag.get_cost( xag.get_node( f2 ) ) == 2 );
  CHECK( cost_xag.get_cost( xag.get_node( f3 ) ) == 7 ); // 3 + 2 + 2
  CHECK( cost_xag.get_cost( xag.get_node( f4 ) ) == 11 ); // 4 + 3 + 2 + 2

}
