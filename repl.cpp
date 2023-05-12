#include "reader.h"
#include "evaluator.h"
#include "repl.h"
#include "builtin.h"

///
/// Printer
///

void pr_str(lref input) {
  input->print();
  std::cout << std::endl;
}

void print(lref input) {
  pr_str(input);
}

void re(const char* const input) {
  eval(current_env, read(input));
}

void rep(const char* const input) {
  print(eval(current_env, read(input)));
}

int main() {
  // Default env
  //re("(def! load-file (fn (path) (eval (read-string \"(progn \n\" (slurp path) \"\nnil)\"))))");
  //re("(load-file \"stdlib.gel\")");

  // Make a new env for user stuff so is-builtin? will work
  current_env = cons(std::make_shared<Map>(), current_env);

  re("(-def-internal! 'progn (fn (&rest forms) (if (empty? forms) nil (last forms))))");
  re("(-def-internal! 'load-file (fn (path) (eval (read-string \"(progn \n\" (slurp path) \"\nnil)\"))))");
  re("(load-file \"boot.gel\")");

  /*
  std::string inp;
  while (!std::cin.eof()) {
    std::cout << "gel> ";
    getline(std::cin, inp);
    try {
      rep(inp.c_str());
    } catch (const lisp_error& e) {
      std::cout << "Unhandled error: " << e.value.get()->repr() << std::endl;
    }
  }
  */

  return 0;
}
