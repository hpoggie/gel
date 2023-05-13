#include <limits.h>
#include <regex>

#include "regex.h"
#include "reader.h"

static const lref CloseParen = std::make_shared<LispObject>();
static const lref CloseBrace = std::make_shared<LispObject>();

// Yes, this is godawful. Need to escape backslashes for cpp and then
// AGAIN for regex. Fuck.
// cpp escapes "\\\\\"" to "\\\""
// regex escapes that to "\""
static const auto backslash_quote_regex = std::regex("\\\\\"");

const char* Reader::next() {
  // Returns and consumes the next token

  if (idx >= input.size()) {
    return nullptr;
  }

  return this->input[idx++].c_str();
}

const char* Reader::peek() {
  // Returns the next token but does not consume it.
  if (idx >= input.size()) {
    return nullptr;
  }

  return this->input[idx].c_str();
}

// Splits input into an array of tokens (strings)
std::vector<std::string> tokenize(const char* const input) {
  return match_str(std::string(input));
}

Reader read_str(const char* const input) {
  return Reader(tokenize(input));
}

// String to LispInt
// We can't use stol because we'd need to catch an exception, and we can't use atol
// because it has undefined behavior (UB) in the spec FOR SOME REASON so we roll our
// own
lref stoli(std::string token) {
  int sign = 1;

  if (token[0] == '-') {
    token.erase(0, 1);
    sign = -1;
  }

  LispInt sum = 0;
  // Yes, std::string defines its own size type. This is presumably so they can
  // change it at a later time.
  // I don't know why the C++ committee thought this would be a good idea, since no
  // one is going to know about this unless they look it up on some obscure wiki page,
  // meaning everyone's code is going to break if they actually do end up changing it.
  // Except mine, since apparently I'm a raving paranoiac.
  std::string::size_type size = token.size() - 1;

  // Compute 10^x using repeated squaring
  LispInt mult = 1;
  LispInt base = 10;
  while (size > 0) {
    if ((size & 1) == 1) {
      mult *= base;
    }

    // Don't overflow base if we don't need to
    if (size >> 1 == 0) {
      break;
    }

    base *= base;
    size >>= 1;
  }

  // Surprisingly, it's OK to skip the checks here, since mult is always positive
  // and INT_MIN is guaranteed to be <= -INT_MAX
  // We need to do this first so we can represent lower numbers than -INT_MAX
  mult.val *= sign;

  for (auto c : token) {
    sum += mult * LispInt(c - '0');
    mult /= 10;
  }
 
  return std::make_shared<LispInt>(sum);
}

// Have to forward-declare this because c++ is dumb
lref read_form(Reader* const reader);
lref read_list(Reader* const reader, const lref& end_marker) {
  // Consume the left paren
  reader->next();

  // Put things into the list in left to right order

  lref ret = Nil;
  // Track the end of the list so we don't have to step through it every time
  Cons *tail = nullptr;

  lref form;
  while((form = read_form(reader)) != end_marker) {
    if (form == nullptr) {
      throw reader_error(end_marker == CloseParen ?
                         "Missing closing parenthesis." : "Missing closing brace.");
    }

    if (ret == Nil) {
      ret = cons(form, Nil);
      tail = (Cons*)(ret.get());
    } else {
      tail->cdr = cons(form, Nil);
      tail = (Cons*)(tail->cdr.get());
    }
  }

  if (reader->filename != "") {
    Linum& linum = line_table[(unsigned long)ret.get()];
    linum.filename = reader->filename;
  }
  return ret;
}

lref read_atom(Reader* const reader) {
  auto token = std::string(reader->next());

  if (token.size() == 0) {
    throw reader_error("Empty token. This shouldn't happen.");
  }

  // If the token started with ", we have a string
  if (token[0] == '\"') {
    token.erase(0, 1); // Remove opening quote
    if (token.size() < 1 || token[token.size() - 1] != '\"') {
      throw reader_error("Unbalanced quotation marks.");
    }
    token.erase(token.size() - 1, 1); // Remove closing quote
    // Unescape the string
    token = std::regex_replace(token, backslash_quote_regex, "\"");
    return std::make_shared<String>(token);
  }

  if (token == "nil") {
    return Nil;
  }

  if (token == "true") {
    return True;
  }

  if (token == "false") {
    return False;
  }

  // Slightly hacky way to determine if token is an int literal
  // TODO: make this work properly
  if ((token[0] >= '0' && token[0] <= '9') ||
      (token[0] == '-' && token.size() > 1 && token[1] >= '0' && token[1] <= '9')) {
    return stoli(token);
  }

  return std::make_shared<Symbol>(token);
}

lref read_form(Reader* const reader) {
  auto next = reader->peek();

  if (next == nullptr) {
    return nullptr;
  }

  auto n0 = next[0];
  while (n0 == ';') {
    reader->next();
    next = reader->peek();
    if (next == nullptr) {
      return nullptr;
    }
    n0 = next[0];
  }

  switch(n0) {
    case '(':
      return read_list(reader, CloseParen);
    case ')':
      reader->next();
      return CloseParen;
    case '}':
      reader->next();
      return CloseBrace;
    case '{':
      return cons(std::make_shared<Symbol>("make-map"),
                  read_list(reader, CloseBrace));
    case ';':
      return nullptr;
    case '\'':
    {
      reader->next();  // Consume '
      auto form = read_form(reader);
      if (form == nullptr) {
        throw reader_error("Quote failed. No form to quote.");
      }
      return cons(std::make_shared<Symbol>("quote"), cons(form, Nil));
    }
    case '`':
    {
      reader->next();  // Consume `
      auto form = read_form(reader);
      if (form == nullptr) {
        throw reader_error("Quasiquote failed. No form to quasiquote.");
      }
      return cons(std::make_shared<Symbol>("quasiquote"), cons(form, Nil));
    }
    case ',':
    {
      reader->next();  // Consume ,
      auto form = read_form(reader);
      if (form == nullptr) {
        throw reader_error("Unquote failed. No form to unquote.");
      }
      return cons(std::make_shared<Symbol>(next[1] == '@' ?
                                           "splice-unquote" : "unquote"),
                  cons(form, Nil));
    }
    default:
      return read_atom(reader);
  }
}

lref read_internal(Reader& reader, const char* const input) {
  auto res = read_form(&reader);

  if (res == CloseParen) {
    throw reader_error("Unmatched close parenthesis in: " + std::string(input));
  }

  if (res == CloseBrace) {
    throw reader_error("Unmatched close brace in: " + std::string(input));
  }

  while (reader.peek() != nullptr) {
    auto form = read_form(&reader);

    if (form == CloseParen) {
      throw reader_error("Unmatched close parenthesis in: " + std::string(input));
    } else if (form == CloseBrace) {
      throw reader_error("Unmatched close brace in: " + std::string(input));
    } else if (form != nullptr) {
      throw reader_error("Junk at end of line: " + try_repr(form));
    }
  }

  return res;
}

lref read(const char* const input) {
  auto reader = Reader(match_str(input));
  return read_internal(reader, input);
}

lref read(const char* const input, const std::string& filename) {
  auto reader = Reader(match_str(input));
  reader.filename = filename;
  return read_internal(reader, input);
}
