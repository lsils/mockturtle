#include <catch.hpp>

#include <chrono>
#include <optional>
#include <set>

#include <fmt/format.h>

#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_util.hpp>
#include <mockturtle/algorithms/aqfp_resynthesis/detail/dag_gen.hpp>

using namespace mockturtle;

void f1()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_CASE( "join a threads (without lambda)", "[thread]" )
{
  std::cout << __clang__ << std::endl;
  std::cout << __clang_major__ << std::endl;
  std::cout << __clang_minor__ << std::endl;
  
  std::thread thread( f1 );
  if ( thread.joinable() )
  {
    std::cout << "thread is joinable" << std::endl;
  }
  std::cout << "try to join" << std::endl;  
  thread.join();
  std::cout << "finished joining" << std::endl;
}

TEST_CASE( "join a threads (without args)", "[thread]" )
{
  std::thread thread( []() {} );
  if ( thread.joinable() )
  {
    std::cout << "thread is joinable" << std::endl;
  }
  std::cout << "try to join" << std::endl;  
  thread.join();
  std::cout << "finished joining" << std::endl;
}

TEST_CASE( "join a thread (with args)", "[thread]" )
{
  std::thread thread( []( auto ) {}, 0 );
  if ( thread.joinable() )
  {
    std::cout << "thread is joinable" << std::endl;
  }
  std::cout << "try to join" << std::endl;  
  thread.join();
  std::cout << "finished joining" << std::endl;
}
