# A brief explanation of gel

## First of all.

- [ ] TODO: this is out of date. Use SCons

You need to get set up with an actual copy of the executable, obviously, or you
won't be able to do any programming. Currently this means you will need to build
it from source.

You will need:
- `g++` that supports C++14 (you can check this using the shell command `g++ -dM
  -E -x c++ /dev/null | grep -F __cplusplus`, it should say something like `
  #define __cplusplus 201402L`, or a higher number)
- `make`

If you're on Windows the best way to get this stuff is probably Cygwin, or WSL.
- https://www.cygwin.com/
- https://docs.microsoft.com/en-us/windows/wsl/install-win10

Now you can clone the repo. From a Linux command line, do:
`git clone git@gitlab.com:hpoggie/gel.git` or `https://gitlab.com/hpoggie/gel.git`

```
cd gel
make
```

You should be good to go. Run it with `./gel`.

## Known issues

- The REPL (short for read-eval-print loop, basically the interactive prompt)
  doesn't like implicit parenthesization because you can't type newlines without
  telling the REPL to evaluate the form. You can run the examples with
  indentation by putting them in a file and doing `(load-file FILENAME)`, or by
  adding the parens back yourself and putting them on one line.
- Only integer arithmetic is currently supported. This is why there's no `/`
  (division). If you want integer division, use `//`.
- There is no gel syntax highlighting / plugin for any text editor.

## The Basics

Everything in gel is written in the follwing way:
```
(function-or-macro arg1 arg2 ...)
```
This is called an S-Expression.

Examples:

```
;; Call the function prn with the argument "Hello, World!"
(prn "Hello, World!") ;; Prints "Hello, World!". An unexpected twist.

;; Call the function + with arguments 2 and 3
(+ 2 3) ;; => 5
```

(You probably guessed this, but `;` indicates a comment.)

S-expressions can be nested:
```
(= (+ 2 2) 4)  ;; => true
```

Every S-expression evaluates to a value. They can have side effects, but they
still evaluate to something. If you want to use something _just_ for its side
effects, you will probably want to use `progn`. If you don't know what that is,
don't worry. All will be revealed in time.

## If statements

```
(if condition first-value second-value)
```
This evaluates to first-value if condition is true and second-value otherwise.

```
(if true 1 0)  ;; => 1
(if (= (+ 2 2) 4) "foo" "bar")  ;; => "foo"
```

## Loops

```
(dotimes 10 (prn "Hello!"))  ;; Prints "Hello!" 10 times

(for (x '(1 2 3)) (prn x))
;; Prints:
;; 1
;; 2
;; 3
```

## Doing a bunch of stuff at once

If you try to do something like `(if condition foo bar baz)`, you will run
into a problem. If statements only have two branches, so you need some way 
to combine `bar` and `baz` into one expression. What you need is `progn`:

```
(progn bar baz)
```

`progn` takes any number of arguments, evaluates them, and returns the 
result of evaluating the last one. Very handy.

## Local Variables

In gel, the scope of a local variable is defined by a `let` block:

```
(let (x 10)
  (prn x))  ;; Prints 10
```

They can be modified using `set`.

```
(let (x 10)
  (progn
    (prn x)  ;; Prints 10
    (set x 42)
    (prn x)))  ;; Prints 42
```

## Global Variables

... are defined using `def`.

```
(def *x* 3)
(set *x* 4)
(prn *x*)  ;; Prints 4
```

Pretty simple.

By convention, global variables have `*earmuffs*`. If we're using a global variable, we want people to know it.

## Functions

```
defun! foo (x)
  (+ x 2)
  
(foo 3)  ;; => 5
```

Functions are values:

```
(def! bar foo)
(bar 4)  ;; => 6
```

`fn` creates a function value without binding it to a variable. This is the same
as a feature called `lambda` in a lot of programming languages.

```
(fn (x) (+ x 2))  ;; => <function (x) ((+ x 2))>
```

## Recursion and TCO

Normally, if you nest function calls deeply enough, you will eventually overflow
the stack and your program will crash. However, if a function call is the last
thing a function does (known as being in tail position), nothing actually needs
to be pushed to the stack. So gel can change function calls that are in tail
position into jumps. This is called __tail call optimization (TCO)__.

So we can do something like this:

```
(defun! loop () (loop))
(loop)  ;; Loops infinitely. Will never stack overflow
```

However:

```
(defun! loop () (progn (loop) 2))  ;; Will eventually stack overflow
```

This example can't be TCO'd because `(loop)` isn't the last thing the function
does - it also returns `2`.

You can use this to fake a loop:

```
;; Print the perfect squares up to max
(defun! squares (max)
  (let (f (fn (i)
            (if (> i max)
                nil
                (progn
                  (prn (* i i))
                  (f (+ i 1))))))
    (f 1)))
```

One-line version for easy copy-pasting into the REPL:

```
(defun! squares (max) (let (f (fn (i) (if (> i max) nil (progn (prn (* i i
)) (f (+ i 1)))))) (f 1)))

(squares 10)
;; Prints the following
1
4
9
16
25
36
49
64
81
100
```

## Special Symbols

Up to this point, we've talked about a lot of things that have the same
S-expression syntax as functions, but clearly aren't. For example, you might
think "there's no way `if` could be a function, right?" And you'd be right.
There are a bunch of things that are built into the language that look like
functions but work a bit differently. These things are called special symbols,
and they make up the core of the language. They are:

```
def
defined?
env-get - used for GEL implementation, you don't need to worry about this
defmacro - create a macro
let
set
progn
if
fn
quote - don't evaluate this
quasiquote - don't evaluate any part of this unless I unquote it
unquote - actually, do evaluate this
macroexpand - show me what this macro expands to
try - Do this thing, but look out for exceptions
apply - pass a list as the arguments to a function
```

Looking at this, you can see we're already done with the basics of gel. The only
things we haven't talked about so far are macros, quoting, and `try` - that is,
the interesting stuff.
