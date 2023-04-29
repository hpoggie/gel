#ifndef TYPES_H
#define TYPES_H

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <limits.h>

struct LispObject;

using lref = std::shared_ptr<LispObject>;
using _lisp_function = std::function<lref(lref)>;

struct lisp_error {
  lref value;
  lref stack_trace;
  lisp_error(lref value);
  lisp_error(const char* const value);
  lisp_error(std::string value);
};

struct map_error : public lisp_error { using lisp_error::lisp_error; };
struct hash_error : public lisp_error { using lisp_error::lisp_error; };
struct unwrap_error : public lisp_error { using lisp_error::lisp_error; };
struct list_error : public lisp_error { using lisp_error::lisp_error; };

struct LispObject {
  virtual std::string repr() const { return "()"; }
  virtual std::string str() const { return this->repr(); }
  virtual std::string type_string() const { return "object"; }
  void print() const { std::cout << this->repr(); }
  virtual bool equals(const lref& other) const { return this == other.get(); }

  // TODO: There is probably a faster way to do this. Want to hash on the actual data,
  // not the string
  size_t hash() const {
    return std::hash<std::string>{}(this->repr());
  }
};

extern const lref Nil;

struct LispInt : LispObject {
  int val;

  LispInt(int val) { this->val = val; }
  std::string repr() const { return std::to_string(val); }
  std::string type_string() const { return "int"; }

  bool equals(const lref& other) const {
    auto ot = std::dynamic_pointer_cast<LispInt>(other);
    return ot != nullptr && val == ot.get()->val;
  }

  bool lt(const lref& other) {
    auto ot = std::dynamic_pointer_cast<LispInt>(other);
    return ot != nullptr && val < ot->val;
  }

  bool gt(const lref& other) {
    auto ot = std::dynamic_pointer_cast<LispInt>(other);
    return ot != nullptr && val > ot->val;
  }

  LispInt operator+(const LispInt& rhs) const;
  LispInt operator-(const LispInt& rhs) const;
  LispInt operator*(const LispInt& rhs) const;
  LispInt operator/(const LispInt& rhs) const;
  LispInt operator%(const LispInt& rhs) const;
  LispInt operator+=(const LispInt& rhs) { return val = (*this + rhs).val; }
  LispInt operator-=(const LispInt& rhs) { return val = (*this - rhs).val; }
  LispInt operator*=(const LispInt& rhs) { return val = (*this * rhs).val; }
  LispInt operator/=(const LispInt& rhs) { return val = (*this / rhs).val; }
  LispInt operator%=(const LispInt& rhs) { return val = (*this % rhs).val; }
};

struct Symbol : LispObject {
  std::string name;

  Symbol(std::string name) { this->name = name; }
  std::string repr() const { return name; }
  std::string type_string() const { return "symbol"; }
};

struct Cons : LispObject {
  lref car;
  lref cdr;

  Cons(lref car, lref cdr) {
    this->car = car;
    this->cdr = cdr;
  }

  std::string repr() const;
  std::string type_string() const { return "cons"; }
  bool equals(const lref& other) const {
    auto other_cons = std::dynamic_pointer_cast<Cons>(other);
    if (other_cons == nullptr) {
      return false;
    }

    if (car == nullptr || other_cons->car == nullptr
        || cdr == nullptr || other_cons->cdr == nullptr) {
      return false;
    }
    // TODO: this *might* get TCO'd? Maybe? Either way it'll break on debug
    // So we need to make this a loop. Sadness.
    return car->equals(other_cons->car) && cdr->equals(other_cons->cdr);
  }
};

struct String : LispObject {
  std::string value;

  String(std::string value) { this->value = value; }
  std::string repr() const { return "\"" + value + "\""; }
  std::string str() const { return value; }
  std::string type_string() const { return "string"; }
  bool equals(const lref& other) const {
    auto ot = std::dynamic_pointer_cast<String>(other);
    return ot != nullptr && value == ot.get()->value;
  }
};

