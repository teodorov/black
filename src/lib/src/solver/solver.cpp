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

#include <black/logic/parser.hpp>
#include <black/solver/solver.hpp>
#include <black/sat/mathsat.hpp>

namespace black::internal
{
  /*
   * Incremental version of 'solve'
   */
  bool solver::inc_solve()
  {
    int k=0;

    msat_env env = _alpha.mathsat_env();
    msat_reset_env(env);

    while(true){
      // Generating the k-unraveling
      add_to_msat(k_unraveling(k));
      // if 'encoding' is unsat, then stop with UNSAT.
      if(!is_sat())
        return false;

      // else, continue to check EMPTY and LOOP.
      // Generating EMPTY and LOOP
      msat_push_backtrack_point(env);
      add_to_msat(empty_and_loop(k));
      // if 'encoding' is sat, then stop with SAT.
      if(is_sat())
        return true;

      // else, generate the PRUNE
      // Computing allSAT of 'encoding & PRUNE^k'
      msat_pop_backtrack_point(env);
      formula all_models = all_sat(_alpha.bottom());
      // Fixing the negation of 'all_models'
      add_to_msat( !all_models );
      // Incrementing 'k' for the next iteration
      k++;
    } // end while(true)
  }


  /*
   * Main algorithm (allSAT-based) for the solver.
   */
  bool solver::solve()
  {
    int k=0;
    formula encoding = _alpha.top();
    while(true){
      // Generating the k-unraveling
      if(k)
        encoding = encoding && k_unraveling(k);
      else // first iteration
        encoding = k_unraveling(k);
      // if 'encoding' is unsat, then stop with UNSAT.
      if(!is_sat(encoding))
        return false;

      // else, continue to check EMPTY and LOOP.
      // Generating EMPTY and LOOP
      formula looped = encoding && empty_and_loop(k);
      // if 'encoding' is sat, then stop with SAT.
      if(is_sat(looped))
        return true;

      // else, generate the PRUNE
      // Computing allSAT of 'encoding & PRUNE^k'
      formula all_models = all_sat(encoding && prune(k), _alpha.bottom());
      // Fixing the negation of 'all_models'
      encoding = encoding && !all_models;
      // Incrementing 'k' for the next iteration
      k++;
    } // end while(true)
  }

  /*
   * Incremental version of 'bsc_prune'
   */
  bool solver::inc_bsc_prune(std::optional<int> k_max_arg)
  {
    msat_env env = _alpha.mathsat_env();
    msat_reset_env(env);

    int k_max = k_max_arg.value_or(std::numeric_limits<int>::max());

    for(int k=0; k <= k_max; ++k)
    {
      // Generating the k-unraveling
      add_to_msat(k_unraveling(k));
      // if 'encoding' is unsat, then stop with UNSAT.
      if(!is_sat())
        return false;

      // else, continue to check EMPTY and LOOP.
      // Generating EMPTY and LOOP
      msat_push_backtrack_point(env);
      add_to_msat(empty_and_loop(k));
      // if 'encoding' is sat, then stop with SAT.
      if(is_sat())
        return true;
      msat_pop_backtrack_point(env);

      // else, generate the PRUNE
      // Computing allSAT of 'encoding & not PRUNE^k'
      add_to_msat( !prune(k) );
      if(!is_sat())
        return false;
    } // end while(true)

    return false;
  }


  /*
   * BSC augmented with the PRUNE rule.
   */
  bool solver::bsc_prune(int k_max)
  {
    formula encoding = _alpha.top();

    msat_env env = _alpha.mathsat_env();
    msat_reset_env(env);

    for(int k = 0; k <= k_max; ++k)
    {
      if(k)
        encoding = encoding && k_unraveling(k);
      else // first iteration
        encoding = k_unraveling(k);

      // if 'encoding' is unsat, then stop with UNSAT.
      if(!is_sat(encoding))
        return false;

      // else, continue to check EMPTY and LOOP.
      // Generating EMPTY and LOOP
      formula empty = k_empty(k);
      formula loop = k_loop(k);

      // if 'encoding' is sat, then stop with SAT.
      if(is_sat(encoding && empty))
        return true;

      //            to_string(loop));
      if(is_sat(encoding && loop))
        return true;

      // else, generate the PRUNE
      // Computing allSAT of 'encoding & not PRUNE^k'
      formula prune = this->prune(k);
      encoding = encoding && !prune;

      if(!is_sat(encoding))
        return false;
    } // end while(true)

    return false;
  }


