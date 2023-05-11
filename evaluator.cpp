#include "evaluator.h"
#include "builtin.h"
#include "reader.h"

static bool Gel_in_debugger = false;

void env_set(const lref& env, const lref& key, const lref& value) {
  map_set(car(env), key, value);
}

void global_env_set(const lref& key, const lref& value) {
  env_set(current_env, key, value);
}

lref env_find(const lref& key, const lref& env_cons) {
  auto env = car(env_cons);

  if (map_contains(env, key)) {
    return env;
  }
  
  auto next = cdr(env_cons);
  if (next == Nil) {
    return nullptr;  // TODO: Fix this BS so I can do lisp stuff with it
  }
  
  return env_find(key, next);
}

lref env_get(const lref& env, const lref& key) {
  auto found_env = env_find(key, env);
  return found_env != nullptr ? map_get(found_env, key) : nullptr;
}

lref eval(lref env, lref input, const lref& callstack);
lref eval_ast(const lref& env, const lref& ast, const lref& callstack) {
  if (ast == Nil) {
    return ast;
  }

  auto sym = std::dynamic_pointer_cast<const Symbol>(ast).get();
  if (sym) {
    auto value = env_get(env, ast);
    if (value == nullptr) {
      throw eval_error("Value " + sym->name + " not in symbol table.");
    }
    return value;
  }

  auto _cons = std::dynamic_pointer_cast<Cons>(ast).get();
  if (_cons) {
    return mapcar([env, callstack](lref args){ return eval(env, car(args), callstack); }, ast);
  }

  return ast;
}

// see https://github.com/kanaka/mal/blob/master/process/guide.md#step7
lref quasiquote(const lref& ast) {
  auto as_cons = std::dynamic_pointer_cast<Cons>(ast).get();
  if (as_cons == nullptr) {
    return cons(std::make_shared<Symbol>("quote"), cons(ast, Nil));
  }

  auto car_as_sym = std::dynamic_pointer_cast<Symbol>(as_cons->car).get();
  if (car_as_sym != nullptr && car_as_sym->name == "unquote") {
    return car(as_cons->cdr);
  }

  auto res = Nil;

  // fuck
  for (auto econs = reversed(ast); econs != Nil; econs = cdr(econs)) {
    auto elt = car(econs);

    auto elt_as_cons = std::dynamic_pointer_cast<Cons>(elt).get();
    if (elt_as_cons != nullptr) {
      auto elt_car_as_sym = std::dynamic_pointer_cast<Symbol>(elt_as_cons->car).get();
      if (elt_car_as_sym != nullptr && elt_car_as_sym->name == "splice-unquote") {
        // So _that_ was why I wrote "fuck" here
        // fuck
        res = cons(std::make_shared<Symbol>("concat"),
                   cons(car(elt_as_cons->cdr),
                        cons(res, Nil)));
      } else {
        res = cons(std::make_shared<Symbol>("cons"),
                   cons(quasiquote(elt),
                        cons(res, Nil)));
      }
    } else {
      //fuck
      res = cons(std::make_shared<Symbol>("cons"),
                  cons(quasiquote(elt),
                      cons(res, Nil)));
    }
  }

  return res;
}

