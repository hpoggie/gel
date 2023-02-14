#include "types.h"
#include "stacktrace.h"

const lref Nil = std::make_shared<LispObject>();
const lref True = std::make_shared<Bool>(true);
const lref False = std::make_shared<Bool>(false);

struct arithmetic_error : lisp_error { using lisp_error::lisp_error; };

std::string Cons::repr() const {
  std::stringstream stream("");

  stream << "(";

  stream << try_repr(car);

  lref current = cdr;
  const Cons* ccons;
  // If the cdr is also a Cons, we are in a list.
  while ((ccons = std::dynamic_pointer_cast<const Cons>(current).get())) {
    stream << " ";
    stream << try_repr(ccons->car);
    if (ccons->cdr == nullptr) {
      stream << "List ends on null";
      break;
    }
    current = ccons->cdr;
  }

  // If the end isn't a Nil, we're in an improper list
  if (current != Nil) {
    stream << " . ";
    stream << try_repr(current);
  }

  stream << ")";

  return stream.str();
}

std::string Map::repr() const {
  std::string ret = "{";
  for (auto iter = value.begin(); iter != value.end(); iter++) {
    if (iter->second == nullptr) {
      continue;
    }

    if (iter->first == nullptr) {
      throw map_error("Null key.");
    }

    if (iter != value.begin()) {
      ret += " ";
    }

    ret += iter->first->repr() + " " + iter->second->repr();
  }
  ret += "}";
  return ret;
}

lref car(const lref& arg) {
  if (arg == nullptr) {
    throw list_error("Argument is null.");
  }

  auto as_cons = std::dynamic_pointer_cast<const Cons>(arg).get();
  if (as_cons == nullptr) {
    // TODO: handle more gracefully
    throw list_error("Argument is not a cons: " + try_repr(arg));
  }

  if (as_cons->car == nullptr) {
    throw list_error("car is nullptr.");
  }

  return as_cons->car;
}

lref cdr(const lref& arg) {
  if (arg == nullptr) {
    throw list_error("Argument is null.");
  }

  auto as_cons = std::dynamic_pointer_cast<const Cons>(arg).get();
  if (as_cons == nullptr) {
    // TODO: handle more gracefully
    throw list_error("Argument is not a cons: " + try_repr(arg));
  }

  if (as_cons->cdr == nullptr) {
    throw list_error("cdr is nullptr.");
  }

  return as_cons->cdr;
}

lref cadr(const lref& obj) {
  return car(cdr(obj));
}

lref cddr(const lref& obj) {
  return cdr(cdr(obj));
}

// Replace the car of _cons with obj
lref rplaca(const lref& _cons, const lref& obj) {
  auto cons_ptr = std::dynamic_pointer_cast<Cons>(_cons).get();
  if (cons_ptr == nullptr) {
    throw list_error("Argument is not a cons: " + try_repr(_cons));
  }

  cons_ptr->car = obj;

  return _cons;
}

// Replace the cdr of _cons with obj
lref rplacd(const lref& _cons, const lref& obj) {
  auto cons_ptr = std::dynamic_pointer_cast<Cons>(_cons).get();
  if (cons_ptr == nullptr) {
    throw list_error("Argument is not a cons: " + try_repr(_cons));
  }

  cons_ptr->cdr = obj;

  return _cons;
}

lref tail(const lref& arg) {
  auto ret = arg;
  while (cdr(ret) != Nil) {
    ret = cdr(ret);
  }
  return ret;
}

lref last(const lref& arg) {
  return car(tail(arg));
}

lref collect(lref arg, std::function<bool(lref)> condition, std::function<lref(lref)> val) {
  if (arg == Nil) {
    return Nil;
  }

  if (!condition(arg)) {
    return Nil;
  }

  auto res = cons(val(arg), Nil);
  auto tail = res;
  arg = cdr(arg);

  while (condition(arg)) {
    rplacd(tail, cons(val(arg), Nil));
    arg = cdr(arg);
    tail = cdr(tail);
  }

  return res;
}

lref butlast(const lref& arg) {
  return collect(arg, [](lref arg){return cdr(arg) != Nil;}, [](lref arg){return car(arg);});
}

lref reversed(lref arg) {
  auto res = Nil;

  while (arg != Nil) {
    res = cons(car(arg), res);
    arg = cdr(arg);
  }

  return res;
}

size_t lref_hash(const lref& arg) {
  if (arg.get() == nullptr) {
    throw hash_error("Argument is null.");
  }

  return arg->hash();
}

lref map_set(const lref& map, const lref& key, const lref& value) {
  if (map == nullptr) {
    throw map_error("Argument is null.");
  }

  auto as_map = std::dynamic_pointer_cast<Map>(map).get();
  if (as_map == nullptr) {
    throw map_error("Argument is not a map: " + try_repr(map));
  }

  as_map->set(key, value);
  return value;
}

