#include <algorithm>
#include <iterator>

/* From Cirkit -- core -- utils -- combinations */

namespace boost
{
namespace unofficial
{

namespace detail
{

// Rotates two discontinuous ranges to put *first2 where *first1 is.
//     If last1 == first2 this would be equivalent to rotate(first1, first2, last2),
//     but instead the rotate "jumps" over the discontinuity [last1, first2) -
//     which need not be a valid range.
//     In order to make it faster, the length of [first1, last1) is passed in as d1,
//     and d2 must be the length of [first2, last2).
//  In a perfect world the d1 > d2 case would have used swap_ranges and
//     reverse_iterator, but reverse_iterator is too inefficient.
template<class BidirIter>
void rotate_discontinuous( BidirIter first1, BidirIter last1,
                           typename std::iterator_traits<BidirIter>::difference_type d1,
                           BidirIter first2, BidirIter last2,
                           typename std::iterator_traits<BidirIter>::difference_type d2 )
{
  using std::swap;
  if ( d1 <= d2 )
    std::rotate( first2, std::swap_ranges( first1, last1, first2 ), last2 );
  else
  {
    BidirIter i1 = last1;
    while ( first2 != last2 )
      swap( *--i1, *--last2 );
    std::rotate( first1, i1, last1 );
  }
}

// Rotates the three discontinuous ranges to put *first2 where *first1 is.
// Just like rotate_discontinuous, except the second range is now represented by
//    two discontinuous ranges: [first2, last2) + [first3, last3).
template<class BidirIter>
void rotate_discontinuous3( BidirIter first1, BidirIter last1,
                            typename std::iterator_traits<BidirIter>::difference_type d1,
                            BidirIter first2, BidirIter last2,
                            typename std::iterator_traits<BidirIter>::difference_type d2,
                            BidirIter first3, BidirIter last3,
                            typename std::iterator_traits<BidirIter>::difference_type d3 )
{
  rotate_discontinuous( first1, last1, d1, first2, last2, d2 );
  if ( d1 <= d2 )
    rotate_discontinuous( std::next( first2, d2 - d1 ), last2, d1, first3, last3, d3 );
  else
  {
    rotate_discontinuous( std::next( first1, d2 ), last1, d1 - d2, first3, last3, d3 );
    rotate_discontinuous( first2, last2, d2, first3, last3, d3 );
  }
}

// Call f() for each combination of the elements [first1, last1) + [first2, last2)
//    swapped/rotated into the range [first1, last1).  As long as f() returns
//    false, continue for every combination and then return [first1, last1) and
//    [first2, last2) to their original state.  If f() returns true, return
//    immediately.
//  Does the absolute mininum amount of swapping to accomplish its task.
//  If f() always returns false it will be called (d1+d2)!/(d1!*d2!) times.
template<class BidirIter, class Function>
bool combine_discontinuous( BidirIter first1, BidirIter last1,
                            typename std::iterator_traits<BidirIter>::difference_type d1,
                            BidirIter first2, BidirIter last2,
                            typename std::iterator_traits<BidirIter>::difference_type d2,
                            Function& f,
                            typename std::iterator_traits<BidirIter>::difference_type d = 0 )
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;
  using std::swap;
  if ( d1 == 0 || d2 == 0 )
    return f();
  if ( d1 == 1 )
  {
    for ( BidirIter i2 = first2; i2 != last2; ++i2 )
    {
      if ( f() )
        return true;
      swap( *first1, *i2 );
    }
  }
  else
  {
    BidirIter f1p = std::next( first1 );
    BidirIter i2 = first2;
    for ( D d22 = d2; i2 != last2; ++i2, --d22 )
    {
      if ( combine_discontinuous( f1p, last1, d1 - 1, i2, last2, d22, f, d + 1 ) )
        return true;
      swap( *first1, *i2 );
    }
  }
  if ( f() )
    return true;
  if ( d != 0 )
    rotate_discontinuous( first1, last1, d1, std::next( first2 ), last2, d2 - 1 );
  else
    rotate_discontinuous( first1, last1, d1, first2, last2, d2 );
  return false;
}

// A binder for binding arguments to call combine_discontinuous
template<class Function, class BidirIter>
class call_combine_discontinuous
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;
  Function f_;
  BidirIter first1_;
  BidirIter last1_;
  D d1_;
  BidirIter first2_;
  BidirIter last2_;
  D d2_;

public:
  call_combine_discontinuous(
      BidirIter first1, BidirIter last1,
      D d1,
      BidirIter first2, BidirIter last2,
      D d2,
      Function& f )
      : f_( f ), first1_( first1 ), last1_( last1 ), d1_( d1 ),
        first2_( first2 ), last2_( last2 ), d2_( d2 ) {}