  /*
   * Incremental version of BSC.
   */
  bool solver::inc_bsc()
  {
    int k=0;

    msat_env env = _alpha.mathsat_env();
    msat_reset_env(env);

    while(true){
      // Generating the k-unraveling
      add_to_msat(k_unraveling(k));

      if(!is_sat())
        return false;

      // Generating EMPTY and LOOP
      msat_push_backtrack_point(env);
      add_to_msat(empty_and_loop(k));

      // if 'encoding' is sat, then stop with SAT.
      if(is_sat())
        return true;

      msat_pop_backtrack_point(env);
      k++;
    }

  }




  /*
   * Naive (not terminating) algorithm for Bounded
   * Satisfiability Checking.
   */
  bool solver::bsc(int k_max)
  {
    formula encoding = _alpha.top();
    formula loop = _alpha.top();
    int k = 0;

    msat_reset_env(_alpha.mathsat_env());

    for(; k <= k_max; ++k)
    {
      // Generating the k-unraveling
      if(k)
        encoding = encoding && k_unraveling(k);
      else // first iteration
        encoding = k_unraveling(k);

      if(!is_sat(encoding))
        return false;

      // Generating EMPTY and LOOP
      loop = k_loop(k);
      formula looped = encoding && (k_empty(k) || loop);

      // if 'encoding' is sat, then stop with SAT.
      if(is_sat(looped))
        return true;
    }

    return false;
  }


  // Generates the PRUNE encoding
  formula solver::prune(int k)
  {
    formula k_prune = _alpha.bottom();
    for(int l=0; l<k-1; l++) {
      formula k_prune_inner = _alpha.bottom();
      for(int j=l+1; j<k; j++) {
        formula llp = l_to_k_loop(l,j) && l_to_k_loop(j,k) && l_j_k_prune(l,j,k);
        k_prune_inner = k_prune_inner || llp;
      }
      k_prune = k_prune || k_prune_inner;
    }
    return k_prune;
  }


  // Generates the _lPRUNE_j^k encoding
  formula solver::l_j_k_prune(int l, int j, int k) {
    formula prune = _alpha.top();
    for(tomorrow xreq : _xrequests) {
      // If the X-requests is an X-eventuality
      if(auto req = get_xev(xreq); req) {
        // Creating the encoding
        formula first_conj = _alpha.var(std::pair(formula{xreq},k));
        formula inner_impl = _alpha.bottom();
        for(int i=j+1; i<=k; i++) {
          formula xnf_req = to_ground_xnf(*req, i, false);
          inner_impl = inner_impl || xnf_req;
        }
        first_conj = first_conj && inner_impl;
        formula second_conj = _alpha.bottom();
        for(int i=l+1; i<=j; i++) {
          formula xnf_req = to_ground_xnf(*req, i, false);
          second_conj = second_conj || xnf_req;
        }
        prune = prune && then(first_conj, second_conj);
      }
    }
    return prune;
  }


  // Generates the EMPTY and LOOP encoding
  formula solver::empty_and_loop(int k) {
    return k_empty(k) || k_loop(k);
  }



  // Generates the encoding for EMPTY_k
  formula solver::k_empty(int k) {
    formula k_empty = _alpha.top();
    for(auto it = _xrequests.begin(); it != _xrequests.end(); it++) {
      k_empty = k_empty && (!( _alpha.var(std::pair<formula,int>(formula{*it},k)) ));
    }
    return k_empty;
  }

  std::optional<formula> solver::get_xev(tomorrow xreq) {
    return xreq.operand().match(
      [](eventually e) { return std::optional{e.operand()}; },
      [](until u) { return std::optional{u.right()}; },
      [](otherwise) { return std::nullopt; }
    );
  }

  // Generates the encoding for LOOP_k
  formula solver::k_loop(int k) {
    formula k_loop = _alpha.bottom();
    for(int l=0; l<k; l++) {
      k_loop = k_loop || (l_to_k_loop(l,k) && l_to_k_period(l,k));
    }
    return k_loop;
  }


