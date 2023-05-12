#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "types.h"

class eval_error : public lisp_error { using lisp_error::lisp_error; };

lref eval_toplevel(lref env, lref input);
lref eval(lref env, lref input, const lref& callstack);
lref apply(const lref& func, const lref& args, lref env, const lref& callstack);

void global_env_set(const lref& key, const lref& value);
lref env_get(const lref& env, const lref& key);

// Structure that allows doing TCO with lref functions
// TODO: think of a better name for this
struct FnReturn : ILispFunction {
  lref body;
  lref params;
  lref env;

  FnReturn(lref body, lref params, lref env): body(body), params(params), env(env) {}

  std::string repr() const {
    return "<function " + name + " " + params->repr() + " " + body->repr()
      + ">";
  }
  std::string type_string() const { return "function"; }
};

#endif
