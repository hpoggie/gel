#ifndef READER_H
#define READER_H

#include <stdexcept>
#include <vector>

#include "types.h"

class reader_error : public lisp_error {
    using lisp_error::lisp_error;
};

class Reader {
  public:
    Reader(const std::vector<std::string> input) {
      this->input = input;
    }

    const char* next();
    const char* peek();
  private:
    std::vector<std::string> input;
    size_t idx = 0;
};

lref read(const char* const input);

#endif
