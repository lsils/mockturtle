/* mockturtle: C++ logic network library
 * Copyright (C) 2021  Princeton University
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
  \file event_crtp.hpp
  \brief Safe event API using CRTP

  \author Jinzheng Tu <jinzheng@princeton.edu>
*/

#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

namespace mockturtle
{

/*! \brief Event handler.
 *
 * This data structure holds a weak pointer to the owner of the handler and a
 * function containing the code to be executed.  Note that we are using void *
 * to store arbitrary owner object, so the handler function is responsible for
 * necessary type pointer conversion (mostly reinterpret_cast).
 */
template<typename ... TArgs>
struct event_handler_t
{
  std::weak_ptr<void> ptr;
  std::function<void( void *self, TArgs && ... args )> handler;

  template<typename Func>
    event_handler_t( std::weak_ptr<void> ptr, Func &&handler )
    : ptr{ std::move(ptr) }, handler{ std::forward<Func>(handler) } { }

  bool operator()(TArgs && ... args)
  {
    if ( auto self = ptr.lock(); self )
    {
      handler( self.get(), std::forward<TArgs>(args) ... );
      return true;
    }
    return false;
  }
};

/*! \brief Event handler list.
 *
 * A vector of event handlers. When the owner of any handler is missing, it
 * will be kick out automatically upon a operator() call.
 */
template<typename ... TArgs>
struct event_handlers_t : public std::vector<event_handler_t<TArgs ...>>
{
  void operator()(TArgs && ... args) {
    std::remove_if(
        std::vector<event_handler_t<TArgs ...>>::begin(),
        std::vector<event_handler_t<TArgs ...>>::end(),
        [&]( auto &eh ) {
          return !eh( std::forward<TArgs>(args) ... );
        } );
  }
};

/*! \brief Base CRTP class for all willing to register their event handlers.
 *
 * This CRTP class takes care of generating self pointers soly and it is the
 * derived class's responsibility to enable its copy/move ctor/assignment to
 * default value, i.e.
 *
 *      class T : public Ntk, public event_crtp<T, Accessor> {
 *        public:
 *          T(const T &) = default;
 *          T(T &&) = default;
 *          T &(const T &) = default;
 *          T &(T &&) = default;
 *      }
 *
 * where Accessor should be one of the event_(add|modified|delete)_crtp in
 * the file mockturtle/networks/events.hpp.
 *
 * Note that event_crtp must be initialized AFTER whatever Accessor is going to
 * access. In mostly of the cases, it means that T must inherit Ntk first
 * before inheriting event_crtp.
 *
 * \tparam Derived The owner class
 * \tparam Accessor Callable of event_handlers_t<...>(Derived &owner)
 */
template <typename Derived, typename Accessor>
class event_crtp
{
  [[nodiscard]] decltype(auto) underlying()
  {
    return static_cast<Derived &>(*this);
  }

  /*! \brief Smart pointer to the Derived class.  Deletion is disabled. */
  std::shared_ptr<Derived> _self;

protected:

  event_crtp() : _self{ &underlying(), [](auto){} } { }

  event_crtp( const event_crtp &other ) : _self{ &underlying(), [](auto){} }
  {
    operator=(other);
  }

  event_crtp( event_crtp &&other ) noexcept : _self{ &underlying(), [](auto){} }
  {
    operator=(std::move(other));
  }

  event_crtp &operator=( const event_crtp &other )
  {
    if ( this != &other ) {
      auto &ehs = Accessor{}( underlying() );
      typename std::remove_reference<decltype(ehs)>::type tmp;
      for (auto &h : ehs)
        if (auto lp = h.ptr.lock(); lp == other._self)
          tmp.emplace_back(_self, h.handler);
      for (auto &h : tmp)
        ehs.push_back(h);
    }
    return *this;
  }

  event_crtp &operator=( event_crtp &&other ) noexcept
  {
    if (this != &other) {
      auto &ehs = Accessor{}( underlying() );
      for (auto &h : ehs)
        if (auto lp = h.ptr.lock(); lp == other._self)
          h.ptr = _self;
    }
    return *this;
  }

  /*! \brief Get a weak pointer to self for adding to event handler list. */
  std::weak_ptr<Derived> wp() const { return _self; }
};

} // namespace mockturtle
