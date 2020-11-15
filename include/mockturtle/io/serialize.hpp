/* mockturtle: C++ logic network library
 * Copyright (C) 2018-2020  EPFL
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file serialize.hpp
  \brief Serialize network into a file

  \author Heinz Riener

  This file implements functions to serialize a (combinational)
  `aig_network` into a file.  The serializer should be used for
  debugging-purpose only.  It allows to store the current state of the
  network (including dangling and dead nodes), but does not guarantee
  platform-independence (use, e.g., `write_verilog` instead).
*/

#pragma once

#include <mockturtle/networks/aig.hpp>
#include <fstream>

namespace mockturtle
{

namespace detail
{

struct serializer
{
public:
  using node_type = typename aig_network::storage::element_type::node_type;
  using pointer_type = typename node_type::pointer_type;

public:
  bool operator()( std::ostream *os, uint64_t const& data ) const
  {
    os->write( (char*)&data, sizeof( uint64_t ) );
    return os->good();
  }

  bool operator()( std::istream *is, uint64_t* data ) const
  {
    is->read( (char*)data, sizeof( uint64_t ) );
    return is->good();
  }

  template<int PointerFieldSize>
  bool operator()( std::ostream *os, node_pointer<PointerFieldSize> const& ptr ) const
  {
    os->write( (char*)&ptr.data, sizeof( ptr.data ) );
    return os->good();
  }

  template<int PointerFieldSize>
  bool operator()( std::istream *is, node_pointer<PointerFieldSize>* ptr ) const
  {
    is->read( (char*)&ptr->data, sizeof( ptr->data ) );
    return is->good();
  }
  
  bool operator()( std::ostream *os, cauint64_t const& data ) const
  {
    os->write( (char*)&data.n, sizeof( data.n ) );
    return os->good();
  }

  bool operator()( std::istream *is, cauint64_t* data ) const
  {
    is->read( (char*)&data->n, sizeof( data->n ) );
    return is->good();
  }
  
  template<int Fanin, int Size, int PointerFieldSize>
  bool operator()( std::ostream *os, regular_node<Fanin, Size, PointerFieldSize> const& n ) const
  {
    uint64_t size = n.children.size();
    os->write( (char*)&size, sizeof( uint64_t ) );

    for ( const auto& c : n.children )
    {
      bool result = this->operator()( os, c );
      if ( !result )
      {
        return false;
      }
    }

    size = n.data.size();
    os->write( (char*)&size, sizeof( uint64_t ) );
    for ( const auto& d : n.data )
    {
      bool result = this->operator()( os, d );
      if ( !result )
      {
        return false;
      }
    }

    return os->good();
  }

  template<int Fanin, int Size, int PointerFieldSize>
  bool operator()( std::istream *is, const regular_node<Fanin, Size, PointerFieldSize>* n ) const
  {
    uint64_t size;
    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      pointer_type ptr;
      bool result = this->operator()( is, &ptr );
      if ( !result )
      {
        return false;
      }
      const_cast<regular_node<Fanin, Size, PointerFieldSize>*>( n )->children[i] = ptr;
    }

    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      cauint64_t data;
      bool result = this->operator()( is, &data );
      if ( !result )
      {
        return false;
      }
      const_cast<regular_node<Fanin, Size, PointerFieldSize>*>( n )->data[i] = data;
    }

    return is->good();
  }

  bool operator()( std::ostream *os, std::pair<const node_type, uint64_t> const& value ) const
  {
    return this->operator()( os, value.first ) && this->operator()( os, value.second );
  }

  bool operator()( std::istream *is, std::pair<const node_type, uint64_t>* value ) const
  {
    return this->operator()( is, &value->first ) && this->operator()( is, &value->second );
  }