lref bind_without_evaluating(lref func, lref args, lref env) {
  auto fn_return = std::dynamic_pointer_cast<FnReturn>(func).get();
  if (fn_return == nullptr) {
    throw eval_error("Bad argument to bind_without_evaluating: " + try_repr(func)
                     + "Can't apply something that isn't a function. Also can't apply builtins.");
  }

  auto new_env = std::make_shared<Map>();
  env = cons(new_env, env);

  lref current_binding = fn_return->params;
  lref current_arg = args;
  for (;; current_binding = cdr(current_binding), current_arg = cdr(current_arg)) {
    if (current_binding == Nil && current_arg != Nil) {
      throw eval_error("Too many arguments to function " + try_repr(func)\
                       + ": " + try_repr(args));
    }
    if (current_binding != Nil && current_arg == Nil) {
      auto as_sym = std::dynamic_pointer_cast<Symbol>(car(current_binding)).get();
      if (as_sym != nullptr && as_sym->name == "&rest") {
        env_set(env, cadr(current_binding), current_arg);
        return env;
      }

      throw eval_error("Too few arguments to function: " + try_repr(func)\
                       + ": " + try_repr(args));
    }
    if (current_binding == Nil) {
      break;
    }

    auto as_sym = std::dynamic_pointer_cast<Symbol>(car(current_binding)).get();
    // If this is not a symbol the error will be caught elsewhere
    if (as_sym != nullptr && as_sym->name == "&rest") {
      env_set(env, cadr(current_binding), current_arg);
      return env;
    }

    env_set(env, car(current_binding), car(current_arg));
  }

  return env;
}

lref apply(const lref& func, const lref& args, lref env) {
  auto fn_return = std::dynamic_pointer_cast<FnReturn>(func).get();
  if (fn_return == nullptr) {
    throw eval_error("Bad argument to apply: " + try_repr(func)
                     + "Can't apply something that isn't a function. Also can't apply builtins.");
  }

  env = bind_without_evaluating(func, args, env);
  auto evald = eval_ast(env, fn_return->body, Nil);
  return last(evald);
}

bool is_macro_call(const lref& ast, const lref& env) {
  auto as_cons = std::dynamic_pointer_cast<Cons>(ast).get();
  if (as_cons == nullptr) {
    return false;
  }

  auto car_as_sym = std::dynamic_pointer_cast<Symbol>(as_cons->car).get();
  if (car_as_sym == nullptr) {
    return false;
  }

  auto sym_value = env_get(env, as_cons->car);
  if (sym_value == nullptr) {
    return false;
  }

  auto val_as_fn = std::dynamic_pointer_cast<ILispFunction>(sym_value).get();
  if (val_as_fn == nullptr) {
    return false;
  }

  return val_as_fn->is_macro;
}

lref macroexpand(lref ast, const lref& env) {
  while (is_macro_call(ast, env)) {
    ast = apply(env_get(env, car(ast)), cdr(ast), env);
  }

  return ast;
}

