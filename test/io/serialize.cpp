#include <catch.hpp>

#include <filesystem>

#include <mockturtle/io/serialize.hpp>

using namespace mockturtle;

#if __GNUC__ == 7
namespace fs = std::experimental::filesystem::v1;
#else
namespace fs = std::filesystem;
#endif

static constexpr char file_name[] = "aig.dmp" ;

TEST_CASE( "serialize aig_network into a file", "[serialize]" )
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_nand( a, b );
  const auto f2 = aig.create_nand( a, f1 );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( a, f1 );
  const auto f5 = aig.create_nand( f4, f3 );
  aig.create_po( f5 );

  /* serialize */
  serialize_network( aig, file_name );

  /* deserialize */
  aig_network aig2 = deserialize_network( file_name );

  CHECK( aig.size() == aig2.size() );
  CHECK( aig.num_cis() == aig2.num_cis() );
  CHECK( aig.num_cos() == aig2.num_cos() );
  CHECK( aig.num_gates() == aig2.num_gates() );

  CHECK( aig._storage->nodes == aig2._storage->nodes );
  CHECK( aig._storage->inputs == aig2._storage->inputs );
  CHECK( aig._storage->outputs == aig2._storage->outputs );
  CHECK( aig._storage->hash == aig2._storage->hash );

  CHECK( aig2._storage->hash.size() == 4u );
  CHECK( aig2._storage->nodes[f1.index].children[0u].index == a.index );
  CHECK( aig2._storage->nodes[f1.index].children[1u].index == b.index );
  CHECK( aig2._storage->nodes[f2.index].children[0u].index == a.index );
  CHECK( aig2._storage->nodes[f2.index].children[1u].index == f1.index );
  CHECK( aig2._storage->nodes[f3.index].children[0u].index == b.index );
  CHECK( aig2._storage->nodes[f3.index].children[1u].index == f1.index );
  CHECK( aig2._storage->nodes[f4.index].children[0u].index == a.index );
  CHECK( aig2._storage->nodes[f4.index].children[1u].index == f1.index );
  CHECK( aig2._storage->nodes[f5.index].children[0u].index == f4.index );
  CHECK( aig2._storage->nodes[f5.index].children[1u].index == f3.index );
}

static aig_network create_network()
{
  aig_network aig;

  const auto a = aig.create_pi();
  const auto b = aig.create_pi();

  const auto f1 = aig.create_nand( a, b );
  const auto f3 = aig.create_nand( b, f1 );
  const auto f4 = aig.create_nand( a, f1 );
  const auto f5 = aig.create_nand( f4, f3 );
  aig.create_po( f5 );

  return aig;
}

// These numbers were chosen to get 100% coverage of the `return false`
// error paths in `serialize.hpp`.
//
// To find a value that gives coverage of a particular `return false`
// statement, change the loops below to iterate from 0 to 1000000,
// configure with `-DCMAKE_BUILD_TYPE=DEBUG` and run in a debugger
// with a breakpoint set at the line of interest. When the breakpoint
// is hit, get the value of `size` from its stack frame and add it
// to this list.
static constexpr int truncate_sizes[] =
{
  0, 8, 16, 32, 40, 344, 352, 368, 376, 384, 672120
};

TEST_CASE( "write errors are propagated", "[serialize]" )
{
  aig_network aig = create_network();

  serialize_network( aig, file_name );
  size_t file_size = fs::file_size( file_name );
  INFO("File size " << file_size);

  for ( size_t size : truncate_sizes ) {
    if ( size >= file_size )
    {
      break;
    }
    phmap::BinaryOutputArchive output ( file_name, size );
    CHECK_FALSE( serialize_network_fallible( aig, output ) );
  }
}

TEST_CASE( "read errors are propagated", "[serialize]" )
{
  aig_network aig = create_network();

  serialize_network( aig, file_name );
  size_t file_size = fs::file_size( file_name );
  INFO("File size " << file_size);

  for ( size_t size : truncate_sizes ) {
    if ( size >= file_size )
    {
      break;
    }
    phmap::BinaryInputArchive input ( file_name, size );
    CHECK_FALSE( deserialize_network_fallible( input ).has_value() );
  }
}