  bool operator()( std::ostream *os, aig_storage const& storage ) const
  {
    /* nodes */
    uint64_t size = storage.nodes.size();
    os->write( (char*)&size, sizeof( uint64_t ) );
    for ( const auto& n : storage.nodes )
    {
      if ( !this->operator()( os, n ) )
      {
        return false;
      }
    }

    /* inputs */
    size = storage.inputs.size();
    os->write( (char*)&size, sizeof( uint64_t ) );
    for ( const auto& i : storage.inputs )
    {
      if ( !this->operator()( os, i ) )
      {
        return false;
      }
    }

    /* outputs */
    size = storage.outputs.size();
    os->write( (char*)&size, sizeof( uint64_t ) );
    for ( const auto& o : storage.outputs )
    {
      if ( !this->operator()( os, o ) )
      {
        return false;
      }
    }

    /* hash */
    const_cast<aig_storage&>( storage ).hash.serialize( *this, os );

    /* storage data */
    os->write( (char*)&storage.data.num_pis, sizeof( uint32_t ) );
    os->write( (char*)&storage.data.num_pos, sizeof( uint32_t ) );
    size = storage.data.latches.size();
    for ( const auto& l : storage.data.latches )
    {
      os->write( (char*)&l, sizeof( int8_t ) );
    }
    os->write( (char*)&storage.data.trav_id, sizeof( uint32_t ) );    
    
    return true;
  }

  bool operator()( std::istream *is, aig_storage* storage ) const
  {
    /* nodes */
    uint64_t size;
    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      node_type n;
      if ( !this->operator()( is, &n ) )
      {
        return false;
      }
      storage->nodes.push_back( n );
    }
  
    /* inputs */
    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      uint64_t value;
      is->read( (char*)&value, sizeof( uint64_t ) );
      storage->inputs.push_back( value );
    }

    /* outputs */
    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      pointer_type ptr;
      if ( !this->operator()( is, &ptr ) )
      {
        return false;
      }
      storage->outputs.push_back( ptr );
    }

    /* hash */
    if ( !storage->hash.unserialize( *this, is ) )
    {
      return false;
    }
  
    /* aig_storage_data */
    is->read( (char*)&storage->data.num_pis, sizeof( uint32_t ) );
    is->read( (char*)&storage->data.num_pos, sizeof( uint32_t ) );
    is->read( (char*)&size, sizeof( uint64_t ) );
    for ( uint64_t i = 0; i < size; ++i )
    {
      int8_t l;
      is->read( (char*)&l, sizeof( int8_t ) );    
      storage->data.latches.push_back( l );
    }
    is->read( (char*)&storage->data.trav_id, sizeof( uint32_t ) );

    return true;
  }
}; /* struct serializer */

} /* namespace detail */

/*! \brief Serializes a combinational AIG network to a stream
 *
 * \param aig Combinational AIG network
 * \param os Output stream
 */
inline std::ostream& serialize_network( aig_network const& aig, std::ostream& os )
{
  detail::serializer _serializer;
  bool const okay = _serializer( &os, *aig._storage );
  (void)okay;
  assert( okay && "failed to serialize the network onto stream" );
  return os;
}

/*! \brief Serializes a combinational AIG network in a file
 *
 * \param aig Combinational AIG network
 * \param filename Filename
 */
inline void serialize_network( aig_network const& aig, std::string const& filename )
{
  std::ofstream os;
  os.open( filename, std::fstream::out | std::fstream::binary );
  serialize_network( aig, os );
  os.close();
}

/*! \brief Deserializes a combinational AIG network from a stream
 *
 * \param is Input stream
 * \return Deserialized AIG network
 */
inline aig_network deserialize_network( std::istream& is )
{
  detail::serializer _serializer;
  auto storage = std::make_shared<aig_storage>();
  storage->nodes.clear();
  storage->inputs.clear();
  storage->outputs.clear();
  storage->latch_information.clear();
  storage->hash.clear();

  bool const okay = _serializer( &is, storage.get() );
  (void)okay;
  assert( okay && "failed to deserialize the network onto stream" );
  return aig_network{storage};
}

/*! \brief Deserializes a combinational AIG network from a file
 *
 * \param filename Filename
 * \return Deserialized AIG network
 */
inline aig_network deserialize_network( std::string const& filename )
{
  std::ifstream is;
  is.open( filename, std::fstream::in | std::fstream::binary );
  auto aig = deserialize_network( is );
  is.close();
  return aig;
}

} /* namespace mockturtle */