  bool operator()()
  {
    return combine_discontinuous( first1_, last1_, d1_, first2_, last2_, d2_, f_ );
  }
};

// See combine_discontinuous3
template<class BidirIter, class Function>
bool combine_discontinuous3_( BidirIter first1, BidirIter last1,
                              typename std::iterator_traits<BidirIter>::difference_type d1,
                              BidirIter first2, BidirIter last2,
                              typename std::iterator_traits<BidirIter>::difference_type d2,
                              BidirIter first3, BidirIter last3,
                              typename std::iterator_traits<BidirIter>::difference_type d3,
                              Function& f,
                              typename std::iterator_traits<BidirIter>::difference_type d = 0 )
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;
  using std::swap;
  if ( d1 == 1 )
  {
    for ( BidirIter i2 = first2; i2 != last2; ++i2 )
    {
      if ( f() )
        return true;
      swap( *first1, *i2 );
    }
    if ( f() )
      return true;
    swap( *first1, *std::prev( last2 ) );
    swap( *first1, *first3 );
    for ( BidirIter i2 = std::next( first3 ); i2 != last3; ++i2 )
    {
      if ( f() )
        return true;
      swap( *first1, *i2 );
    }
  }
  else
  {
    BidirIter f1p = std::next( first1 );
    BidirIter i2 = first2;
    for ( D d22 = d2; i2 != last2; ++i2, --d22 )
    {
      if ( combine_discontinuous3_( f1p, last1, d1 - 1, i2, last2, d22, first3,
                                    last3, d3, f, d + 1 ) )
        return true;
      swap( *first1, *i2 );
    }
    i2 = first3;
    for ( D d22 = d3; i2 != last3; ++i2, --d22 )
    {
      if ( combine_discontinuous( f1p, last1, d1 - 1, i2, last3, d22, f, d + 1 ) )
        return true;
      swap( *first1, *i2 );
    }
  }
  if ( f() )
    return true;
  if ( d1 == 1 )
    swap( *std::prev( last2 ), *first3 );
  if ( d != 0 )
  {
    if ( d2 > 1 )
      rotate_discontinuous3( first1, last1, d1, std::next( first2 ), last2, d2 - 1, first3, last3, d3 );
    else
      rotate_discontinuous( first1, last1, d1, first3, last3, d3 );
  }
  else
    rotate_discontinuous3( first1, last1, d1, first2, last2, d2, first3, last3, d3 );
  return false;
}

// Like combine_discontinuous, but swaps/rotates each combination out of
//    [first1, last1) + [first2, last2) + [first3, last3) into [first1, last1).
//    If f() always returns false, it is called (d1+d2+d3)!/(d1!*(d2+d3)!) times.
template<class BidirIter, class Function>
bool combine_discontinuous3( BidirIter first1, BidirIter last1,
                             typename std::iterator_traits<BidirIter>::difference_type d1,
                             BidirIter first2, BidirIter last2,
                             typename std::iterator_traits<BidirIter>::difference_type d2,
                             BidirIter first3, BidirIter last3,
                             typename std::iterator_traits<BidirIter>::difference_type d3,
                             Function& f )
{
  typedef call_combine_discontinuous<Function&, BidirIter> F;
  F fbc( first2, last2, d2, first3, last3, d3, f ); // BC
  return combine_discontinuous3_( first1, last1, d1, first2, last2, d2, first3, last3, d3, fbc );
}

