#include <catch.hpp>

#include <mockturtle/networks/xag.hpp>
#include <mockturtle/utils/node_map.hpp>
#include <fmt/format.h>

namespace mockturtle
{

template<typename T>
inline uint16_t ptr_cast( T *ptr )
{
  return uint64_t( ptr ) & 0xffff;
}

template<typename Ntk>
class test_view : public Ntk
{
public:
  explicit test_view()
    : Ntk()
  {
    fmt::print( "[i] construct test_view 0x...{:x} without network\n",
                ptr_cast( this ) );
  }

  explicit test_view( Ntk const& ntk )
    : Ntk( ntk )
  {
    fmt::print( "[i] construct test_view 0x...{:x} from network 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &ntk ) );
  }

  explicit test_view( test_view<Ntk> const& other )
    : Ntk( other )
  {
    fmt::print( "[i] copy-construct test_view 0x...{:x} from 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &other ) );
  }

  ~test_view()
  {
    fmt::print( "[i] destory test_view 0x...{:x}\n", ptr_cast( this ) );
  }
};

template<typename Ntk>
class test_view2 : public Ntk
{
public:
  explicit test_view2()
    : Ntk()
    , map_( *this )
  {
    fmt::print( "[i] construct test_view2 0x...{:x} without network\n",
                ptr_cast( this ) );
  }

  explicit test_view2( Ntk const& ntk )
    : Ntk( ntk )
    , map_( ntk )
  {
    fmt::print( "[i] construct test_view2 0x...{:x} from network 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &ntk ) );
  }

  explicit test_view2( test_view2<Ntk> const& other )
    : Ntk( other )
    , map_( other.map_ )
  {
    fmt::print( "[i] copy-construct test_view2 0x...{:x} from 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &other ) );
  }

  ~test_view2()
  {
    fmt::print( "[i] destory test_view2 0x...{:x}\n", ptr_cast( this ) );
  }

public:
  node_map<uint32_t, Ntk> map_;
};

template<typename Ntk>
class test_view3 : public Ntk
{
public:
  explicit test_view3()
    : Ntk()
    , map_( *this )
  {
    fmt::print( "[i] construct test_view3 0x...{:x} without network\n",
                ptr_cast( this ) );
    auto fn = [&]( node<Ntk> const& n ){ on_add( n ); };
    event = this->events().register_add_event( fn );
  }

  explicit test_view3( Ntk const& ntk )
    : Ntk( ntk )
    , map_( ntk )
  {
    fmt::print( "[i] construct test_view3 0x...{:x} from network 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &ntk ) );
    auto fn = [&]( node<Ntk> const& n ){ on_add( n ); };
    event = ntk.events().register_add_event( fn );
  }

  explicit test_view3( test_view3<Ntk> const& other )
    : Ntk( other )
    , map_( other.map_ )
  {
    fmt::print( "[i] copy-construct test_view3 0x...{:x} from 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &other ) );
    auto fn = [&]( node<Ntk> const& n ){ on_add( n ); };
    event = this->events().register_add_event( fn );
  }

  test_view3<Ntk>& operator=( test_view3<Ntk> const& other )
  {
    fmt::print( "[i] copy-assign to test_view3 0x...{:x} from test_view3 0x...{:x}\n",
                ptr_cast( this ), ptr_cast( &other ) );

    /* delete the event of this network */
    this->events().release_add_event( event );

    /* update the base class */
    this->_storage = other._storage;
    this->_events = other._events;

    map_ = other.map_;

    /* register new event in the other network */
    auto fn = [&]( node<Ntk> const& n ){ on_add( n ); };
    event = this->events().register_add_event( fn );

    return *this;
  }

  ~test_view3()
  {
    fmt::print( "[i] destory test_view3 0x...{:x}\n", ptr_cast( this ) );
    this->events().release_add_event( event );
  }

  void on_add( node<Ntk> const& n )
  {
    fmt::print( "[i] test_view3 0x...{:x}: invoke on_add {}\n",
                ptr_cast( this ), n );
    map_.resize();
  }

public:
  node_map<uint32_t, Ntk> map_;
  std::shared_ptr<typename network_events<Ntk>::add_event_type> event;
};

} /* mockturtle */

using namespace mockturtle;

TEST_CASE( "test_view copy_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view<xag_network>{xag};
  test_view<xag_network> txag{*tmp};
  delete tmp;

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );
}

TEST_CASE( "test_view move_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view<xag_network>{xag};
  test_view<xag_network> txag{std::move( *tmp )};
  delete tmp;

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );
}

TEST_CASE( "test_view2 copy_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view2<xag_network>{xag};
  test_view2<xag_network> txag{*tmp};
  delete tmp;

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );
}

TEST_CASE( "test_view2 move_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view2<xag_network>{xag};
  test_view2<xag_network> txag{std::move( *tmp )};
  delete tmp;

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );
}

TEST_CASE( "test_view3 copy_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view3<xag_network>{xag};
  test_view3<xag_network> txag{*tmp};
  delete tmp;

  CHECK( txag.map_.size() == xag.size() );

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );

  CHECK( txag.map_.size() == xag.size() );
}

TEST_CASE( "test_view3 move_ctor", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  auto tmp = new test_view3<xag_network>{xag};
  test_view3<xag_network> txag{std::move( *tmp )};
  delete tmp;

  CHECK( txag.map_.size() == xag.size() );

  auto const a  = txag.create_pi();
  auto const b  = txag.create_pi();
  auto const c  = txag.create_pi();
  auto const t0 = txag.create_xor( a, b );
  auto const t1 = txag.create_xor( b, c );
  auto const t2 = txag.create_and( t0, t1 );
  auto const t3 = txag.create_xor( b, t2 );
  txag.create_po( t3 );

  CHECK( txag.map_.size() == xag.size() );
}

TEST_CASE( "test_view3 auto-update #1", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  test_view3<xag_network> txag_one{xag};
  test_view3<xag_network> txag_two{xag};

  xag.create_pi();
  xag.create_pi();
  xag.create_pi();

  /* BUG: on_add is not triggered for primary inputs */
  // CHECK( txag_one.map_.size() == xag.size() );
  // CHECK( txag_two.map_.size() == xag.size() );
}

TEST_CASE( "test_view3 auto-update #2", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  test_view3<xag_network> txag_one{xag};
  test_view3<xag_network> txag_two{xag};

  auto const a  = xag.create_pi();
  auto const b  = xag.create_pi();
  auto const c  = xag.create_pi();
  auto const t0 = xag.create_xor( a, b );
  auto const t1 = xag.create_xor( b, c );
  auto const t2 = xag.create_and( t0, t1 );
  auto const t3 = xag.create_xor( b, t2 );
  xag.create_po( t3 );

  CHECK( txag_one.map_.size() == xag.size() );
  CHECK( txag_two.map_.size() == xag.size() );
}

TEST_CASE( "test_view3 assignment", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};

  {
    test_view3<xag_network> txag_one{xag};
    auto const a  = xag.create_pi();
    auto const b  = xag.create_pi();
    auto const c  = xag.create_pi();
    auto const t0 = xag.create_xor( a, b );
    auto const t1 = xag.create_xor( b, c );
    auto const t2 = xag.create_and( t0, t1 );
    auto const t3 = xag.create_xor( b, t2 );
    xag.create_po( t3 );

    test_view3<xag_network> txag_two{xag};
    txag_two = txag_one;

    CHECK( xag.size() == 8u );
    CHECK( txag_one.map_.size() == xag.size() );
    CHECK( txag_two.map_.size() == xag.size() );
  }
  CHECK( xag.events().on_add.size() == 0 );
}

TEST_CASE( "test_view3 copy assignment", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};
  {
    test_view3<xag_network> txag{}; // the underlying ntk is a different ntk (and the events are register there)

    auto tmp = new test_view3<xag_network>{xag};
    txag = *tmp; /* copy assignment */
    delete tmp;

    CHECK( xag.events().on_add.size() == 1 );

    auto const a  = xag.create_pi();
    auto const b  = xag.create_pi();
    auto const c  = xag.create_pi();
    auto const t0 = xag.create_xor( a, b );
    auto const t1 = xag.create_xor( b, c );
    auto const t2 = xag.create_and( t0, t1 );
    auto const t3 = xag.create_xor( b, t2 );
    xag.create_po( t3 );

    CHECK( xag.size() == 8u );
    CHECK( txag.map_.size() == xag.size() );
  }
  CHECK( xag.events().on_add.size() == 0 );
}

TEST_CASE( "test_view3 move assignment", "[test_view3]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag{};
  {
    test_view3<xag_network> txag{}; // the underlying ntk is a different ntk (and the events are register there)

    auto tmp = new test_view3<xag_network>{xag};
    txag = std::move( *tmp ); // move assignment
    delete tmp;

    auto const a  = xag.create_pi();
    auto const b  = xag.create_pi();
    auto const c  = xag.create_pi();
    auto const t0 = xag.create_xor( a, b );
    auto const t1 = xag.create_xor( b, c );
    auto const t2 = xag.create_and( t0, t1 );
    auto const t3 = xag.create_xor( b, t2 );
    xag.create_po( t3 );

    CHECK( xag.size() == 8u );
    CHECK( txag.map_.size() == xag.size() );
  }
  CHECK( xag.events().on_add.size() == 0 );
}

TEST_CASE( "test_view3 interesting", "[depth_view]" )
{
  fmt::print( "---------------------------------------------------------------------------\n" );
  xag_network xag_one{};
  xag_network xag_two{};

  {
    test_view3<xag_network> txag_one{xag_one};
    test_view3<xag_network> txag_two{xag_two};

    auto tmp_one = new test_view3<xag_network>{xag_one};
    txag_one = *tmp_one;
    delete tmp_one;

    auto tmp_two = new test_view3<xag_network>{xag_two};
    txag_two = *tmp_two;
    delete tmp_two;
  }

  CHECK( xag_one.events().on_add.size() == 0 );
  CHECK( xag_two.events().on_add.size() == 0 );
}
