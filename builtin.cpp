#include <fstream>
#include <tuple>

#include "builtin.h"
#include "evaluator.h"
#include "repl.h"
#include "reader.h"
#include "vm.h"

void check_num_args(const lref& arglist, int size) {
  if (len(arglist) != size) {
    throw eval_error("Wrong number of arguments: "
                     + try_repr(arglist) + ", expected "
                     + std::to_string(size));
  }
}

lref next(lref& lst) {
  lref ret = car(lst);
  lst = cdr(lst);
  return ret;
}

template<typename base_type>
lref to_lisp(base_type);

template<typename base_type>
base_type from_lisp(lref arg);

template<>
lref to_lisp(int i) {
  return std::make_shared<LispInt>(i);
}

template<>
int from_lisp(lref arg) {
  auto ret = std::dynamic_pointer_cast<LispInt>(arg);
  if (ret.get() == nullptr) {
    throw eval_error("Failed conversion.");
  }
  return ret.get()->val;
}

template<typename return_type, typename... template_args>
LispFunction* wrap_func (std::function<return_type(template_args...)> wrapped_func) {
  return new LispFunction([wrapped_func](lref args) -> lref {
    const int n_args = sizeof...(template_args);
    check_num_args(args, n_args);
    std::tuple<template_args...> to_pass = { (from_lisp<template_args>(next(args)))... };
    return to_lisp(std::apply(wrapped_func, to_pass));
  });
}

LispFunction* plus = new LispFunction([](lref args) -> lref {
  LispInt sum = 0;

  while(args != Nil) {
    auto rhs = std::dynamic_pointer_cast<LispInt>(car(args)).get();
    if (rhs == nullptr) {
      throw lisp_error("Argument to + is not an int: " + try_repr(car(args)));
    }
    sum += *rhs;
    args = cdr(args);
  }

  return std::make_shared<LispInt>(sum);
});

LispFunction* minus = new LispFunction([](lref args) -> lref {
  LispInt sum = 0;

  sum += *((LispInt*)(car(args).get()));
  args = cdr(args);

  while(args != Nil) {
    auto rhs = std::dynamic_pointer_cast<LispInt>(car(args)).get();
    if (rhs == nullptr) {
      throw lisp_error("Argument to - is not an int: " + try_repr(car(args)));
    }
    sum -= *rhs;
    args = cdr(args);
  }

  return std::make_shared<LispInt>(sum);
});

LispFunction* int_divide = new LispFunction([](lref args) -> lref {
  LispInt sum = *((LispInt*)(car(args).get()));
  args = cdr(args);

  while(args != Nil) {
    auto rhs = std::dynamic_pointer_cast<LispInt>(car(args)).get();
    if (rhs == nullptr) {
      throw lisp_error("Argument to // is not an int: " + try_repr(car(args)));
    }
    sum /= *rhs;
    args = cdr(args);
  }

  return std::make_shared<LispInt>(sum);
});

LispFunction* mult = new LispFunction([](lref args) -> lref {
  LispInt sum = *((LispInt*)(car(args).get()));
  args = cdr(args);

  while(args != Nil) {
    auto rhs = std::dynamic_pointer_cast<LispInt>(car(args)).get();
    if (rhs == nullptr) {
      throw lisp_error("Argument to * is not an int: " + try_repr(car(args)));
    }
    sum *= *rhs;
    args = cdr(args);
  }

  return std::make_shared<LispInt>(sum);
});

LispFunction* prn = new LispFunction([](lref args) -> lref {
  std::string to_print = "";
  while(args != Nil) {
    to_print += try_str(car(args));
    args = cdr(args);
  }

  std::cout << to_print << std::endl;

  return Nil;
});

LispFunction* _repr = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return std::make_shared<String>(try_repr(car(args)));
});

LispFunction* list = new LispFunction([](lref args) -> lref {
  return args;
});

LispFunction* _cons = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  return cons(car(args), cadr(args));
});

LispFunction* consp = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return std::dynamic_pointer_cast<Cons>(car(args)) ? True : False;
});

LispFunction* emptyp = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return car(args) == Nil ? True : False;
});

LispFunction* _len = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return std::make_shared<LispInt>(len(car(args)));
});

LispFunction* equals = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  return car(args).get()->equals(cadr(args)) ? True : False;
});