// See permute
template<class BidirIter, class Function>
bool permute_( BidirIter first1, BidirIter last1,
               typename std::iterator_traits<BidirIter>::difference_type d1,
               Function& f )
{
  using std::swap;
  switch ( d1 )
  {
  case 0:
  case 1:
    return f();
  case 2:
    if ( f() )
      return true;
    swap( *first1, *std::next( first1 ) );
    return f();
  case 3:
  {
    if ( f() )
      return true;
    BidirIter f2 = std::next( first1 );
    BidirIter f3 = std::next( f2 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *first1, *f3 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *first1, *f2 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *f2, *f3 );
    return f();
  }
  }
  BidirIter fp1 = std::next( first1 );
  for ( BidirIter p = fp1; p != last1; ++p )
  {
    if ( permute_( fp1, last1, d1 - 1, f ) )
      return true;
    std::reverse( fp1, last1 );
    swap( *first1, *p );
  }
  return permute_( fp1, last1, d1 - 1, f );
}

// Calls f() for each permutation of [first1, last1)
// Divided into permute and permute_ in a (perhaps futile) attempt to
//    squeeze a little more performance out of it.
template<class BidirIter, class Function>
bool permute( BidirIter first1, BidirIter last1,
              typename std::iterator_traits<BidirIter>::difference_type d1,
              Function& f )
{
  using std::swap;
  switch ( d1 )
  {
  case 0:
  case 1:
    return f();
  case 2:
  {
    if ( f() )
      return true;
    BidirIter i = std::next( first1 );
    swap( *first1, *i );
    if ( f() )
      return true;
    swap( *first1, *i );
  }
  break;
  case 3:
  {
    if ( f() )
      return true;
    BidirIter f2 = std::next( first1 );
    BidirIter f3 = std::next( f2 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *first1, *f3 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *first1, *f2 );
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *f2, *f3 );
    if ( f() )
      return true;
    swap( *first1, *f3 );
  }
  break;
  default:
    BidirIter fp1 = std::next( first1 );
    for ( BidirIter p = fp1; p != last1; ++p )
    {
      if ( permute_( fp1, last1, d1 - 1, f ) )
        return true;
      std::reverse( fp1, last1 );
      swap( *first1, *p );
    }
    if ( permute_( fp1, last1, d1 - 1, f ) )
      return true;
    std::reverse( first1, last1 );
    break;
  }
  return false;
}

// Creates a functor with no arguments which calls f_(first_, last_).
//   Also has a variant that takes two It and ignores them.
template<class Function, class It>
class bound_range
{
  Function f_;
  It first_;
  It last_;

public:
  bound_range( Function f, It first, It last )
      : f_( f ), first_( first ), last_( last ) {}

  bool
  operator()()
  {
    return f_( first_, last_ );
  }

  bool
  operator()( It, It )
  {
    return f_( first_, last_ );
  }
};

// A binder for binding arguments to call permute
template<class Function, class It>
class call_permute
{
  typedef typename std::iterator_traits<It>::difference_type D;
  Function f_;
  It first_;
  It last_;
  D d_;

public:
  call_permute( Function f, It first, It last, D d )
      : f_( f ), first_( first ), last_( last ), d_( d ) {}

  bool
  operator()()
  {
    return permute( first_, last_, d_, f_ );
  }
};

} // namespace detail

template<class BidirIter, class Function>
Function
for_each_combination( BidirIter first, BidirIter mid,
                      BidirIter last, Function f )
{
  detail::bound_range<Function&, BidirIter> wfunc( f, first, mid );
  detail::combine_discontinuous( first, mid, std::distance( first, mid ),
                                 mid, last, std::distance( mid, last ),
                                 wfunc );
  return std::move( f );
}

template<class UInt>
UInt gcd( UInt x, UInt y )
{
  while ( y != 0 )
  {
    UInt t = x % y;
    x = y;
    y = t;
  }
  return x;
}

