#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>

#include "../traits.hpp"
#include "../utils/akers_synthesis_combinations.hpp"


#include <boost/assign/std/vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext/iota.hpp>

using namespace boost::assign;
using boost::format;
using boost::str;
using boost::adaptors::reversed;
using boost::container::flat_map;
using boost::container::flat_set;

namespace mockturtle
{

class unitized_table
{
public:
  using row_t = boost::dynamic_bitset<>;

  unitized_table( const std::string& columns )
      : columns( columns )
  {
  }

  row_t create_row() const
  {
    return row_t( columns.size() );
  }

  void add_row( const row_t& row )
  {
    rows += row;
  }

  void reduce()
  {
    auto progress = true;

    while ( progress )
    {
      progress = false;

      progress = progress || reduce_columns() || reduce_rows();
    }
  }

  friend std::ostream& operator<<( std::ostream& os, const unitized_table& table );

  inline std::vector<row_t>::const_iterator begin() const
  {
    return rows.begin();
  }

  inline std::vector<row_t>::const_iterator end() const
  {
    return rows.end();
  }

  inline int operator[]( unsigned index ) const
  {
    return (int)(unsigned char)columns[index];
  }

  inline bool is_opposite( unsigned c1, unsigned c2 ) const
  {
    const auto _n1 = columns[c1];
    const auto _n2 = columns[c2];

    const auto n1 = std::min( _n1, _n2 );
    const auto n2 = std::max( _n1, _n2 );

    return ( n1 == 0x30 && n2 == 0x31 ) || ( ( n1 + 0x20 ) == n2 );
  }

  inline unsigned num_columns() const
  {
    return columns.size();
  }

  int add_gate( const flat_set<unsigned>& gate )
  {
    assert( gate.size() == 3u );

    auto it = gate.begin();
    const auto c1 = *it++;
    const auto c2 = *it++;
    const auto c3 = *it;

    for ( auto& r : rows )
    {
      const auto bi1 = r[c1];
      const auto bi2 = r[c2];
      const auto bi3 = r[c3];

      r.push_back( ( bi1 && bi2 ) || ( bi1 && bi3 ) || ( bi2 && bi3 ) );
    }

    auto ret = (int)next_gate_id;
    columns.push_back( next_gate_id++ );
    return ret;
  }

  unsigned count_essential_ones( bool skip_last_column = true ) const
  {
    auto count = 0u;
    auto end = columns.size();
    if ( skip_last_column )
    {
      --end;
    }

    for ( auto column = 0u; column < end; ++column )
    {
      std::vector<row_t> one_rows;

      /* find rows with a 1 at column */
      for ( const auto& row : rows )
      {
        if ( row.test( column ) )
        {
          auto rt = row;
          rt.flip( column );
          one_rows.push_back( rt );
        }
      }

      /* find essential rows */
      for ( auto i = 0u; i < one_rows.size(); ++i )
      {
        for ( auto j = 0u; j < one_rows.size(); ++j )
        {
          if ( i == j )
          {
            continue;
          }

          if ( ( one_rows[i] & one_rows[j] ).none() )
          {
            ++count; /* entry i is essential in column */
            break;
          }
        }
      }
    }

    return count;
  }

private:
  bool reduce_rows()
  {
    std::vector<unsigned> to_be_removed;

    for ( auto i = 0u; i < rows.size(); ++i )
    {
      for ( auto j = i + 1u; j < rows.size(); ++j )
      {
        if ( rows[i] == rows[j] )
        {
          to_be_removed += i;
        }
        else
        {
          if ( rows[i].is_subset_of( rows[j] ) )
          {
            to_be_removed += j;
          }
          if ( rows[j].is_subset_of( rows[i] ) )
          {
            to_be_removed += i;
          }
        }
      }
    }

    boost::sort( to_be_removed );
    to_be_removed.erase( std::unique( to_be_removed.begin(), to_be_removed.end() ), to_be_removed.end() );

    boost::reverse( to_be_removed );

    for ( auto index : to_be_removed )
    {
      rows.erase( rows.begin() + index );
    }
    return !to_be_removed.empty();
  }