  // Generates the encoding for _lP_k
  formula solver::l_to_k_period(int l, int k) {
    formula period_lk = _alpha.top();
    for(tomorrow xreq : _xrequests) {
      // If the X-requests is an X-eventuality
      if(auto req = get_xev(xreq); req) {
        // Creating the encoding
        formula atom_phi_k = _alpha.var( std::pair(formula{xreq},k) );
        formula body_impl = _alpha.bottom();
        for(int i=l+1; i<=k; i++) {
          formula req_atom_i = to_ground_xnf(*req, i, false);
          body_impl = body_impl || req_atom_i;
        }
        period_lk = period_lk && then(atom_phi_k, body_impl);
      }
    }
    return period_lk;
  }


  // Generates the encoding for _lL_k
  formula solver::l_to_k_loop(int l, int k) {
    formula loop_lk = _alpha.top();
    //for(auto it = _xrequests.begin(); it != _xrequests.end(); it++) {
    for(tomorrow xreq : _xrequests) {
      formula first_atom = _alpha.var( std::pair(formula{xreq},l) );
      formula second_atom = _alpha.var( std::pair(formula{xreq},k) );
      // big and formula
      loop_lk = loop_lk && iff(first_atom,second_atom);
    }
    return loop_lk;
  }


  // Generates the k-unraveling for the given k.
  formula solver::k_unraveling(int k) {
    // Keep the X-requests generated in phase k-1.
    // Clear all the X-requests from the vector
    _xrequests.clear();

    if(k==0)
      return to_ground_xnf(_frm,k,true);

    formula big_and = _alpha.top();
    for(tomorrow xreq : _xclosure) {
      // X(_alpha)_P^{k-1}
      formula left_hand = _alpha.var(std::pair(formula{xreq},k-1));
      // xnf(\_alpha)_P^{k}
      formula right_hand = to_ground_xnf(xreq.operand(),k,true);
      // left_hand IFF right_hand
      big_and = big_and && iff(left_hand, right_hand);
    }
    return big_and;
  }


  // Turns the current formula into Next Normal Form
  formula solver::to_ground_xnf(formula f, int k, bool update) {
    // FIRST: transformation in NNF. SECOND: transformation in xnf
    return f.match(
      // Future Operators
      [&](boolean) { return f; },
      [&](atom)    { return _alpha.var(std::pair(f,k)); },
      [&,this](tomorrow t)   {
        if(update)
          _xrequests.push_back(t);
        return _alpha.var(std::pair(f,k));
      },
      [&](negation n)    {
        return !to_ground_xnf(n.operand(),k, update);
      },
      [&](conjunction c) {
        return to_ground_xnf(c.left(),k,update) 
            && to_ground_xnf(c.right(),k,update);
      },
      [&](disjunction d) {
        return to_ground_xnf(d.left(),k,update) 
            || to_ground_xnf(d.right(),k,update);
      },
      [&](then d) {
        return then(
          to_ground_xnf(d.left(),k,update),
          to_ground_xnf(d.right(),k,update)
        );
      },
      [&](iff d) {
        return iff(
          to_ground_xnf(d.left(),k,update),
          to_ground_xnf(d.right(),k,update)
        );
      },
      [&,this](until u) {
        if(update)
          _xrequests.push_back(X(u));

        return
          to_ground_xnf(u.right(),k,update) ||
            (to_ground_xnf(u.left(),k,update) &&
              _alpha.var(std::pair(formula{X(u)},k)));
      },
      [&,this](eventually e) {
        if(update)
          _xrequests.push_back(X(e));
        return
          to_ground_xnf(e.operand(),k,update) ||
            _alpha.var(std::pair(formula{X(e)},k));
      },
      [&,this](always a) {
        if(update)
          _xrequests.push_back(X(a));
        return
          to_ground_xnf(a.operand(),k,update) &&
            _alpha.var(std::pair(formula{X(a)},k));
      },
      [&,this](release r) {
        if(update)
          _xrequests.push_back(X(r));
        return  to_ground_xnf(r.right(),k,update)
            && (to_ground_xnf(r.left(),k,update) ||
                _alpha.var(std::pair(formula{X(r)},k)));
      },
      // TODO: past operators
      [&](otherwise) -> formula {
        black_unreachable();
      }
    );
  }