template<class UInt>
UInt count_each_combination( UInt d1, UInt d2 )
{
  if ( d2 < d1 )
    std::swap( d1, d2 );
  if ( d1 == 0 )
    return 1;
  if ( d1 > std::numeric_limits<UInt>::max() - d2 )
    throw std::overflow_error( "overflow in count_each_combination" );
  UInt n = d1 + d2;
  UInt r = n;
  --n;
  for ( UInt k = 2; k <= d1; ++k, --n )
  {
    // r = r * n / k, known to not not have truncation error
    UInt g = gcd( r, k );
    r /= g;
    UInt t = n / ( k / g );
    if ( r > std::numeric_limits<UInt>::max() / t )
      throw std::overflow_error( "overflow in count_each_combination" );
    r *= t;
  }
  return r;
}

template<class BidirIter>
std::uintmax_t
count_each_combination( BidirIter first, BidirIter mid, BidirIter last )
{
  return count_each_combination<std::uintmax_t>( std::distance( first, mid ), std::distance( mid, last ) );
}

// For each of the permutation algorithms, use for_each_combination (or
//    combine_discontinuous) to handle the "r out of N" part of the algorithm.
//    Thus each permutation algorithm has to deal only with an "N out of N"
//    problem.  I.e. For each combination of r out of N items, permute it thusly.
template<class BidirIter, class Function>
Function
for_each_permutation( BidirIter first, BidirIter mid,
                      BidirIter last, Function f )
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;
  typedef detail::bound_range<Function&, BidirIter> Wf;
  typedef detail::call_permute<Wf, BidirIter> PF;
  Wf wfunc( f, first, mid );
  D d1 = std::distance( first, mid );
  PF pf( wfunc, first, mid, d1 );
  detail::combine_discontinuous( first, mid, d1,
                                 mid, last, std::distance( mid, last ),
                                 pf );
  return std::move( f );
}

template<class UInt>
UInt count_each_permutation( UInt d1, UInt d2 )
{
  // return (d1+d2)!/d2!
  if ( d1 > std::numeric_limits<UInt>::max() - d2 )
    throw std::overflow_error( "overflow in count_each_permutation" );
  UInt n = d1 + d2;
  UInt r = 1;
  for ( ; n > d2; --n )
  {
    if ( r > std::numeric_limits<UInt>::max() / n )
      throw std::overflow_error( "overflow in count_each_permutation" );
    r *= n;
  }
  return r;
}

template<class BidirIter>
std::uintmax_t
count_each_permutation( BidirIter first, BidirIter mid, BidirIter last )
{
  return count_each_permutation<std::uintmax_t>( std::distance( first, mid ), std::distance( mid, last ) );
}

namespace detail
{

// Adapt functor to permute over [first+1, last)
//   A circular permutation of N items is done by holding the first item and
//   permuting [first+1, last).
template<class Function, class BidirIter>
class circular_permutation
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;

  Function f_;
  D s_;

public:
  explicit circular_permutation( Function f, D s ) : f_( f ), s_( s ) {}

  bool
  operator()( BidirIter first, BidirIter last )
  {
    if ( s_ <= 1 )
      return f_( first, last );
    bound_range<Function, BidirIter> f( f_, first, last );
    return permute( std::next( first ), last, s_ - 1, f );
  }
};

} // namespace detail

template<class BidirIter, class Function>
Function
for_each_circular_permutation( BidirIter first,
                               BidirIter mid,
                               BidirIter last, Function f )
{
  for_each_combination( first, mid, last, detail::circular_permutation<Function&, BidirIter>( f, std::distance( first, mid ) ) );
  return std::move( f );
}