  bool reduce_columns()
  {
    std::vector<unsigned> to_be_removed;

    auto mask = ~create_row();

    for ( auto c = 0u; c < columns.size(); ++c )
    {
      mask.reset( c );
      auto can_be_removed = true;

      /* pretend column c is removed */
      for ( auto i = 0u; i < rows.size(); ++i )
      {
        for ( auto j = i + 1u; j < rows.size(); ++j )
        {
          const auto result = rows[i] & rows[j] & mask;

          if ( result.none() )
          {
            can_be_removed = false;
            break;
          }
        }

        if ( !can_be_removed )
        {
          break;
        }
      }

      if ( can_be_removed )
      {
        to_be_removed += c;
      }
      else
      {
        mask.set( c );
      }
    }

    /* remove columns */
    boost::reverse( to_be_removed );

    for ( auto index : to_be_removed )
    {
      columns.erase( columns.begin() + index );

      for ( auto& row : rows )
      {
        erase_bit( row, index );
      }
    }

    return !to_be_removed.empty();
  }

  void erase_bit( row_t& row, unsigned pos )
  {
    for ( auto i = pos + 1u; i < row.size(); ++i )
    {
      row[i - 1u] = row[i];
    }
    row.resize( row.size() - 1u );
  }

public:
  std::string columns;

  std::vector<row_t> rows;

  unsigned char next_gate_id = 'z' + 0x21;
};

std::ostream& operator<<( std::ostream& os, const unitized_table& table )
{
  os << table.columns << std::endl;

  for ( const auto& row : table.rows )
  {
    std::string buffer; 
    to_string( row , buffer);
    os << ( buffer | reversed ) << std::endl;
  }

  return os;
}

bool operator==( unitized_table& table,  unitized_table& original_table)
{
	if (table.rows.size() != original_table.rows.size())
		return false;
	for (auto x = 0u; x < table.rows.size(); x++)
	{
		if (table.rows[x] == original_table.rows[x])
			continue; 
		else return false; 
	}

  return true;
}

namespace detail
{
template<typename Ntk, typename LeavesIterator>
class akers_synthesis
{
public:
  akers_synthesis( Ntk ntk, kitty::dynamic_truth_table const& func, kitty::dynamic_truth_table const& care, LeavesIterator begin, LeavesIterator end )
      : ntk( ntk ),
        func( func ),
        care( care ),
        begin( begin ),
        end( end )
  {
  }

public:
  signal<Ntk> run()
  {
    auto table = create_unitized_table();
    return synthesize( table );
  }

private:
  unitized_table create_unitized_table()
  {
    const unsigned int num_vars = func.num_vars();

    /* create column names */
    std::string columns;

    for ( auto i = 0u; i < num_vars; ++i )
    {
      columns += 'a' + i;
    }
    for ( auto i = 0u; i < num_vars; ++i )
    {
      columns += 'A' + i;
    }
    columns += '0';
    columns += '1';

    unitized_table table( columns );

    /* add rows */
    for ( auto pos = 0u; pos < care.num_bits(); pos++ )
    {
      boost::dynamic_bitset<> half( num_vars, pos );
      auto row = table.create_row();

      /* copy the values */
      for ( auto i = 0u; i < num_vars; ++i )
      {
        row[i] = half[i];
        row[i + num_vars] = !half[i];
      }
      row[num_vars << 1] = false;
      row[( num_vars << 1 ) + 1] = true;

      if ( !kitty::get_bit( func, pos ) )
      {
        row.flip();
      }
      table.add_row( row );
    }
    /*foreach_bit( care, [&num_vars, this, &table]( unsigned pos ) {
    kitty::
      boost::dynamic_bitset<> half( num_vars, pos );
      auto row = table.create_row();

      //copy the values 
      for ( auto i = 0u; i < num_vars; ++i )
      {
        row[i] = half[i];
        row[i + num_vars] = !half[i];
      }
      row[num_vars << 1] = false;
      row[( num_vars << 1 ) + 1] = true;

      if ( !kitty::get_bit(func,pos) )
      {
        row.flip();
      }
      table.add_row( row );
    } );*/
    
    table.reduce();
    return table;
  }

  flat_set<flat_set<unsigned>> find_gates_for_column( const unitized_table& table, unsigned column )
  {
    std::vector<unitized_table::row_t> one_rows;
    boost::dynamic_bitset<> matrix;

    /* find rows with a 1 at column */
    for ( const auto& row : table )
    {
      if ( row.test( column ) )
      {
        auto rt = row;
        rt.flip( column );
        one_rows.push_back( rt );
      }
    }

    /* find essential rows */
    for ( auto i = 0u; i < one_rows.size(); ++i )
    {
      for ( auto j = 0u; j < one_rows.size(); ++j )
      {
        if ( i == j )
        {
          continue;
        }

        if ( ( one_rows[i] & one_rows[j] ).none() )
        {
          for ( auto k = 0u; k < one_rows[i].size(); ++k )
          {
            matrix.push_back( one_rows[i][k] );
          }
          break;
        }
      }
    }

    return clauses_to_products_enumerative( table, column, matrix );
  }

