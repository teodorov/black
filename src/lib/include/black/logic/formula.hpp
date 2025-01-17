//
// BLACK - Bounded Ltl sAtisfiability ChecKer
//
// (C) 2019 Nicola Gigante
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

#ifndef BLACK_LOGIC_FORMULA_HPP_
#define BLACK_LOGIC_FORMULA_HPP_

#include <black/support/common.hpp>

#include <optional>
#include <cstdint>

namespace black::internal {
  //
  // Enumeration of known types of formulas.
  // This type is not exported directly from the `black` namespace.
  // Instead, refer to it as formula::type, as declared below.
  //
  enum class formula_type : uint8_t {
    boolean = 1,
    atom,
    // unary formulas
    negation,
    tomorrow,
    w_tomorrow,
    yesterday,
    w_yesterday,
    always,
    eventually,
    once,
    historically,
    // binary formulas
    conjunction,
    disjunction,
    implication,
    iff,
    until,
    release,
    w_until,
    s_release,
    since,
    triggered
  };
}

#include <black/internal/formula/base.hpp>

namespace black::internal
{
  class formula
  {
  public:
    //
    // Constructors. Default construction is forbidden.
    // Formulas can only be created from methods of class `alphabet`,
    // or by combining other formulas
    //
    formula() = delete;
    formula(formula const&) = default;
    formula(formula &&) = default;

    // Equality operators.
    // Two `formula` objects compare equal iff they represent two
    // syntactically identical formulas
    friend bool operator==(formula f1, formula f2);
    friend bool operator!=(formula f1, formula f2);

    // Default assignment operators.
    formula &operator=(formula const&) = default;
    formula &operator=(formula &&) = default;

    // Enumeration of possible types of formulas, as declared above
    using type = black::internal::formula_type;

    // Gets to the type of the represented formula
    type formula_type() const;

    // Gets the alphabet that manages this formula
    alphabet *sigma() const;

    //
    // The to() function converts a formula into a specific kind of
    // formula type. For example:
    //
    // std::optional<tomorrow> xev = formula.to<tomorrow>();
    //
    // The result is empty if the given type is wrong.
    template<typename H>
    std::optional<H> to() const;

    // The is() function asks whether the formula is of a given specific type
    // Given a formula `f`,
    // `f.to<T>()` is guaranteed to be non-empty iff `f.is<T>()` returns true.
    template<typename H>
    bool is() const;

    //
    // Pattern matching function on formulas. See the docs for examples.
    //
    template<typename ...Cases>
    auto match(Cases&&...) const;

    // Hashing function for formulas, for use in hash tables.
    // Used for the implementation of `std::hash<formula>`, rarely needed alone.
    size_t hash() const;

    //
    // Get a lightweight, opaque, printable unique identifier for the formula
    //
    formula_id unique_id() const;

  private:
    class alphabet *_alphabet; // the alphabet the formula comes from
    formula_base *_formula; // concrete object representing the formula

    friend struct formula_base;

    template<typename, typename>
    friend struct handle_base;

  // Public constructor, but for internal use
  public:
    explicit formula(class alphabet *sigma, formula_base *f)
      : _alphabet{sigma}, _formula{f} { black_assert(f != nullptr); }
  };

  /*
   * handles for specific types of formulas
   */
  struct boolean : handle_base<boolean, boolean_t>
  {
    // inheriting base class constructors (for internal use)
    using handle_base<boolean, boolean_t>::handle_base;

    // The boolean value
    // (true for alphabet::top(), false for alphabet::bottom())
    bool value() const { return _formula->value; }
  };

  struct atom : handle_base<atom, atom_t>
  {
    // inheriting base class constructors (for internal use)
    using handle_base<atom, atom_t>::handle_base;

    // The generic label of the atom
    std::any label() const;

    // The label of the atom, as a specific type
    // The result is empty if the type is wrong.
    template<typename T>
    std::optional<T> label() const;
  };

  struct unary : handle_base<unary, unary_t>
  {
    // inheriting base class constructors (for internal use)
    using handle_base<unary, unary_t>::handle_base;

    // enumeration of possible unary operators
    enum class type : uint8_t {
      negation     = to_underlying(formula::type::negation),
      tomorrow,
      w_tomorrow,
      yesterday,
      w_yesterday,
      always,
      eventually,
      once,
      historically
    };

    // Constructor: takes the type of operator and the argument formula
    unary(type t, formula f);

    // Actual type of unary operator represented by the object
    enum type formula_type() const;

    // Argument of the operator
    formula operand() const;

    //
    // Pattern matching function specific for unary formulas
    //
    template<typename ...Cases>
    auto match(Cases&&...) const;
  };

