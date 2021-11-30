//
// BLACK - Bounded Ltl sAtisfiability ChecKer
//
// (C) 2019 Luca Geatti
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


#include <black/sat/backends/mathsat.hpp>

#include <black/logic/alphabet.hpp>
#include <black/logic/formula.hpp>
#include <black/logic/parser.hpp>

#include <mathsat.h>
#include <fmt/format.h>
#include <tsl/hopscotch_map.h>

#include <string>

BLACK_REGISTER_SAT_BACKEND(mathsat, {black::sat::feature::smt})

namespace black::sat::backends
{
  struct mathsat::_mathsat_t {
    msat_env env;
    tsl::hopscotch_map<formula, msat_term> formulas;
    tsl::hopscotch_map<term_id, msat_term> terms;
    tsl::hopscotch_map<std::string, msat_decl> functions;
    tsl::hopscotch_map<term_id, msat_decl> variables;
    std::optional<msat_model> model;

    msat_term to_mathsat(formula);
    msat_term to_mathsat_inner(formula);
    msat_term to_mathsat(term);
    msat_term to_mathsat_inner(term);
    msat_type to_mathsat(sort);
    msat_decl to_mathsat(alphabet *, std::string const&, int, bool);
    msat_decl to_mathsat(variable);

    ~_mathsat_t() {
      if(model)
        msat_destroy_model(*model);
    }
  };

  mathsat::mathsat() : _data{std::make_unique<_mathsat_t>()}
  {  
    msat_config cfg = msat_create_config();
    msat_set_option(cfg, "model_generation", "true");
    msat_set_option(cfg, "unsat_core_generation","3");
    
    _data->env = msat_create_env(cfg);
  }

  mathsat::~mathsat() { }

  void mathsat::assert_formula(formula f) {
    msat_assert_formula(_data->env, _data->to_mathsat(f));
  }

  bool mathsat::is_sat() { 
    msat_result res = msat_solve(_data->env);

    if(res == MSAT_SAT) {
      if(_data->model)
        msat_destroy_model(*_data->model);

      _data->model = msat_get_model(_data->env);
      black_assert(!MSAT_ERROR_MODEL(*_data->model));
    }

    return (res == MSAT_SAT);
  }

  bool mathsat::is_sat_with(formula f) 
  {
    msat_push_backtrack_point(_data->env);
  
    assert_formula(f);
    msat_result res = msat_solve(_data->env);

    if(res == MSAT_SAT) {
      if(_data->model)
        msat_destroy_model(*_data->model);

      _data->model = msat_get_model(_data->env);
      black_assert(!MSAT_ERROR_MODEL(*_data->model));
    }
  
    msat_pop_backtrack_point(_data->env);
    return (res == MSAT_SAT);
  }

  tribool mathsat::value(proposition a) const {
    auto it = _data->formulas.find(a);
    if(it == _data->formulas.end())
      return tribool::undef;

    if(!_data->model)
      return tribool::undef;
    
    msat_term var = it->second;
    msat_term result = msat_model_eval(*_data->model, var);
    if(msat_term_is_true(_data->env, result))
      return true;
    if(msat_term_is_false(_data->env, result))
      return false;

    return tribool::undef;
  }

  void mathsat::clear() {
    msat_reset_env(_data->env);
  }

  msat_term mathsat::_mathsat_t::to_mathsat(formula f) 
  {
    if(auto it = formulas.find(f); it != formulas.end()) 
      return it->second;

    msat_term term = to_mathsat_inner(f);
    formulas.insert({f, term});

    return term;
  }

  msat_term mathsat::_mathsat_t::to_mathsat_inner(formula f) 
  {
    return f.match(
      [this](boolean b) {
        return b.value() ? 
          msat_make_true(env) : msat_make_false(env);
      },
      [this](atom a) {
        std::vector<msat_term> args;
        for(term t : a.terms())
          args.push_back(to_mathsat(t));
        
        // we know how to encode known relations
        if(auto k = a.rel().known_type(); k) {
          black_assert(z3_terms.size() == 2);
          switch(*k) {
            case relation::equal:
              return msat_make_eq(env, args[0], args[1]);
            case relation::not_equal:
              return msat_make_not(env, msat_make_eq(env, args[0], args[1]));
            case relation::less_than:
              return msat_make_and(env,
                msat_make_leq(env, args[0], args[1]),
                msat_make_not(env, msat_make_eq(env, args[0], args[1]))
              );
            case relation::less_than_equal: 
              return msat_make_leq(env, args[0], args[1]);
            case relation::greater_than:
              return msat_make_not(env, msat_make_leq(env, args[0], args[1]));
            case relation::greater_than_equal:
              return msat_make_or(env,
                msat_make_eq(env, args[0], args[1]),
                msat_make_not(env, msat_make_leq(env, args[0], args[1]))
              );
          }
        }

        msat_decl rel = to_mathsat(a.sigma(), a.rel().name(), args.size(),true);

        return msat_make_term(env, rel, args.data());
      },
      [this](quantifier) -> msat_term {
        black_unreachable(); // mathsat does not support quantifiers
      },
      [this](proposition p) {
        msat_decl msat_prop =
          msat_declare_function(env, to_string(p.unique_id()).c_str(),
          msat_get_bool_type(env));

        return msat_make_constant(env, msat_prop);
      },
      [this](negation n) {
        return msat_make_not(env, to_mathsat(n.operand()));
      },
      [this](big_conjunction c) {
        msat_term acc = msat_make_true(env);

        for(formula op : c.operands())
          acc = msat_make_and(env, acc, to_mathsat(op));

        return acc;
      },
      [this](big_disjunction c) {
        msat_term acc = msat_make_false(env);

        for(formula op : c.operands())
          acc = msat_make_or(env, acc, to_mathsat(op));

        return acc;
      },
      [this](implication t) {
        return
          msat_make_or(env,
            msat_make_not(env, 
              to_mathsat(t.left())), to_mathsat(t.right())
          );
      },
      [this](iff i) {
        return msat_make_iff(env, 
          to_mathsat(i.left()), to_mathsat(i.right()));
      },
      [](temporal) -> msat_term { // LCOV_EXCL_LINE
        black_unreachable(); // LCOV_EXCL_LINE
      }
    );
  }