  flat_set<unsigned> find_gate_for_table( unitized_table& table )
  {
    flat_map<flat_set<unsigned>, unsigned> gates;
    flat_map<unsigned, flat_set<unsigned>> random_gates;
    auto g_count = 0;

    for ( auto c = 0u; c < table.num_columns(); ++c )
    {
      for ( const auto& g : find_gates_for_column( table, c ) )
      {
        gates[g]++;
        random_gates[g_count] = g;
        g_count++;
      }
    }

    if ( gates.empty() )
    {
      reduce++;
      return find_gate_for_table_brute_force( table );
    }
    if ( gates.size() == previous_size )
    {
      reduce++;
      return find_gate_for_table_brute_force( table );
    }

    assert( !gates.empty() );
    reduce = 0;
    previous_size = gates.size();
    using pair_t = decltype( gates )::value_type;

    for ( auto f = 0u; f < random_gates.size(); f++ )
    {
      auto pr = std::max_element( std::begin( gates ), std::end( gates ), []( const pair_t& p1, const pair_t& p2 ) { return p1.second < p2.second; } );
      random_gates[f] = pr->first;
      gates.erase( pr->first );
    }

    auto this_table = table;
    for ( auto f = 0; f < g_count; f++ )
    {
      auto last_gate_id = table.add_gate( random_gates[f] );
      table.reduce();
      if ( ( table.rows.size() != this_table.rows.size() ) || ( table.columns.size() != this_table.columns.size() - 1 ) )
      {
        table = this_table;
        return random_gates[f];
      }
      table = this_table;
    }

    reduce++;
    return random_gates[0];
  }

  flat_set<unsigned> find_gate_for_table_brute_force( const unitized_table& table )
  {
    auto best_count = std::numeric_limits<unsigned>::max();
    flat_set<unsigned> best_gate;

    std::vector<unsigned> numbers( table.num_columns() );
    boost::iota( numbers, 0u );

    boost::unofficial::for_each_combination( numbers.begin(), numbers.begin() + 3, numbers.end(),
                                             [&best_count, &best_gate, &table]( decltype( numbers )::const_iterator first,
                                                                                decltype( numbers )::const_iterator last ) {
                                               flat_set<unsigned> gate;
                                               while ( first != last )
                                               {
                                                 gate.insert( *first++ );
                                               }

                                               auto table_copy = table;
                                               table_copy.add_gate( gate );

                                               const auto new_count = table_copy.count_essential_ones();
                                               if ( new_count < best_count )
                                               {
                                                 best_count = new_count;
                                                 best_gate = gate;
                                               }

                                               return false;
                                             } );

    return best_gate;
  }

  signal<Ntk> synthesize( unitized_table& table )
  {

    std::unordered_map<int, signal<Ntk>> c_to_f;

    c_to_f[0x30] = ntk.get_constant( false );
    c_to_f[0x31] = ntk.get_constant( true );

    for ( auto i = 0; i < func.num_vars(); ++i )
    {
      auto pi = *begin++; // should take the leaves values
      c_to_f[0x41 + i] = !pi;
      c_to_f[0x61 + i] = pi;
    }

    auto last_gate_id = 0;

    while ( table.num_columns() )
    {
      auto gate = find_gate_for_table( table );

      auto it = gate.begin();
      const auto f1 = *it++;
      const auto f2 = *it++;
      const auto f3 = *it;

      last_gate_id = table.add_gate( gate );

      c_to_f[last_gate_id] = ntk.create_maj( c_to_f[table[f1]], c_to_f[table[f2]], c_to_f[table[f3]] );

      //std::cout << "[i] create gate " << last_gate_id << " " << table[f1] << " " << table[f2] << " " << table[f3] << std::endl;
      if ( reduce == 0 )
      table.reduce();
      //std::cout << table << std::endl; 
    }

    return c_to_f[last_gate_id];
  }