template<class UInt>
UInt count_each_circular_permutation( UInt d1, UInt d2 )
{
  // return d1 > 0 ? (d1+d2)!/(d1*d2!) : 1
  if ( d1 == 0 )
    return 1;
  UInt r;
  if ( d1 <= d2 )
  {
    try
    {
      r = count_each_combination( d1, d2 );
    }
    catch ( const std::overflow_error& )
    {
      throw std::overflow_error( "overflow in count_each_circular_permutation" );
    }
    for ( --d1; d1 > 1; --d1 )
    {
      if ( r > std::numeric_limits<UInt>::max() / d1 )
        throw std::overflow_error( "overflow in count_each_circular_permutation" );
      r *= d1;
    }
  }
  else
  { // functionally equivalent but faster algorithm
    if ( d1 > std::numeric_limits<UInt>::max() - d2 )
      throw std::overflow_error( "overflow in count_each_circular_permutation" );
    UInt n = d1 + d2;
    r = 1;
    for ( ; n > d1; --n )
    {
      if ( r > std::numeric_limits<UInt>::max() / n )
        throw std::overflow_error( "overflow in count_each_circular_permutation" );
      r *= n;
    }
    for ( --n; n > d2; --n )
    {
      if ( r > std::numeric_limits<UInt>::max() / n )
        throw std::overflow_error( "overflow in count_each_circular_permutation" );
      r *= n;
    }
  }
  return r;
}

template<class BidirIter>
std::uintmax_t
count_each_circular_permutation( BidirIter first, BidirIter mid, BidirIter last )
{
  return count_each_circular_permutation<std::uintmax_t>( std::distance( first, mid ), std::distance( mid, last ) );
}

namespace detail
{

// Difficult!!!  See notes for operator().
template<class Function, class Size>
class reversible_permutation
{
  Function f_;
  Size s_;

public:
  reversible_permutation( Function f, Size s ) : f_( f ), s_( s ) {}

  template<class BidirIter>
  bool
  operator()( BidirIter first, BidirIter last );
};

// rev1 looks like call_permute
template<class Function, class BidirIter>
class rev1
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;

  Function f_;
  BidirIter first1_;
  BidirIter last1_;
  D d1_;

public:
  rev1( Function f, BidirIter first, BidirIter last, D d )
      : f_( f ), first1_( first ), last1_( last ), d1_( d ) {}

  bool operator()()
  {
    return permute( first1_, last1_, d1_, f_ );
  }
};

// For each permutation in [first1, last1),
//     call f() for each permutation of [first2, last2).
template<class Function, class BidirIter>
class rev2
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;

  Function f_;
  BidirIter first1_;
  BidirIter last1_;
  D d1_;
  BidirIter first2_;
  BidirIter last2_;
  D d2_;

public:
  rev2( Function f, BidirIter first1, BidirIter last1, D d1,
        BidirIter first2, BidirIter last2, D d2 )
      : f_( f ), first1_( first1 ), last1_( last1 ), d1_( d1 ),
        first2_( first2 ), last2_( last2 ), d2_( d2 ) {}

  bool operator()()
  {
    call_permute<Function, BidirIter> f( f_, first2_, last2_, d2_ );
    return permute( first1_, last1_, d1_, f );
  }
};

// For each permutation in [first1, last1),
//     and for each permutation of [first2, last2)
//     call f() for each permutation of [first3, last3).
template<class Function, class BidirIter>
class rev3
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;

  Function f_;
  BidirIter first1_;
  BidirIter last1_;
  D d1_;
  BidirIter first2_;
  BidirIter last2_;
  D d2_;
  BidirIter first3_;
  BidirIter last3_;
  D d3_;

public:
  rev3( Function f, BidirIter first1, BidirIter last1, D d1,
        BidirIter first2, BidirIter last2, D d2,
        BidirIter first3, BidirIter last3, D d3 )
      : f_( f ), first1_( first1 ), last1_( last1 ), d1_( d1 ),
        first2_( first2 ), last2_( last2 ), d2_( d2 ),
        first3_( first3 ), last3_( last3 ), d3_( d3 ) {}

  bool operator()()
  {
    rev2<Function, BidirIter> f( f_, first2_, last2_, d2_, first3_, last3_, d3_ );
    return permute( first1_, last1_, d1_, f );
  }
};