  msat_type mathsat::_mathsat_t::to_mathsat(sort s) {
    if(s == sort::Int)
      return msat_get_integer_type(env);
    
    return msat_get_rational_type(env);
  }

  msat_decl mathsat::_mathsat_t::to_mathsat(
    alphabet *sigma, std::string const&name, int arity, bool is_relation
  ) {
    black_assert(!f.known_type());
    if(auto it = functions.find(name); it != functions.end())
      return it->second;

    black_assert(sigma->domain());
    msat_type type = to_mathsat(*sigma->domain());
    msat_type bool_type = msat_get_bool_type(env);

    std::vector<msat_type> types(arity, type);

    msat_type functype = msat_get_function_type(
      env, types.data(), arity, is_relation ? bool_type : type
    );
    msat_decl d = msat_declare_function(env, name.c_str(), functype);

    functions.insert({name, d});

    return d;
  }

  msat_decl mathsat::_mathsat_t::to_mathsat(variable x) {
    if(auto it = variables.find(x.unique_id()); it != variables.end())
      return it->second;

    black_assert(x.sigma()->domain());
    msat_type t = to_mathsat(*x.sigma()->domain());

    msat_decl var = 
      msat_declare_function(env, to_string(x.unique_id()).c_str(), t);

    return var;
  }

  msat_term mathsat::_mathsat_t::to_mathsat(term t) 
  {
    if(auto it = terms.find(t.unique_id()); it != terms.end()) 
      return it->second;

    msat_term term = to_mathsat_inner(t);
    terms.insert({t.unique_id(), term});

    return term;
  }
  
  msat_term mathsat::_mathsat_t::to_mathsat_inner(term t) {
    return t.match(
      [&](constant c) {
        if(std::holds_alternative<int>(c.value()))
          return
            msat_make_number(env, 
              std::to_string(std::get<int>(c.value())).c_str());
        else
          return
            msat_make_number(env,
              std::to_string(std::get<double>(c.value())).c_str());
      },
      [&](variable x) {
        return msat_make_constant(env, to_mathsat(x));
      },
      [&](application a) {
        std::vector<msat_term> args;
        for(term t : a.arguments())
          args.push_back(to_mathsat(t));

        black_assert(a.arguments().size() > 0);

        // We know how to encode known functions
        if(auto k = a.func().known_type(); k) {
          switch(*k) {
            case function::negation:
              black_assert(args.size() == 1);
              return msat_make_times(env, msat_make_number(env, "-1"), args[0]);
            case function::subtraction:
              black_assert(args.size() == 2);
              return msat_make_plus(env, 
                args[0],
                msat_make_times(env, msat_make_number(env, "-1"), args[1])
              );
            case function::addition:
              black_assert(args.size() == 2);
              return 
                msat_make_plus(env, args[0], args[1]);
            case function::multiplication:
              black_assert(args.size() == 2);
              return 
                msat_make_times(env, args[0], args[1]);
            case function::division:
              black_assert(args.size() == 2);
              return msat_make_divide(env, args[0], args[1]);
            case function::modulo:
              black_unreachable();
          }
          black_unreachable();
        }

        msat_decl func = to_mathsat(
          a.arguments()[0].sigma(), a.func().name(), args.size(), false
        );
        return msat_make_uf(env, func, args.data());
      },
      [&](next) -> msat_term { black_unreachable(); }
    );
  }

  std::optional<std::string> mathsat::license() const {
    return R"(
MathSAT5 is copyrighted 2009-2020 by Fondazione Bruno Kessler, Trento, Italy, 
University of Trento, Italy, and others. All rights reserved.

MathSAT5 is available for research and evaluation purposes only. It can not be 
used in a commercial environment, particularly as part of a commercial product, 
without written permission. MathSAT5 is provided as is, without any warranty.
)";
  }

}