  struct binary : handle_base<binary, binary_t>
  {
    // inheriting base class constructors (for internal use)
    using handle_base<binary, binary_t>::handle_base;

    // enumeration of possible binary operators
    enum class type : uint8_t {
      conjunction = to_underlying(formula::type::conjunction),
      disjunction,
      implication,
      iff,
      until,
      release,
      w_until,
      s_release,
      since,
      triggered
    };

    // Constructor: takes the type of operator and the two argument formulas
    binary(type t, formula f1, formula f2);

    // Actual type of binary operator represented by the object
    type formula_type() const;

    // Left hand side of the binary operator
    formula left() const;

    // Right hand side of the binary operator
    formula right() const;

    //
    // Pattern matching function specific for binary formulas
    //
    template<typename ...Cases>
    auto match(Cases&&...) const;
  };

  //
  // Specific types for each single type of supported operator,
  // with statically known type of operator.
  //
  // They have the exact same public interface as,
  // respectively, the `unary` and `binary` types declared above.
  //

  // Unary operators. Same public interface as struct `unary`
  struct negation;
  struct tomorrow;
  struct w_tomorrow;
  struct yesterday;
  struct w_yesterday;
  struct always;
  struct eventually;
  struct once;
  struct historically;

  // Binary operators. Same public interface as struct `binary`
  struct conjunction;
  struct disjunction;
  struct implication;
  struct iff;
  struct until;
  struct release;
  struct w_until;
  struct s_release;
  struct since;
  struct triggered;

  //
  // Operators.
  // These provides for a natural syntax for building formulas.
  //
  // Besides the proper operator, we also provide a number of helper functions
  // that, similarly to prefix operators, can be use as nice shorthands to build
  // formulas.
  //
  // Note that, despite the very short names, there is no risk of name clashes
  // with user code, because these names are *not* exported by the `black`
  // namespace, but are found by argument-dependent lookup.
  //
  negation operator !(formula f);
  conjunction operator &&(formula f1, formula f2);
  disjunction operator ||(formula f1, formula f2);
  implication implies(formula f1, formula f2);

  tomorrow     X(formula f);
  w_tomorrow  wX(formula f);
  yesterday    Y(formula f);
  w_yesterday  Z(formula f);
  eventually   F(formula f);
  always       G(formula f);
  once         O(formula f);
  historically H(formula f);

  until     U(formula f1, formula f2);
  release   R(formula f1, formula f2);
  w_until   W(formula f1, formula f2);
  s_release M(formula f1, formula f2);
  since     S(formula f1, formula f2);
  triggered T(formula f1, formula f2);

  tomorrow   XF(formula f);
  tomorrow   XG(formula f);
  eventually FG(formula f);
  always     GF(formula f);
  yesterday  YP(formula f);
  yesterday  YH(formula f);

  //
  // Utility functions
  //

  // Simplifies the formula in some simple ways.
  // Currently, it only removes true/false constants, where possible.
  // This function acts on the *top-level* operator.
  // Use simplify_deep() to recursively simplify the whole formula
  BLACK_EXPORT
  formula simplify(formula f);

  BLACK_EXPORT
  formula simplify_deep(formula f);

  // true if there is any true/false constant in the formula
  BLACK_EXPORT
  bool has_constants(formula f);

  // Conjunct multiple formulas generated from a range
  template<typename Iterator, typename EndIterator, typename F>
  formula big_and(alphabet &sigma, Iterator b, EndIterator e, F&& f);

  template<typename Range, typename F>
  formula big_and(alphabet &sigma, Range r, F&& f);

  // Disjunct multiple formulas generated from a range
  template<typename Iterator, typename EndIterator, typename F>
  formula big_or(alphabet &sigma, Iterator b, EndIterator e, F&& f);

  template<typename Range, typename F>
  formula big_or(alphabet &sigma, Range r, F&& f);

  //
  // Matchers that do not correspond to concrete formula types
  //
  struct big_conjunction;
  struct big_disjunction;
  struct propositional;
  struct temporal;
  struct future;
  struct past;
}

// Names exported from the `black` namespace
namespace black {
  using internal::boolean;
  using internal::atom;
  using internal::unary;
  using internal::binary;
  using internal::formula;
  using internal::formula_id;
  using internal::otherwise;

  using internal::simplify;
  using internal::simplify_deep;
  using internal::has_constants;

  using internal::big_and;
  using internal::big_or;

  using internal::big_conjunction;
  using internal::big_disjunction;
  using internal::propositional;
  using internal::temporal;
  using internal::future;
  using internal::past;
}

#include <black/internal/formula/impl.hpp>
#include <black/logic/alphabet.hpp>

#endif // BLACK_LOGIC_FORMULA_HPP_