LispFunction* lt = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  auto arg1 = std::dynamic_pointer_cast<LispInt>(car(args)).get();
  auto arg2 = std::dynamic_pointer_cast<LispInt>(cadr(args)).get();
  if (arg1 == nullptr || arg2 == nullptr) {
    throw eval_error("Bad argument types: "
                     + try_repr(car(args)) + " " + try_repr(cadr(args)));
  }
  return arg1->lt(cadr(args)) ? True : False;
});

LispFunction* gt = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  auto arg1 = std::dynamic_pointer_cast<LispInt>(car(args)).get();
  auto arg2 = std::dynamic_pointer_cast<LispInt>(cadr(args)).get();
  if (arg1 == nullptr || arg2 == nullptr) {
    throw eval_error("Bad argument types: "
                     + try_repr(car(args)) + " " + try_repr(cadr(args)));
  }
  return arg1->gt(cadr(args)) ? True : False;
});

/*
  (mapcar fn lst)
  Returns the result of applying function fn to each element of list lst.
  Doesn't mutate lst.
*/
SecondOrderLispFunction* _mapcar = new SecondOrderLispFunction([](lref args, const lref& callstack) -> lref {
  check_num_args(args, 2);
  auto func_ref = car(args);
  auto func = std::dynamic_pointer_cast<ILispFunction>(func_ref).get();
  if (func == nullptr) {
    throw eval_error("Bad argument type: First argument should be a function: "
                     + try_repr(car(args)));
  }

  // Have to do some jank, but trust me, this is the least bad way to do it
  // Try to cast the function to an FnReturn (user function).
  auto as_fn_return = std::dynamic_pointer_cast<FnReturn>(func_ref).get();
    
  return mapcar([as_fn_return, func_ref, callstack](lref arg){
    return apply(func_ref, arg,
                 // We don't care about the env if it's a builtin.
                 // In that case, use current_env
                 // Otherwise, use the function's env
                 as_fn_return != nullptr ? as_fn_return->env : current_env,
                 callstack);},
    cadr(args));
});

/*
  Read a string into a lisp form.
*/
LispFunction* read_string = new LispFunction([](lref args) -> lref {
  std::string ret = "";
  while (args != Nil) {
    auto str = std::dynamic_pointer_cast<String>(car(args)).get();
    if (str == nullptr) {
      throw eval_error("Bad argument type: " + try_repr(car(args)));
    }
    ret += str->value;
    args = cdr(args);
  }
  auto ast = read(ret.c_str());
  return ast.get() != nullptr ? ast : Nil;
});

/*
  Read a file into a string.
*/
LispFunction* slurp = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  auto s = std::dynamic_pointer_cast<String>(car(args)).get();

  if (s == nullptr) {
    throw eval_error("Bad argument type: Argument should be a string: "
                     + try_repr(car(args)));
  }
    
  std::ifstream file(s->value);

  if (!file.is_open()) {
    throw lisp_error("File " + s->value + " does not exist.");
  }
    
  // https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
  return std::make_shared<String>(
    std::string((std::istreambuf_iterator<char>(file)),
                (std::istreambuf_iterator<char>())));
});

/*
  Eval evaluates the structure it's given as a lisp form. Unless you want the
  user to be able to execute arbitrary code, you probably don't need to use
  this.

  Looking at the way builtin functions are handled in evaluator.cpp, you may
  notice that this does not respect the enclosing env. For example:

  ? (let* (x 3) (eval '(+ x 2)))
  Value x not in symbol table.

  This is intentional, and consistent with the way this works in other lisps
  (tested on Common Lisp and Emacs Lisp).
*/
SecondOrderLispFunction* _eval = new SecondOrderLispFunction([](lref args, const lref& callstack) -> lref {
  check_num_args(args, 1);
  auto ret = eval(current_env, car(args), callstack);
  // TODO: this is a bad way to handle this
  if (ret == nullptr) {
    throw eval_error("Failed to eval. First arg is not a symbol: "
                     + try_repr(car(args)));
  }
  return ret;
});

// Concatenate two lists.
LispFunction* _concat = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  return concat(car(args), cadr(args));
});

LispFunction* _car = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return car(car(args));
});

LispFunction* _cdr = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return cdr(car(args));
});

LispFunction* _last = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return last(car(args));
});

