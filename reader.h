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
      this->filename = "";
    }

    Reader(const std::vector<std::string> input, const char* const filename) {
      this->input = input;
      this->filename = filename;
    }

    const char* next();
    const char* peek();

    std::string filename;
  private:
    std::vector<std::string> input;
    size_t idx = 0;
};

lref read(const char* const input);
lref read(const char* const input, const std::string& filename);

// For the debugger
struct Linum {
  std::string filename;
  int line;
};
extern std::unordered_map<unsigned long, Linum> line_table;

#endif
