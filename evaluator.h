#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "types.h"

class eval_error : public lisp_error { using lisp_error::lisp_error; };

lref eval(lref env, lref input);
lref apply(const lref& func, const lref& args, lref env);

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
};

#endif
