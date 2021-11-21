//
// BLACK - Bounded Ltl sAtisfiability ChecKer
//
// (C) 2021 Nicola Gigante
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef BLACK_LOGIC_TERM_BASE_HPP
#define BLACK_LOGIC_TERM_BASE_HPP

#ifndef BLACK_TERM_HPP
  #error "This header file cannot be included alone, "\
         "please include <black/logic/term.hpp> instead."
#endif

#include <black/support/assert.hpp>
#include <black/support/hash.hpp>

#include <vector>

namespace black::internal
{
  class alphabet;
  class term;

  constexpr bool is_constant_type(term_type type) {
    return type == term_type::constant;
  }

  constexpr bool is_variable_type(term_type type) {
    return type == term_type::variable;
  }

  constexpr bool is_application_type(term_type type) {
    return type == term_type::application;
  }

  constexpr bool is_next_type(term_type type) {
    return type == term_type::next;
  }

  struct term_base
  {
    term_base(term_type t) : type{t} { }

    const term_type type{};
  };

  struct constant_t : term_base 
  {
    static constexpr auto accepts_type = is_constant_type;

    constant_t(int c)
      : term_base{term_type::constant}, value{c} {}

    int value{};
  };

  struct variable_t : term_base
  {
    static constexpr auto accepts_type = is_variable_type;

    variable_t(any_hashable const& _label)
      : term_base{term_type::variable}, label{_label} {}

    any_hashable label;
  };

  struct application_t : term_base
  {
    static constexpr auto accepts_type = is_application_type;

    application_t(function _f, std::vector<term_base *> const&_args)
      : term_base{term_type::application}, f{_f}, args{_args} {}

    function f;
    std::vector<term_base *> args;
  };

  struct next_t : term_base
  {
    static constexpr auto accepts_type = is_next_type;

    next_t(term_base *_arg)
      : term_base{term_type::next}, arg{_arg} {}

    term_base *arg{};
  };

  template<typename U, typename T = std::remove_pointer_t<U>>
  T *term_cast(term_base *t) 
  {
    black_assert(t != nullptr);
    if(T::accepts_type(t->type))
      return static_cast<T *>(t);
    return nullptr;
  }

  enum class term_id : uintptr_t;

  template<typename H, typename T>
  struct term_handle_base
  {
    friend class term;
    friend class alphabet;

    term_handle_base(alphabet *sigma, T *t)
      : _alphabet{sigma}, _term{t}
    { black_assert(_term); }

    term_handle_base(std::pair<alphabet *, T *> args)
      : term_handle_base{args.first, args.second} { }
    
    operator otherwise() const { return {}; }

    operator term() const;

    alphabet *sigma() const;

    term_id unique_id() const;

  protected:
    using handled_term_t = T;

    static std::optional<H> cast(class alphabet *sigma, term_base *t) {
      if(auto ptr = term_cast<typename H::handled_term_t *>(t); ptr)
        return std::optional<H>{H{sigma, ptr}};
      return std::nullopt;
    }

    static std::pair<class alphabet *, application_t *>
    allocate_application(function const&func, std::vector<term> const&args);

    template<typename Arg>
    static std::pair<class alphabet *, next_t *>
    allocate_next(Arg arg);

    class alphabet *_alphabet;
    T *_term;
  };
}

#endif