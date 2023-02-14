#ifndef LISP_FUNCTIONS_H
#define LISP_FUNCTIONS_H

#include "types.h"

extern lref repl_env;
extern lref current_env;
extern LispFunction *plus, *minus;

void check_num_args(const lref& arglist, int size);

#endif