lref map_get(const lref& map, const lref& key) {
  if (map == nullptr) {
    throw map_error("Argument is null.");
  }

  auto as_map = std::dynamic_pointer_cast<Map>(map).get();
  if (as_map == nullptr) {
    throw map_error("Argument is not a map: " + try_repr(map));
  }

  return as_map->get(key);
}

bool map_contains(const lref& map, const lref& key) {
  if (map == nullptr) {
    throw map_error("Argument is null.");
  }

  auto as_map = std::dynamic_pointer_cast<Map>(map).get();
  if (as_map == nullptr) {
    throw map_error("Argument is not a map: " + try_repr(map));
  }

  return as_map->value[key] != nullptr;
}

lref cons(const lref& car, const lref& cdr) {
  return std::make_shared<Cons>(car, cdr);
}

lref mapcar(_lisp_function fn, const lref& lst) {
  return collect(lst, [](lref arg){return arg != Nil;},
                 [fn](lref arg){return fn(cons(car(arg), Nil));});
}

lref copy_list(const lref& arg) {
  return mapcar([](lref args){ return car(args); }, arg);
}

lref concat(const lref& list1, const lref& list2) {
  auto res = copy_list(list1);
  rplacd(tail(res), copy_list(list2));
  return res;
}

int len(lref arg) {
  int i;
  for (i = 0; arg != Nil; i++, arg = (cdr(arg)));
  return i;
}

LispInt LispInt::operator+(const LispInt& rhs) const {
  // Check for overflow
  // Have to do this before rather than just reading some kind of flags since
  // overflows are UB in C++
  // AAAAAAAAAAAAAAAAAAAAAAAAAAAA
  // https://stackoverflow.com/questions/199333/how-do-i-detect-unsigned-integer-multiply-overflow
  if (rhs.val > 0 && val > INT_MAX - rhs.val) {
    throw arithmetic_error("Addition would overflow: " + std::to_string(val)
                           + " + " + std::to_string(rhs.val));
  }

  if (rhs.val < 0 && val < INT_MIN - rhs.val) {
    throw arithmetic_error("Addition would underflow: " + std::to_string(val)
                           + " + " + std::to_string(rhs.val));
  }

  return val + rhs.val;
}

LispInt LispInt::operator-(const LispInt& rhs) const {
  if (rhs.val > 0 && val < INT_MIN + rhs.val) {
    throw arithmetic_error("Subtraction would underflow: "
                           + std::to_string(val) + " - "
                           + std::to_string(rhs.val));
  }

  if (rhs.val < 0 && val > INT_MAX + rhs.val) {
    throw arithmetic_error("Subtraction would overflow: "
                           + std::to_string(val) + " - "
                           + std::to_string(rhs.val));
  }

  return val - rhs.val;
}

LispInt LispInt::operator*(const LispInt& rhs) const {
  if (rhs.val > 0 && val > INT_MAX / rhs.val ) {
    throw arithmetic_error("Multiplication would overflow: "
                           + std::to_string(val) + " * "
                           + std::to_string(rhs.val));
  }

  if (rhs.val == -1 && val < -INT_MAX) {
    throw arithmetic_error("Multiplication by negative would overflow: "
                           + std::to_string(val) + " * "
                           + std::to_string(rhs.val));
  }

  if (rhs.val < -1 && val < INT_MAX / rhs.val) {
    throw arithmetic_error("Multiplication would underflow:"
                           + std::to_string(val) + " * "
                           + std::to_string(rhs.val));
  }

  return val * rhs.val;
}

LispInt LispInt::operator/(const LispInt& rhs) const {
  if (rhs.val == 0) {
    throw arithmetic_error("Division by 0: "
                           + std::to_string(val) + " / "
                           + std::to_string(rhs.val));
  }

  if (rhs.val == -1 && val < -INT_MAX) {
    throw arithmetic_error("Division by -1 would overflow: "
                           + std::to_string(val) + " / "
                           + std::to_string(rhs.val));
  }

  if (val == -1 && rhs.val < -INT_MAX) {
    throw arithmetic_error("Division by -1 would overflow: "
                           + std::to_string(val) + " / "
                           + std::to_string(rhs.val));
  }

  return val / rhs.val;
}

LispInt LispInt::operator%(const LispInt& rhs) const {
  return val % rhs.val;
}

lisp_error::lisp_error(lref value) : value(value) {
  this->stack_trace = std::make_shared<String>(backtrace());
}

lisp_error::lisp_error(const char* const value) {
  this->value = std::make_shared<String>(value);
  this->stack_trace = std::make_shared<String>(backtrace());
}

lisp_error::lisp_error(std::string value) {
  this->value = std::make_shared<String>(value);
  this->stack_trace = std::make_shared<String>(backtrace());
}