// There are simpler implementations.  I believe the simpler ones are far more
//     expensive.
template<class Function, class Size>
template<class BidirIter>
bool reversible_permutation<Function, Size>::operator()( BidirIter first,
                                                         BidirIter last )
{
  typedef rev2<bound_range<Function&, BidirIter>, BidirIter> F2;
  typedef rev3<bound_range<Function&, BidirIter>, BidirIter> F3;
  // When the range is 0 - 2, then this is just a combination of N out of N
  //   elements.
  if ( s_ < 3 )
    return f_( first, last );
  using std::swap;
  // Hold the first element steady and call f_(first, last) for each
  //    permutation in [first+1, last).
  BidirIter a = std::next( first );
  bound_range<Function&, BidirIter> f( f_, first, last );
  if ( permute( a, last, s_ - 1, f ) )
    return true;
  // Beginning with the first element, swap the previous element with the
  //    next element.  For each swap, call f_(first, last) for each
  //    permutation of the discontinuous range:
  //    [prior to the orignal element] + [after the original element].
  Size s2 = s_ / 2;
  BidirIter am1 = first;
  BidirIter ap1 = std::next( a );
  for ( Size i = 1; i < s2; ++i, ++am1, ++a, ++ap1 )
  {
    swap( *am1, *a );
    F2 f2( f, first, a, i, ap1, last, s_ - i - 1 );
    if ( combine_discontinuous( first, a, i, ap1, last, s_ - i - 1, f2 ) )
      return true;
  }
  // If [first, last) has an even number of elements, then fix it up to the
  //     original permutation.
  if ( 2 * s2 == s_ )
  {
    std::rotate( first, am1, a );
  }
  // else if the range has length 3, we need one more call and the fix is easy.
  else if ( s_ == 3 )
  {
    swap( *am1, *a );
    if ( f_( first, last ) )
      return true;
    swap( *am1, *a );
  }
  // else the range is an odd number greater than 3.  We need to permute
  //     through exactly half of the permuations with the original element in
  //     the middle.
  else
  {
    // swap the original first element into the middle, and hold the current
    //   first element steady.  This creates a discontinuous range:
    //     [first+1, middle) + [middle+1, last).  Run through all permutations
    //     of that discontinuous range.
    swap( *am1, *a );
    BidirIter b = first;
    BidirIter bp1 = std::next( b );
    F2 f2( f, bp1, a, s2 - 1, ap1, last, s_ - s2 - 1 );
    if ( combine_discontinuous( bp1, a, s2 - 1, ap1, last, s_ - s2 - 1, f2 ) )
      return true;
    // Swap the current first element into every place from first+1 to middle-1.
    //   For each location, hold it steady to create the following discontinuous
    //   range (made of 3 ranges): [first, b-1) + [b+1, middle) + [middle+1, last).
    //   For each b in [first+1, middle-1), run through all permutations of
    //      the discontinuous ranges.
    b = bp1;
    ++bp1;
    BidirIter bm1 = first;
    for ( Size i = 1; i < s2 - 1; ++i, ++bm1, ++b, ++bp1 )
    {
      swap( *bm1, *b );
      F3 f3( f, first, b, i, bp1, a, s2 - i - 1, ap1, last, s_ - s2 - 1 );
      if ( combine_discontinuous3( first, b, i, bp1, a, s2 - i - 1, ap1, last, s_ - s2 - 1, f3 ) )
        return true;
    }
    // swap b into into middle-1, creates a discontinuous range:
    //     [first, middle-1) + [middle+1, last).  Run through all permutations
    //     of that discontinuous range.
    swap( *bm1, *b );
    F2 f21( f, first, b, s2 - 1, ap1, last, s_ - s2 - 1 );
    if ( combine_discontinuous( first, b, s2 - 1, ap1, last, s_ - s2 - 1, f21 ) )
      return true;
    // Revert [first, last) to original order
    std::reverse( first, b );
    std::reverse( first, ap1 );
  }
  return false;
}

} // namespace detail

