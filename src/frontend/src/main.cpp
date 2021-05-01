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

#include <black/frontend/cli.hpp>
#include <black/frontend/io.hpp>
#include <black/frontend/support.hpp>

#include <black/logic/formula.hpp>
#include <black/logic/parser.hpp>
#include <black/logic/past_remover.hpp>
#include <black/solver/solver.hpp>

#include <black/sat/dimacs.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <type_traits>
#include <set>

using namespace black::frontend;

int ltl(std::optional<std::string>, std::istream &);
int dimacs(std::optional<std::string> path, std::istream &file);
void print_model(black::solver &solver, black::formula f);
void collect_atoms(black::formula f, std::unordered_set<black::atom> &atoms);

int main(int argc, char **argv)
{
  parse_command_line(argc, argv);

  if(!cli::filename && !cli::formula) {
    command_line_error("please specify a filename or the --formula option");
    quit(status_code::command_line_error);
  }

  if(cli::formula) {
    std::istringstream str{*cli::formula};
    return ltl(std::nullopt, str);
  }

  if(*cli::filename == "-")
    return 
      cli::dimacs ? 
        dimacs(std::nullopt, std::cin) :
        ltl(std::nullopt, std::cin);

  std::ifstream file{*cli::filename, std::ios::in};

  if(!file)
    io::fatal(status_code::filesystem_error,
      "Unable to open file `{}`: {}",
      *cli::filename, system_error_string(errno)
    );

  return cli::dimacs ? dimacs(cli::filename, file) : ltl(cli::filename, file);
}

int ltl(std::optional<std::string> path, std::istream &file)
{
  black::alphabet sigma;

  std::optional<black::formula> f =
    black::parse_formula(sigma, file, [&path](auto error) {
      io::fatal(status_code::syntax_error, 
                "syntax error: {}: {}\n", 
                path ? *path : "<stdin>", error);
    });

  black_assert(f.has_value());

  black::solver slv{sigma};
  
  if (cli::sat_backend)
    slv.set_sat_backend(*cli::sat_backend);

  if (cli::remove_past)
    slv.assert_formula(black::remove_past(*f));
  else
    slv.assert_formula(*f);

  size_t bound = cli::bound ? *cli::bound : std::numeric_limits<size_t>::max();
  black::tribool res = slv.solve(bound);

  if(res == black::tribool::undef) {
    io::message("UNKNOWN (stopped at k = {})", bound);
  } else if (res == true) {
    io::message("SAT");
    if(cli::print_model)
      print_model(slv, *f);
  } else {
    io::message("UNSAT");
  }

  return 0;
}

int dimacs(std::optional<std::string>, std::istream &in) 
{
  using namespace black::sat;
  
  std::optional<dimacs::problem> problem = 
    dimacs::parse(in, [](std::string error) {
      io::message("{}", error);
      exit(1);
    });

  if(!problem) {
    io::message("Parsing problem");
    return (int)status_code::syntax_error;
  }

  std::string backend = cli::sat_backend ? *cli::sat_backend : "z3";
  std::optional<dimacs::solution> s = dimacs::solve(*problem, backend);

  dimacs::print(std::cout, s);

  return 0;
}

void print_model(black::solver &solver, black::formula f) {
  black_assert(solver.model().has_value());

  io::message("Model size: {}", solver.model()->size());
  io::message("Loop at: {}", solver.model()->loop());

  std::unordered_set<black::atom> atoms;
  collect_atoms(f, atoms);

  for(size_t i = 0; i < solver.model()->size(); ++i) {
    io::message("- Time step: {}", i);
    for(black::atom a : atoms) {
      black::tribool v = solver.model()->value(a, i);
      if(v == black::tribool::undef)
        io::message("  -  {} = undef", to_string(a));
      else if(v)
        io::message("  -  {} = true", to_string(a));
      else
        io::message("  -  {} = false", to_string(a));
    }
  }
  
}

void collect_atoms(black::formula f, std::unordered_set<black::atom> &atoms) {
  using namespace black;
  f.match(
    [&](atom a) {
      atoms.insert(a);
    },
    [&](unary, formula f1) {
      collect_atoms(f1, atoms);
    },
    [&](binary, formula f1, formula f2) {
      collect_atoms(f1, atoms);
      collect_atoms(f2, atoms);
    }
  );
}