  std::vector<std::vector<int>> create_gates( const unitized_table& table )
  {
    const auto num_vars = func.num_vars();
    std::vector<int> count( table.columns.size(), 0 );
    auto best_count = 0;

    std::vector<std::vector<int>> gates;

    for ( auto c = 0u; c < table.columns.size(); ++c )
    {
      for ( const auto& row : table )
      {
        best_count++;
        if ( !row.test( c ) )
        {
          count[c]++;
        }
      }
    }

    auto best_column = 0;

    for ( auto c = 0; c < count.size(); ++c )
    {
      if ( count[c] < best_count )
      {
        best_column = c;
        best_count = count[c];
      }
    }

    auto icx = 0;
    auto name = table.columns[best_column];
    if ( islower( name ) )
      icx = name - 'a';
    else if ( isupper( name ) )
      icx = name - 'A' + num_vars;
    else if ( name == '0' )
      icx = num_vars * 2;
    else
      icx = num_vars * 2 + 1;

    std::vector<int> best_c;

    best_c.push_back( icx );
    gates.push_back( best_c );

    for ( const auto& row : table )
    {

      if ( !row.test( best_column ) )
      {
        std::vector<int> gate1;
        for ( auto c = 0u; c < table.num_columns(); ++c )
        {
          if ( c == best_column )
            continue;
          if ( row.test( c ) )
          {
            unsigned icx;
            auto name = table.columns[c];
            if ( islower( name ) )
              icx = name - 'a';
            else if ( isupper( name ) )
              icx = name - 'A' + num_vars;
            else if ( name == '0' )
              icx = num_vars * 2;
            else
              icx = num_vars * 2 + 1;
            gate1.push_back( icx );
          }
        }
        gates.push_back( gate1 );
      }
    }
    std::vector<int> g;
    g.push_back( num_vars );

    gates.push_back( g );
    return gates;
  }

  flat_set<flat_set<unsigned>> clauses_to_products_enumerative( const unitized_table& table, unsigned column,
                                                                const boost::dynamic_bitset<>& matrix )
  {
    flat_set<flat_set<unsigned>> products;

    const auto num_columns = table.num_columns();
    const auto num_rows = matrix.size() / num_columns;

    for ( auto i = 0u; i < num_columns; ++i )
    {
      if ( table.is_opposite( column, i ) )
      {
        continue;
      }

      for ( auto j = i + 1u; j < num_columns; ++j )
      {
        if ( table.is_opposite( i, j ) || table.is_opposite( column, j ) )
        {
          continue;
        }

        auto found = true;
        auto offset = 0u;
        for ( auto r = 0u; r < num_rows; ++r, offset += num_columns )
        {
          if ( !matrix[offset + i] && !matrix[offset + j] )
          {
            found = false;
            break;
          }
        }

        if ( found )
        {
          flat_set<unsigned> product;
          product.insert( i );
          product.insert( j );
          product.insert( column );
          products.insert( product );
        }
      }
    }

    return products;
  }

private:
  Ntk ntk;
  kitty::dynamic_truth_table const& func;
  kitty::dynamic_truth_table const& care;
  LeavesIterator begin;
  LeavesIterator end;

  unsigned reduce = 0;
  unsigned previous_size = 0;
};

} // namespace detail

template<typename Ntk, typename LeavesIterator>
signal<Ntk> akers_synthesis( Ntk& ntk, kitty::dynamic_truth_table const& func, kitty::dynamic_truth_table const& care, LeavesIterator begin, LeavesIterator end )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );
  static_assert( has_create_maj_v<Ntk>, "Ntk does not implement the create_po method" );

  detail::akers_synthesis<Ntk, LeavesIterator> tt( ntk, func, care, begin, end );
  return tt.run();
}

template<typename Ntk>
Ntk akers_synthesis( Ntk& ntk, kitty::dynamic_truth_table const& func, kitty::dynamic_truth_table const& care )
{
  static_assert( is_network_type_v<Ntk>, "Ntk is not a network type" );
  static_assert( has_create_pi_v<Ntk>, "Ntk does not implement the create_pi method" );
  static_assert( has_create_po_v<Ntk>, "Ntk does not implement the create_po method" );

  std::vector<signal<Ntk>> pis;

  for ( auto i = 0; i < func.num_vars(); ++i )
  {
    pis.push_back( ntk.create_pi() );
  }

  const auto f = akers_synthesis( ntk, func, care, pis.begin(), pis.end() );
  ntk.create_po( f );
  return ntk;
}
} // namespace mockturtle