  // Dual operators for use in to_nnf()
  static constexpr unary::type dual(unary::type t) {
    switch(t) {
      case unary::type::negation:
      case unary::type::tomorrow:
      case unary::type::yesterday:
        return t;
      case unary::type::always:
        return unary::type::eventually;
      case unary::type::eventually:
        return unary::type::always;
      case unary::type::past:
        return unary::type::historically;
      case unary::type::historically:
        return unary::type::past;
    }
    black_unreachable();
  }

  static constexpr binary::type dual(binary::type t)
  {
    switch(t) {
      case binary::type::conjunction:
        return binary::type::disjunction;
      case binary::type::disjunction:
        return binary::type::conjunction;
      case binary::type::until:
        return binary::type::release;
      case binary::type::release:
        return binary::type::until;
      case binary::type::since:
        return binary::type::triggered;
      case binary::type::triggered:
        return binary::type::since;
      case binary::type::iff:
      case binary::type::then:
        black_unreachable(); // these two operators do not have simple duals
    }
    black_unreachable();
  }

  // Transformation in NNF
  formula to_nnf(formula f)
  {
    return f.match(
      [](boolean b) { return b; },
      [](atom a)    { return a; },
      // Push the negation down to literals
      [](negation n) {
        return n.operand().match(
          [](boolean b) { return !b; },
          [](atom a)    { return !a; },
          [](negation n2) { // special case for double negation
            return to_nnf(n2.operand());
          },
          [](unary u) {
            return unary(dual(u.formula_type()), to_nnf(!u.operand()));
          },
          [](then d) {
            return to_nnf(d.left()) && to_nnf(! d.right());
          },
          [](iff d) {
            return iff(to_nnf(!d.left()), to_nnf(d.right()));
          },
          [](binary b) {
            return binary(dual(b.formula_type()),
                          to_nnf(!b.left()), to_nnf(!b.right()));
          }
        );
      },
      // other cases: just recurse down the formula
      [&](unary u) {
        return unary(u.formula_type(), to_nnf(u.operand()));
      },
      [](binary b) {
        return binary(b.formula_type(), to_nnf(b.left()), to_nnf(b.right()));
      }
    );
  }

  void solver::add_xclosure(formula f)
  {
    f.match(
      [&](tomorrow t)   { _xclosure.push_back(t); },
      [&](until u)      { _xclosure.push_back(X(u)); },
      [&](release r)    { _xclosure.push_back(X(r)); },
      [&](always a)     { _xclosure.push_back(X(a)); },
      [&](eventually e) { _xclosure.push_back(X(e)); },
      [](otherwise)     { }
    );

    f.match(
      [&](unary u) {
        add_xclosure(u.operand());
      },
      [&](binary b) {
        add_xclosure(b.left());
        add_xclosure(b.right());
      },
      [](otherwise) { }
    );
  }

  // Asks MathSAT for the satisfiability of current formula
  bool solver::is_sat(formula encoding)
  {
    msat_env env = _alpha.mathsat_env();

    msat_term term = encoding.to_sat();

    msat_result res;
    //msat_push_backtrack_point(env);
    msat_assert_formula(env, term);
    res = msat_solve(env);
    // if(res == MSAT_SAT)
    //   print_mathsat_model(_alpha);
    msat_reset_env(env);

    return (res == MSAT_SAT);
  }


  // Asks MathSAT for the satisfiability of current formula
  bool solver::is_sat()
  {
    msat_result res = msat_solve(_alpha.mathsat_env());
    return (res == MSAT_SAT);
  }



  // Asks MathSAT for a model (if any) of current formula
  // The result is given as a cube.
  formula solver::get_model(formula) {
    formula mdl = _alpha.top();
    return mdl;
  }


  // Incremental version of 'get_model'.
  formula solver::get_model() {
    formula mdl = _alpha.top();
    return mdl;
  }


  // Simple implementation of an allSAT solver.
  formula solver::all_sat(formula f, formula models) {
    if(is_sat(f)){
      formula sigma = get_model(f);
      return all_sat(f && !sigma, models || sigma);
    }
    return models;
  }


  // Incremental version of 'all_sat()'
  formula solver::all_sat(formula models) {
    if(is_sat()) {
      formula sigma = get_model();
      add_to_msat(!sigma);
      return all_sat(models || sigma);
    }
    return models;
  }


  void solver::add_to_msat(formula f)
  {
    msat_env env = _alpha.mathsat_env();
    msat_assert_formula(env, f.to_sat());
  }



} // end namespace black::internal