// We intentionally don't have a compare for this.
// At the moment, we never create more bools than True and False,
// so we can just point to those and compare addresses rather than having
// to look them up, which is a little faster and simpler to look at in the
// debugger.
struct Bool : LispObject {
  bool value;

  Bool(bool value) { this->value = value; }
  std::string repr() const { return (value ? "true" : "false"); }
  std::string type_string() const { return "bool"; }
};

// Have to declare this here because C++ is dumb
size_t lref_hash(const lref& arg);

struct LrefHash {
  std::size_t operator()(const lref& lr) const noexcept {
    return lref_hash(lr);
  }
};

// https://en.cppreference.com/w/cpp/container/unordered_map
// It doesn't just compare the hash values. It compares using the compares.
// So you have to give it a compare, so it doesn't compare with the wrong compares.
// AAAAAAA
struct LrefReprEqual {
  bool operator()(const lref& lhs, const lref& rhs) const noexcept {
    return lhs->repr() == rhs->repr();
  }
};

struct Map : LispObject {
  std::unordered_map<lref, lref, LrefHash, LrefReprEqual> value;

  Map() {}

  // Don't use this.
  // This is only for builitin.cpp, to make defining repl_env look nicer
  // TODO: find a nicer way
  Map(std::unordered_map<std::string, LispObject*> map) {
    for (auto pair : map) {
      this->set(std::make_shared<Symbol>(pair.first),  // TODO: should be weak pointer
                std::shared_ptr<LispObject>(pair.second));
    }
  }

  std::string repr() const;
  std::string type_string() const { return "map"; }

  void set(lref key, lref value) {
    this->value[key] = value;
  }

  lref get(lref key) {
    auto ret = this->value[key];
    return ret != nullptr ? ret : Nil;
  }
};

extern const lref True;
extern const lref False;


struct ILispFunction : LispObject {
  bool is_macro = false;
  std::string name;
};

struct LispFunction : ILispFunction {
  _lisp_function value;

  LispFunction(_lisp_function value) { this->value = value; }
  std::string repr() const { return "<function>"; }
  std::string type_string() const { return "builtin-function"; }
  lref operator()(lref args) { return this->value(args); }
};

struct MaybeError : LispObject {
  std::string type_string() const { return "maybe-error"; }
};

struct Error : MaybeError {
  std::string desc;
  Error(std::string desc) : desc(desc) {}

  std::string repr() const {
    return "Error: " + desc;
  }
  std::string type_string() const { return "error"; }
};

struct NonError : MaybeError {
  lref wrapped;
  NonError(lref obj) : wrapped(obj) {}

  std::string repr() const {
    return "Wrapped: " + (wrapped.get() != nullptr ? wrapped->repr() : "NULL");
  };
  std::string type_string() const { return "non-error"; }
};

lref cons(const lref& car, const lref& cdr);
lref car(const lref& arg);
lref cdr(const lref& arg);
lref cadr(const lref& arg);
lref cddr(const lref& arg);
lref rplaca(const lref& _cons, const lref& obj);
lref rplacd(const lref& _cons, const lref& obj);
lref concat(const lref& list1, const lref& list2);

lref tail(const lref& arg);
lref last(const lref& arg);
lref butlast(const lref& arg);
lref reversed(lref arg);
lref copy_list(const lref& arg);
int len(lref arg);

lref map_set(const lref& map, const lref& key, const lref& value);
lref map_get(const lref& map, const lref& key);
// Used in case the actual value in the map might be nil
bool map_contains(const lref& map, const lref& key);

lref mapcar(_lisp_function fn, const lref& list);

void warn(const std::string msg);
void warn(const char* const msg);

inline std::string try_repr(const lref& obj) {
  return obj == nullptr ? "!!NULL!!" : obj->repr();
}

inline std::string try_str(const lref& obj) {
  return obj == nullptr ? "!!NULL!!" : obj->str();
}

#endif
