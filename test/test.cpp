#define CATCH_CONFIG_RUNNER

#include <catch.hpp>

#include <filesystem>
#include <random>

#include <fmt/format.h>

#if __GNUC__ == 7
namespace fs = std::experimental::filesystem::v1;
#else
namespace fs = std::filesystem;
#endif

// Insecure but portable creation of a temporary directory
static fs::path temp_directory()
{
  std::random_device rd;
  std::mt19937_64 generator( rd() );
  std::uniform_int_distribution<uint64_t> distribution( 0, std::numeric_limits<uint64_t>::max() );
  uint64_t random_number = distribution( generator );

  fs::path path = fs::temp_directory_path();
  path /= fmt::format( "mockturtle_test_{:x}", random_number );
  return path;
}

int main(int argc, char* argv[])
{
  auto temp_dir = temp_directory();
  fs::create_directory( temp_dir );
  fs::current_path( temp_dir );

  int exit_code = Catch::Session().run(argc, argv);

  if ( !exit_code ) {
    fs::current_path( ".." );
    fs::remove_all( temp_dir );
  }

  return exit_code;
}
