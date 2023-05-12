#include "reader.h"
#include "evaluator.h"
#include "builtin.h"

void re(const char* const input) {
  eval_toplevel(current_env, read(input));
}

int main() {
  // Make a new env for user stuff so is-builtin? will work
  current_env = cons(std::make_shared<Map>(), current_env);

  re("(-def-internal! 'progn (fn (&rest forms) (if (empty? forms) nil (last forms))))");
  re("(-def-internal! 'load-file (fn (path) (eval (read-string \"(progn \n\" (slurp path) \"\nnil)\"))))");
  re("(load-file \"boot.gel\")");

  return 0;
}