lref eval(lref env, lref input, const lref& old_callstack) {
  auto new_callstack = cons(input, old_callstack);

  while (true) {
    if (input.get() == nullptr) {
      // If there's nothing left to evaluate, quit
      std::cout << "bye" << std::endl;
      exit(0);
    }

    if (Gel_in_debugger) {
      std::cout << input->repr() << std::endl;

      std::string inp;
      std::cout << "geldb λ ";
      getline(std::cin, inp);
      if (inp == "c") {
        std::cout << "Resuming..." << std::endl;
        Gel_in_debugger = false;
        return Nil;
      }

      if (inp == "bt") {
        for (lref cursor = new_callstack; cursor != Nil; cursor = cdr(cursor)) {
          auto rep = car(cursor)->repr();
          if (rep.size() > 96) {
            rep = rep.substr(0, 96) + "...";
          }

          std::cout << rep << std::endl;
        }
        continue;
      }

      if (inp != "s") {
        try {
          Gel_in_debugger = false;
          std::cout << eval(env, read(inp.c_str()))->repr() << std::endl;
          Gel_in_debugger = true;
        } catch (const lisp_error& e) {
          auto val = e.value.get();
          if (val == nullptr) {
              std::cout << "Unknown error. Value of lisp_error was Nil."
                          << " This should never happen."
                          << std::endl;
              return Nil;
          }

          std::cout << "Unhandled error: "
                      << val->repr()
                      << std::endl
                      << e.stack_trace.get()->str();
        }
        continue;
      }
      // Otherwise, fallthrough
    }

    if (input == Nil) {
      return input;
    }

    // If we didn't get a cons, just eval it
    if (std::dynamic_pointer_cast<Cons>(input).get() == nullptr) {
      return eval_ast(env, input, new_callstack);
    }

    input = macroexpand(input, env);

    // Check if macroexpand returned a cons
    if (std::dynamic_pointer_cast<Cons>(input).get() == nullptr) {
      return eval_ast(env, input, new_callstack);
    }

    auto fname = car(input);
    auto args = cdr(input);

    auto special_symbol = std::dynamic_pointer_cast<Symbol>(fname).get();
    if (special_symbol != nullptr) {
      if (special_symbol->name == "break") {
        Gel_in_debugger = true;
        input = Nil;
        continue;
      }

      if (special_symbol->name == "env") {
        return env;
      }

      if (special_symbol->name == "set") {
        check_num_args(args, 2);
        auto l_env = env_find(car(args), env);
        if (l_env == nullptr) {
          throw eval_error("Symbol " + try_repr(car(args)) + " not found.");
        }

        map_set(l_env, car(args), eval(env, cadr(args), new_callstack));
        input = cadr(args);
        continue;
      }

      if (special_symbol->name == "if") {
        check_num_args(args, 3);

        auto condition = eval(env, car(args), new_callstack);
        input = (condition == Nil || condition == False) ? cadr(cdr(args)) : cadr(args);
        continue;
      }

      if (special_symbol->name == "fn") {
        auto bindings = car(args);
        auto body = cdr(args);
        return std::make_shared<FnReturn>(body, bindings, env);
      }

      if (special_symbol->name == "quote") {
        check_num_args(args, 1);
        return car(args);
      }

      if (special_symbol->name == "quasiquote") {
        check_num_args(args, 1);
        input = quasiquote(car(args));
        continue;
      }

      if (special_symbol->name == "macroexpand") {
        check_num_args(args, 1);
        return macroexpand(car(args), env);
      }

      // (try A B C)
      if (special_symbol->name == "try") {
        check_num_args(args, 3);
        try {
          return eval(env, car(args), new_callstack);
        } catch (const lisp_error &e) {
          auto catch_form = cdr(args);

          // Make a new env that binds B to the exception
          auto new_env = std::make_shared<Map>();
          env = cons(new_env, env);
          env_set(env, std::dynamic_pointer_cast<Symbol>(car(catch_form)), e.value);

          input = cadr(catch_form);
          continue;
        }
      }

      // Same as normal evaluation but with a list
      if (special_symbol->name == "apply") {
        check_num_args(args, 2);
        input = cons(car(args), eval(env, cadr(args), new_callstack));
        // Intentional fallthrough
      }
    }

    // If it wasn't a special form, eval it normally
    auto evald = eval_ast(env, input, new_callstack);

    // If the evaluation didn't result in a cons, just return the result
    auto _cons = std::dynamic_pointer_cast<Cons>(evald).get();
    if (_cons == nullptr) {
      return evald;
    }

    // If it was a list, use the first argument of the eval'd list as a function
    // and call it using the rest as the args

    // If the first argument is an FnReturn, use that
    auto fn_return = std::dynamic_pointer_cast<FnReturn>(_cons->car).get();
    if (fn_return != nullptr) {
      // Set up a new env using the bindings
      env = bind_without_evaluating(_cons->car, cdr(evald), env);

      // Eval the body of the function
      if (cdr(fn_return->body) != Nil) {
        eval_ast(env, butlast(fn_return->body), new_callstack);
      }
      input = last(fn_return->body);
      continue;
    }
    
    auto function = std::dynamic_pointer_cast<LispFunction>(_cons->car).get();
    if (function == nullptr) {
      throw eval_error("Failed to eval. First arg is not a function: "
                       + try_repr(_cons->car));
    }
    // TODO: handle improper list
    return function->value(cdr(evald));
  }
}

lref eval(lref env, lref input) {
  return eval(env, input, Nil);
}