template<class BidirIter, class Function>
Function
for_each_reversible_permutation( BidirIter first,
                                 BidirIter mid,
                                 BidirIter last, Function f )
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;
  for_each_combination( first, mid, last,
                        detail::reversible_permutation<Function&, D>( f,
                                                                      std::distance( first, mid ) ) );
  return std::move( f );
}

template<class UInt>
UInt count_each_reversible_permutation( UInt d1, UInt d2 )
{
  // return d1 > 1 ? (d1+d2)!/(2*d2!) : (d1+d2)!/d2!
  if ( d1 > std::numeric_limits<UInt>::max() - d2 )
    throw std::overflow_error( "overflow in count_each_reversible_permutation" );
  UInt n = d1 + d2;
  UInt r = 1;
  if ( d1 > 1 )
  {
    r = n;
    if ( ( n & 1 ) == 0 )
      r /= 2;
    --n;
    UInt t = n;
    if ( ( t & 1 ) == 0 )
      t /= 2;
    if ( r > std::numeric_limits<UInt>::max() / t )
      throw std::overflow_error( "overflow in count_each_reversible_permutation" );
    r *= t;
    --n;
  }
  for ( ; n > d2; --n )
  {
    if ( r > std::numeric_limits<UInt>::max() / n )
      throw std::overflow_error( "overflow in count_each_reversible_permutation" );
    r *= n;
  }
  return r;
}

template<class BidirIter>
std::uintmax_t
count_each_reversible_permutation( BidirIter first, BidirIter mid, BidirIter last )
{
  return count_each_reversible_permutation<std::uintmax_t>( std::distance( first, mid ), std::distance( mid, last ) );
}

namespace detail
{

// Adapt functor to permute over [first+1, last)
//   A reversible circular permutation of N items is done by holding the first
//   item and reverse-permuting [first+1, last).
template<class Function, class BidirIter>
class reverse_circular_permutation
{
  typedef typename std::iterator_traits<BidirIter>::difference_type D;

  Function f_;
  D s_;

public:
  explicit reverse_circular_permutation( Function f, D s ) : f_( f ), s_( s ) {}

  bool
  operator()( BidirIter first, BidirIter last )
  {
    if ( s_ == 1 )
      return f_( first, last );
    typedef typename std::iterator_traits<BidirIter>::difference_type D;
    typedef bound_range<Function, BidirIter> BoundFunc;
    BoundFunc f( f_, first, last );
    BidirIter n = std::next( first );
    return reversible_permutation<BoundFunc, D>( f, std::distance( n, last ) )( n, last );
  }
};

} // namespace detail

template<class BidirIter, class Function>
Function
for_each_reversible_circular_permutation( BidirIter first,
                                          BidirIter mid,
                                          BidirIter last, Function f )
{
  for_each_combination( first, mid, last, detail::reverse_circular_permutation<Function&, BidirIter>( f, std::distance( first, mid ) ) );
  return std::move( f );
}

template<class UInt>
UInt count_each_reversible_circular_permutation( UInt d1, UInt d2 )
{
  // return d1 == 0 ? 1 : d1 <= 2 ? (d1+d2)!/(d1*d2!) : (d1+d2)!/(2*d1*d2!)
  UInt r;
  try
  {
    r = count_each_combination( d1, d2 );
  }
  catch ( const std::overflow_error& )
  {
    throw std::overflow_error( "overflow in count_each_reversible_circular_permutation" );
  }
  if ( d1 > 3 )
  {
    for ( --d1; d1 > 2; --d1 )
    {
      if ( r > std::numeric_limits<UInt>::max() / d1 )
        throw std::overflow_error( "overflow in count_each_reversible_circular_permutation" );
      r *= d1;
    }
  }
  return r;
}

template<class BidirIter>
std::uintmax_t
count_each_reversible_circular_permutation( BidirIter first, BidirIter mid,
                                            BidirIter last )
{
  return count_each_reversible_circular_permutation<std::uintmax_t>( std::distance( first, mid ), std::distance( mid, last ) );
}

} // namespace unofficial
} // namespace boost