// Return the last cons of the list (as opposed to the last element).
LispFunction* _tail = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return tail(car(args));
});

LispFunction* _copy_list = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return copy_list(car(args));
});

LispFunction* _hash = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  return std::make_shared<LispInt>(car(args)->hash());
});

LispFunction* make_map = new LispFunction([](lref args) -> lref {
  auto ret = std::make_shared<Map>();
  while(args != Nil) {
    auto key = car(args);
    auto val = cadr(args);
    map_set(ret, key, val);
    args = cddr(args);
  }

  return ret;
});

LispFunction* _map_get = new LispFunction([](lref args) -> lref {
  check_num_args(args, 2);
  return map_get(car(args), cadr(args));
});

LispFunction* _map_set = new LispFunction([](lref args) -> lref {
  check_num_args(args, 3);
  return map_set(car(args), cadr(args), car(cddr(args)));
});

LispFunction* _throw = new LispFunction([](lref args) -> lref {
  check_num_args(args, 1);
  throw lisp_error(car(args));
});


lref repl_env = std::shared_ptr<Map>(new Map({
    {"-def-internal!", new LispFunction([](lref args) -> lref {
      check_num_args(args, 2);

      auto arg1 = std::dynamic_pointer_cast<Symbol>(car(args));
      auto arg2 = cadr(args);
      if (arg1.get() == nullptr || arg2.get() == nullptr) {
        throw eval_error("Bad values passed to def: "
                         + try_repr(arg1) + " " + try_repr(arg2));
      }

      global_env_set(arg1, arg2);
      return arg2;
    })},
    {"-make-macro!", new LispFunction([](lref args) -> lref {
      check_num_args(args, 1);

      auto arg = std::dynamic_pointer_cast<FnReturn>(car(args));
      if (arg.get() == nullptr) {
        throw eval_error("Argument is not a function: "
                         + try_repr(arg));
      }

      if (arg->is_macro) {
        throw eval_error("Argument is already a macro.");
      }

      arg->is_macro = true;
      return arg;
    })},
    {"+", plus},
    {"-", minus},
    {"*", mult},
    {"//", int_divide},
    {"%", new LispFunction([](lref args) -> lref {
      check_num_args(args, 2);
      auto lhs = std::dynamic_pointer_cast<LispInt>(car(args)).get();
      if (lhs == nullptr) {
        throw lisp_error("Argument to % is not an int: " + try_repr(car(args)));
      }

      auto rhs = std::dynamic_pointer_cast<LispInt>(cadr(args)).get();
      if (rhs == nullptr) {
        throw lisp_error("Argument to % is not an int: " + try_repr(cadr(args)));
      }

      return std::make_shared<LispInt>(*lhs % *rhs);
    })},
    {"prn", prn},
    {"put", new LispFunction([](lref args) -> lref {
      std::string to_print = "";
      while(args != Nil) {
        to_print += try_str(car(args));
        args = cdr(args);
      }

      std::cout << to_print;

      return Nil;
    })},
    {"repr", _repr},
    {"list", list},
    {"cons", _cons},
    {"cons?", consp},
    {"empty?", emptyp},
    {"len", _len},
    {"=", equals},
    {"<", lt},
    {">", gt},
    {"mapcar", _mapcar},
    {"read-string", read_string},
    {"slurp", slurp},
    {"eval", _eval},
    {"concat", _concat},
    {"car", _car},
    {"cdr", _cdr},
    {"cadr", new LispFunction([](lref args) -> lref {
      check_num_args(args, 1);
      return cadr(car(args));
    })},
    {"cddr", new LispFunction([](lref args) -> lref {
      check_num_args(args, 1);
      return cddr(car(args));
    })},
    {"last", _last},
    {"tail", _tail},
    {"copy-list", _copy_list},
    {"reversed", new LispFunction([](lref args) -> lref {
      check_num_args(args, 1);
      return reversed(car(args));
    })},
    {"hash", _hash},
    {"make-map", make_map},
    {"map-get", _map_get},
    {"map-set", _map_set},
    {"throw", _throw},
    {"INT_MAX", new LispInt(INT_MAX)},
    {"INT_MIN", new LispInt(INT_MIN)},
    // These have ! in the name to indicate
    // 1) they have side effects, because no one knows wtf rplaca does
    // just by reading the name, and
    // 2) you shouldn't use them
    {"rplaca!", new LispFunction([](lref args) -> lref {
      check_num_args(args, 2);
      return rplaca(car(args), cadr(args));
    })},
    {"rplacd!", new LispFunction([](lref args) -> lref {
      check_num_args(args, 2);
      return rplacd(car(args), cadr(args));
    })},
    {"get-function-name", new LispFunction([](lref args) -> lref {
      check_num_args(args, 1);
      auto f = std::dynamic_pointer_cast<ILispFunction>(car(args));
      if (f == nullptr) {
        throw lisp_error("Argument to get-function-name is not a function.");
      }

      return std::make_shared<String>(f->name);
    })},
    // ! because you shouldn't use this
    {"set-function-name!", new LispFunction([](lref args) -> lref {
      check_num_args(args, 2);
      auto f = std::dynamic_pointer_cast<ILispFunction>(car(args));
      if (f == nullptr) {
        throw lisp_error("First argument to set-function-name is not a function.");
      }

      auto s = std::dynamic_pointer_cast<String>(cadr(args));
      if (s == nullptr) {
        throw lisp_error("Second argument to set-function-name is not a string.");
      }

      f->name = s->value;

      return car(args);
    })},
    {"rand", new LispFunction([](lref args) -> lref {
      return std::make_shared<LispInt>(rand());
    })},
    {"strcat", new LispFunction([](lref args) {
      std::string ret = "";
      while(args != Nil) {
        ret += try_str(car(args));
        args = cdr(args);
      }

      return std::make_shared<String>(ret);
    })},
    {"str=", new LispFunction([](lref args) {
      if (args == Nil || cdr(args) == Nil) return True;
      
      auto last_ptr = std::dynamic_pointer_cast<String>(car(args)).get();
      if (last_ptr == nullptr) {
        return False;
      }

      std::string last_str = last_ptr->value;

      args = cdr(args);

      while (args != Nil) {
        auto cur_ptr = std::dynamic_pointer_cast<String>(car(args)).get();
        if (cur_ptr == nullptr) {
          return False;
        }

        std::string cur_str = cur_ptr->value;
        if (cur_str != last_str) {
          return False;
        }
        args = cdr(args);
      }

      return True;
    })},
    {"type", new LispFunction([](lref args) {
      check_num_args(args, 1);
      auto obj = car(args);
      if (obj == Nil) {
        return std::make_shared<Symbol>("nil-type");
      }
      return std::make_shared<Symbol>(obj->type_string());
    })},
    {"assemble", new LispFunction([](lref args) {
      check_num_args(args, 1);
      return std::make_shared<Bytecode>(assemble(car(args)));
    })},
    {"run-bytecode", new LispFunction([](lref args) {
      check_num_args(args, 1);
      return run_bytecode(car(args));
    })},
    {"is-builtin?", new LispFunction([](lref args) {
      check_num_args(args, 1);
      auto sym = std::dynamic_pointer_cast<Symbol>(car(args));
      if (sym == nullptr) {
        throw lisp_error("Argument is not a symbol.");
      }

      return map_get(repl_env, sym) != Nil ? True : False;
    })},
    {"defined?", new LispFunction([](lref args) {
      check_num_args(args, 1);
      auto sym = std::dynamic_pointer_cast<Symbol>(car(args));
      if (sym == nullptr) {
        throw lisp_error("Argument is not a symbol.");
      }

      return env_get(current_env, sym) != nullptr ? True : False;
    })},
    {"env-get", new LispFunction([](lref args) {
      check_num_args(args, 1);
      auto sym = std::dynamic_pointer_cast<Symbol>(car(args));
      if (sym == nullptr) {
        throw lisp_error("First argument to env-get is not a symbol.");
      }

      // TODO: this is bad, figure out a better way to signal error
      auto val = env_get(current_env, sym);
      if (val == nullptr) {
        throw lisp_error("Key to env-get not in symbol table: " + try_repr(sym));
      }

      return val;
    })},
    {"input", new LispFunction([](lref args) -> lref {
      check_num_args(args, 0);
      if (std::cin.eof()) {
        return Nil;
      }

      std::string inp;
      getline(std::cin, inp);
      return std::make_shared<String>(inp);
    })},
  }));

lref current_env = cons(repl_env, Nil);
