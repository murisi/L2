# L2

* [Introduction](#introduction)
* [Getting Started](#getting-started)
  * [Building L2](#building-l2)
  * [Shell Interface](#shell-interface)
* [Primitive Expressions](#primitive-expressions)
  * [Begin](#begin)
  * [Binary](#binary)
  * [Reference](#reference)
  * [If](#if)
  * [Function](#function)
  * [Invoke](#invoke)
  * [With Continuation](#with-continuation)
  * [Make Continuation](#make-continuation)
  * [Continue](#continue)
* [Internal Representation](#internal-representation)
* [Expression](#expression)
* [Reductions](#reductions)

## Introduction
L2 is an attempt to find the smallest most distilled programming language equivalent to C. The goal is to turn as much of C's preprocessor directives, control structures, statements, literals, and functions requiring compiler assistance (setjmp, longjmp, alloca, ...) into things definable inside L2. The language does not surject to all of C, its most glaring omission being that of a type-system. However, I reckon the result is still pretty interesting.

The approach taken to achieve this has been to make C's features more composable, more multipurpose, and, at least on one occasion, add a new feature so that a whole group of distinct features could be dropped. In particular, the most striking changes are that C's:
1. irregular syntax is replaced by [S-expressions](#internal-representation); because simple syntax composes well with a non-trivial preprocessor (and [no, I have not merely transplanted Common Lisp's macros into C](#expression))
2. loop constructs are replaced with what I could only describe as [a more structured variant of setjmp and longjmp without stack destruction](#make-continuation) (and [no, there is no performance overhead associated with this](#an-optimization))

There are [9 language primitives](#primitive-expressions) and for each one of them I describe their syntax, what exactly they do in English, the i386 assembly they translate into, and an example usage of them. Following this comes a brief description of [L2's internal representation and the 9 functions (loosely speaking) that manipulate it](#internal-representation). After that comes a description of how [a non-primitive L2 expression](#expression) is compiled. The above descriptions take about 8 pages and are essentially a complete description of L2. Following this comes a sort of "glossary" that shows how not only C's constructs, but more exotic stuff like coroutines, Python's generators, and Scheme's lambdas can be defined in terms of L2.

## Getting Started
### Building L2
```shell
./buildl2
```
The L2 compiler needs a Linux distribution running on the i386 or AMD64 architecture with the GNU C compiler installed to run successfully. To build L2, simply run the `buildl2` script at the root of the repository. This will create a directory called `bin` containing the files `l2compile` and `demort.o`. `l2compile` is the compiler for L2 and its interface is described below. `demort.o` is not a part of L2, but it will be used in the demonstrations below.

### Shell Interface
```shell
./bin/l2compile (-pic | -pdc) -object output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...
./bin/l2compile (-pic | -pdc) -library output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...
./bin/l2compile (-pic | -pdc) -program output objects.o ... (- inputs.l2 ...) ... - inputs.l2 ...
```
Starting at the first hyphen argument, the compiler reads `inputs.l2 ...` until either the next hyphen argument is found or the command line arguments are finished. Each of the files read should be of the form `expression1 expression2 ... expressionN`. The compiler then concatenates all the L2 files read, in the same order. After that, the compiler compiles each expression in the concatenated L2 file emitting the corresponding object code in the same order as the expressions of the concatenated file. L2 is executed top-down, there is no main function. Each expression is compiled in the environment: the set of defined symbols.

If there are still unconsumed hyphens, then the object file is packaged into a shared library along with `objects.o ...`, and this shared library is dynamically loaded into the environment. And the compilation process starts again, only this time with the next set of `inputs.l2...`. If there are no more unconsumed hyphens, then the output should either be a position independent or dependent object, shared library, or program called `output` as specified by the first 3 arguments to `l2compile`. If the final output is not an object file, then `objects.o ...` are embeded or linked into it.

The initial environment, the one that is there before any group of files is compiled, comprises 17 functions: `lst`, `lst?`, `fst`, `rst`, `sexpr`, `nil`, `nil?`, `-<character>-`, `<character>?`, `begin`, `b`, `if`, `function`, `invoke`, `with-continuation`, `make-continuation`, and `continue`. The former 9 are defined later. Each one of the latter 8 functions does nothing else but return an s-expression formed by prepending its function name to the list of s-expressions supplied to them. For example, the `b` function could have the following definition: `(function b (sexprs) [lst [lst [-b-] [nil]] [' sexprs]])`.

#### Example
file1.l2:
```racket
(function foo (sexprs)
	(with-continuation return
		(begin
			[putchar [+ (b 00000000000000000000000001100001) (b 00000000000000000000000000000001)]]
			{return [lst [lst [-b-] [lst [-e-] [lst [-g-] [lst [-i-] [lst [-n-] [nil]]]]]] [nil]]})))

[putchar (b 00000000000000000000000001100001)]
```
file2.l2:
```racket
(function bar ()
	[putchar (b 00000000000000000000000001100011)])
(foo this text does not matter)
[putchar (b 00000000000000000000000001100100)]
```
Running `./bin/l2compile -pdc -program myprogram bin/demort.o - file1.l2 - file2.l2` produces a program called `myprogram`. During the compilation, the text "ab" should have been printed to standard output. The "a" comes from the last expression of file1.l2. It was printed after the compilation of file1.l2, when it was being loaded into the compiler. Why? Because L2 libraries are executed from top to bottom when they are dynamically loaded (and also when they are statically linked). The "b" comes from within the function in file1.l2. It was executed when the expression `(foo this text does not matter)` in file2.l2 was being compiled. Why? Because the `foo` causes the compiler to invoke a function called `foo` in the environment. The s-expression `(this text does not matter)` is the argument to the function `foo`, but the function `foo` ignores it and returns the s-expression `(begin)`. Hence `(begin)` replaces `(foo this text does not matter)` in `file2.l2`. Now `file2.l2` is entirely made up of primitive expressions which are compiled in the way specified below. The resulting executable `myprogram` is run using the command `./myprogram`. It prints the text "d" when executed. Why? Because the last expression of file2.l2 is the only one that actually does something.

If instead we run `./bin/l2compile -pic -library mylibrary.so bin/demort.o - file1.l2 - file2.l2`, a shared library named `mylibrary.so` is produced. Running `objdump -T mylibrary.so` on it shows us that the function `bar` is exported. It also shows us that `foo` is not exported. Why is the second fact true? Because file1.l2 does not come after the final hyphen. It only has relevance during the compilation process. Why is the first fact true? Because file1.l2 comes after the final hyphen and because `bar` is a top-level expression. When mylibrary.so is dynamically loaded (perhaps by using `dlopen`), the text "d" will be printed to standard output. And if the symbol `bar` (perhaps obtained using `dlsym`) is invoked, the text "c" will be printed to standard output.

Running `./bin/l2compile -pic -object mylibrary.o bin/demort.o - file1.l2 - file2.l2` followed by `./bin/l2compile -pdc -program myprogram mylibrary.o bin/demort.o - file1.l2 -` produces the program `myprogram`. The execution of the first command should have caused the compiler to print "ab" to standard output for the same reasons as above. As for the second command, the text "da" should have been printed to standard output. Why? Because after file1.l2 is compiled into object code, it is packaged in a shared object in such a way that the object files mylibrary.o and demort.o come before it. Hence when that shared library is loaded, the last expression of file2.l2 is executed followed by the last expression of file1.l2. There being no source files after the final hyphen, mylibrary.o and demort.o are linked to nothing else which in the end produces an executable called `myprogram` that prints "d" to standard output.

## Primitive Expressions
### Begin
```racket
(begin expression1 expression2 ... expressionN)
```
Evaluates its subexpressions sequentially from left to right. That is, it evaluates `expression1`, then `expression2`, and so on, ending with the execution of `expressionN`. Specifying zero subexpressions is valid. The return value is unspecified.

This expression is implemented by emitting the instructions for `expression1`, then emitting the instructions for `expression2` immediately afterwords and so on, ending with the emission of `expressionN`.

Say the expression `[foo]` prints the text "foo" to standard output and the expression `[bar]` prints the text "bar" to standard output. Then `(begin [foo] [bar] [foo] [foo] [foo])` prints the text "foobarfoofoofoo" to standard output.

### Binary
```racket
(b b31b30...b0)
```
The resulting value is the 32 bit number specified in binary inside the brackets. Specifying less than or more than 32 bits is an error. Useful for implementing character and string literals, and numbers in other bases.

This expression is implemented by emitting an instruction to `mov` an immediate value into a memory location designated by the surrounding expression.

Say the expression `[putchar x]` prints the character `x`. Then `[putchar (b 00000000000000000000000001100001)]` prints the text "a" to standard output.

### Reference
```racket
reference0
```
The resulting value is the address in memory to which this reference refers.

This expression is implemented by the emission of an instruction to `lea` of some data into a memory location designated by the surrounding expression.

Say the expression `[' x]` evaluates to the value at the reference `x` and the expression `[set x y]` puts the value `y` into the reference `x`. Then `(begin [set x (b 00000000000000000000000001100001)] [putchar [' x]])` prints the text "a" to standard output.

### If
```racket
(if expression0 expression1 expression2)
```
If `expression0` is non-zero, then only `expression1` is evaluated and its resulting value becomes that of the whole expression. If `expression0` is zero, then only `expression2` is evaluated and its resulting value becomes that of the whole expression.

This expression is implemented by first emitting an instruction to `or` `expression0` with itself. Then an instruction to `je` to `expression2`'s label is emitted. Then the instructions for `expression1` are emitted with the location of the resulting value fixed to the same memory address designated for the resulting value of the `if` expression. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this `if` expression. Then the label for `expression2` is emitted. Then the instructions for `expression2` are emitted with the location of the resulting value fixed to the same memory address designated for the resulting value of the `if` expression.

The expression `[putchar (if (b 00000000000000000000000000000000) (b 00000000000000000000000001100001) (b 00000000000000000000000001100010))]` prints the text "b" to standard output.

### Function
```racket
(function function0 (reference1 reference2 ... referenceN) expression0)
```
Makes a function to be invoked with exactly `N` arguments. When the function is invoked, `expression0` is evaluated in an environment where `function0` is a reference to the function itself and `reference1`, `reference2`, up to `referenceN` are references to the resulting values of evaluating the corresponding arguments in the invoke expression invoking this function. Once the evaluation is complete, control flow returns to the invoke expression and the invoke expression's resulting value is the resulting value of evaluating `expression0`. The resulting value of this function expression is a reference to the function.

This expression is implemented by first emitting an instruction to `mov` the address `function0` (a label to be emitted later) into the memory location designated by the surrounding expression. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this function. Then the label named `function0` is emitted. Then instructios to `push` each callee-saved register onto the stack are emitted. Then an instruction to push the frame-pointer onto the stack is emitted. Then an instruction to move the value of the stack-pointer into the frame-pointer is emitted. Then an instruction to `sub` from the stack-pointer the amount of words reserved on this function's stack-frame is emitted. After this the instructions for `expression0` are emitted with the location of the resulting value fixed to a word within the stack-pointer's drop. After this an instruction is emitted to `mov` the word from this location into the register `eax`. And finally, instructions are emitted to `leave` the current function's stack-frame, `pop` the callee-save registers, and `ret` to the address of the caller.

The expression `[putchar [(function my- (a b) [- [' b] [' a]]) (b 00000000000000000000000000000001) (b 00000000000000000000000001100011)]]` prints the text "b" to standard output.

### Invoke
```racket
(invoke function0 expression1 expression2 ... expressionN)
[function0 expression1 expression2 ... expressionN]
```
Both the above expressions are equivalent. Evaluates `function0`, `expression1`, `expression2`, up to `expressionN` in an unspecified order and then invokes `function0`, a reference to a function, providing it with the resulting values of evaluating `expression1` up to `expressionN`, in order. The resulting value of this expression is determined by the function being invoked.

`N+1` words must be reserved in the current function's stack-frame plan. The expression is implemented by emitting the instructions for any of the subexpressions with the location of the resulting value fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. Then an instruction to `push` the last reserved word onto the stack is emitted, followed by the second last, and so on, ending with an instruction to `push` the first reserved word onto the stack. A `call` instruction with the zeroth reserved word as the operand is then emitted. Note that L2 expects registers `esp`, `ebp`, `ebx`, `esi`, and `edi` to be preserved across `call`s. An `add` instruction that pops N words off the stack is then emitted. Then an instruction is emitted to `mov` the register `eax` into a memory location designated by the surrounding expression.

A function with the reference `-` that returns the value of subtracting its second parameter from its first could be defined as follows:
```assembly
-:
movl 4(%esp), %eax
subl 8(%esp), %eax
ret
```
The following invokation of it, `(invoke putchar (invoke - (b 00000000000000000000000001100011) (b 00000000000000000000000000000001)))`, prints the text "b" to standard output.

### With Continuation
```racket
(with-continuation continuation0 expression0)
```
Makes a continuation to the containing expression that is to be continued to with exactly one argument. Then `expression0` is evaluated in an environment where `continuation0` is a reference to the aforementioned continuation. The resulting value of this expression is unspecified if the evaluation of `expression0` completes. If the continuation `continuation0` is continued to, then this `with-continuation` expression evaluates to the resulting value of the single argument within the responsible continue expression.

5+1 words must be reserved in the current function's stack-frame plan. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting instructions to store the program's state at `continuation0`, that is, instructions are emitted to `mov` `ebp`, the address of the instruction that should be executed after continuing (a label to be emitted later), `edi`, `esi`, and `ebx`, in that order, to the first 5 words at `continuation0`. After this, the instructions for `expression0` are emitted. Then the label for the first instruction of the continuation is emitted. And finally, an instruction is emitted to `mov` the resulting value of the continuation, the 6th word at `continuation0`, into the memory location designated by the surrounding expression.

#### Examples
Note that the expression `{continuation0 expression0}` continues to the continuation reference by `continuation0` with resulting value of evaluating `expression0` as its argument. With the note in mind, the expression `(begin [putchar (with-continuation ignore (begin {ignore (b 00000000000000000000000001001110)} [foo] [foo] [foo]))] [bar])` prints the text "nbar" to standard output.

The following assembly function `allocate` receives the number of bytes it is to allocate as its first argument, allocates that memory, and passes the initial address of this memory as the single argument to the continuation it receives as its second argument.
```assembly
allocate:
/* All sanctioned by L2 ABI: */
movl 8(%esp), %ecx
movl 16(%ecx), %ebx
movl 12(%ecx), %esi
movl 8(%ecx), %edi
movl 0(%ecx), %ebp
subl 4(%esp), %esp
andl $0xFFFFFFFC, %esp
movl %esp, 20(%ecx)
jmp *4(%ecx)
```
The following usage of it, `(with-continuation dest [allocate (b 00000000000000000000000000000011) dest])`, evaluates to the address of the allocated memory. If allocate had just decreased `esp` and returned, it would have been invalid because L2 expects functions to preserve `esp`.

### Make Continuation
```racket
(make-continuation continuation0 (reference1 reference2 ... referenceN) expression0)
```
Makes a continuation to be continued to with exactly `N` arguments. When the continuation is continued to, `expression0` is evaluated in an environment where `continuation0` is a reference to the continuation itself and `reference1`, `reference2`, up to `referenceN` are references to the resulting values of evaluating the corresponding arguments in the continue expression continuing to this function. Undefined behavior occurs if the evaluation of `expression0` completes - i.e. programmer must direct the control flow out of `continuation0` somewhere within `expression0`. The resulting value of this make-continuation expression is a reference to the continuation.

5+N words must be reserved in the current function's stack-frame plan. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting an instruction to `mov` the reference `continuation0` into the memory location designated by the surrounding expression. Instructions are then emitted to store the program's state at `continuation0`, that is, instructions are emitted to `mov` `ebp`, the address of the instruction that should be executed after continuing (a label to be emitted later), `edi`, `esi`, and `ebx`, in that order, to the first 5 words at `continuation0`. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this make-continuation expression. Then the label for the first instruction of the continuation is emitted. After this the instructions for `expression0` are emitted.

The expression `{(make-continuation forever (a b) (begin [putchar [' a]] [putchar [' b]] {forever [- [' a] (b 00000000000000000000000000000001)] [- [' b] (b 00000000000000000000000000000001)]})) (b 00000000000000000000000001011010) (b 00000000000000000000000001111010)}` prints the text "ZzYyXxWw"... to standard output.

### Continue
```racket
(continue continuation0 expression1 expression2 ... expressionN)
{continuation0 expression1 expression2 ... expressionN}
```
Both the above expressions are equivalent. Evaluates `continuation0`, `expression1`, `expression2`, up to `expressionN` in an unspecified order and then continues to `continuation0`, a reference to a continuation, providing it with a local copies of `expression1` up to `expressionN` in order. The resulting value of this expression is unspecified.

`N+1` words must be reserved in the current function's stack-frame plan. The expression is implemented by emitting the instructions for any of the subexpressions with the location of the resulting value fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. Then an instruction to `mov` the first reserved word to 5 words from the beginning of the continuation is emitted, followed by an instruction to `mov` the second reserved word to an address immediately after that, and so on, ending with an instruction to `mov` the last reserved word into the last memory address of that area. The program's state, that is, `ebp`, the address of the instruction that should be executed after continuing, `edi`, `esi`, and `ebx`, in that order, are what is stored at the beginning of a continuation. Instructions to `mov` these values from the buffer into the appropiate registers and then set the program counter appropiately are, at last, emitted.

The expression `(begin (with-continuation cutter (continue (make-continuation cuttee () (begin [bar] [bar] (continue cutter (b 00000000000000000000000000000000)) [bar] [bar] [bar])))) [foo])` prints the text "barbarfoo" to standard output.

#### An Optimization
Looking at the examples above where the continuation reference does not escape, `(with-continuation reference0 expression0)` behaves a lot like the pseudo-assembly `expression0 reference0:` and `(make-continuation reference0 (...) expression0)` behaves a lot like `reference0: expression0`. To be more precise, when references to a particular continuation only occur as the `continuation0` subexpression of a continue statement, we know that the continuation is constrained to the function in which it is declared, and hence there is no need to store or restore `ebp`, `edi`, `esi`, and `ebx`. Continuations, then, are how efficient iteration is achieved in L2.

## Internal Representation
After substituting out the syntactic sugar used for the `invoke` and `continue` expressions. We find that all L2 programs are just compositions of the `<pre-s-expression>`s: `<symbol>` and `(<pre-s-expression> <pre-s-expression> ... <pre-s-expression>)`. If we now replace every symbol with a list of its characters so that for example `foo` becomes `(f o o)`, we now find that all L2 programs are now just compositions of the `<s-expression>`s `<character>` and `(<s-expression> <s-expression> ... <s-expression>)`. The following functions that manipulate these s-expressions are not part of the L2 language and hence the compiler does not give references to them special treatment during compilation. However, when compiled code is loaded into an L2 compiler, undefined references to these functions are to be dynamically resolved.

### `[lst x y]`
`x` must be a s-expression and `y` a list.

Makes a list where `x` is first and `y` is the rest.

Say the s-expression `foo` is stored at `a` and the list `(bar)` is stored at `b`. Then `[lst [' a] [' b]]` is the s-expression `(foo bar)`.
### `[lst? x]`
`x` must be a s-expression.

Evaluates to the complement of zero if `x` is also list. Otherwise evaluates to zero.

Say the s-expression `foo` is stored at `a`. Then `[lst? [' a]]` evaluates to `(b 11111111111111111111111111111111)`.
### `[fst x]`
`x` must be a list.

Evaluates to a s-expression that is the first of `x`.

Say the list `foo` is stored at `a`. Then `[fst [' a]]` is the s-expression `a`. This `a` is not a list but is a character.
### `[rst x]`
`x` must be a list`.

Evaluates to a list that is the rest of `x`.

Say the list `foo` is stored at `a`. Then `[rst [' a]]` is the s-expression `oo`.
### `[sexpr x]`
`x` must be a list.

Evaluates to an s-expression wrapper of `x`.

Say the s-expression `foo` is stored at `a` and `(bar)` is stored at `b`. Then `[lst [sexpr [rst [' a]]] [' b]]` is the s-expression `(oo bar)`. Note that without the `sexpr` invokation, the preconditions of `lst` would be violated.
### `[nil]`
Evaluates to the empty list.

Say the s-expression `foo` is stored at `a`. Then `[lst [' a] [nil]]` is the s-expression `(foo)`.
### `[nil? x]`
`x` must be a list.

Evaluates to the complement of zero if `x` is the empty list. Otherwise evaluates to zero.

Say the s-expression `((foo bar bar bar))` is stored at `x`. Then `[nil? [rst [' x]]]` evaluates to `(b 11111111111111111111111111111111)`.
### `[-<character>-]`
Evaluates to the character `<character>`.

The expression `[lst [-f-] [lst [-o-] [lst [-o-] [nil]]]]` evaluates to the s-expression `foo`.
### `[<character>? x]`
`x` must be a s-expression.

Evaluates to the complement of zero if `x` is the character <character>. Otherwise evaluates to zero.

Say the s-expression `(foo (bar bar) foo foo)` is stored at `x`. Then `[m? [' x]]` evaluates to `(b 00000000000000000000000000000000)`.

## Expression
```racket
(function0 expression1 ... expressionN)
```
If the above expression is not a primitive expression, then `function0` is evaluated in the environment. The resulting value of this evluation is then invoked with the (unevaluated) list of s-expressions `(expression1 expression2 ... expressionN)` as its only argument. The list of s-expressions returned by this function then replaces the entire list of s-expressions `(function0 expression1 ... expressionN)`. If the result of this replacement is still a non-primitive expression, then the above process is repeated. When this process terminates, the appropiate assembly code for the resulting primitive expression is emitted.

The expression `((function comment (sexprs) [fst [' sexprs]]) [foo] This comment is ignored. No, seriously.)` is replaced by `[foo]`, which in turn compiles into assembly similar to what is generated for other invoke expressions.

## Reductions
In the extensive list processing that follows, the follwing functions prove to be convenient abbreviations:

abbreviations.l2:
```racket
(function frst (l) [fst [rst [' l]]])
(function frfst (l) [fst [rst [fst [' l]]]])
(function frrst (l) [fst [rst [rst [' l]]]])
(function frrrst (l) [fst [rst [rst [rst [' l]]]]])
(function rfst (l) [rst [fst [' l]]])
(function ffst (l) [fst [fst [' l]]])
(function llst (a b c) [lst [' a] [lst [' b] [' c]]])
(function lllst (a b c d) [lst [' a] [lst [' b] [lst [' c] [' d]]]])
(function llllst (a b c d e) [lst [' a] [lst [' b] [lst [' c] [lst [' d] [' e]]]]])
(function llllllst (a b c d e f g) [lst [' a] [lst [' b] [lst [' c] [lst [' d] [lst [' e] [lst [' f] [' g]]]]]]])
```

### Commenting
L2 has no built-in mechanism for commenting code written in it. The following comment function that follows takes a list of s-expressions as its argument and returns the last s-expression in that list (which is guaranteed to be a list of s-expressions) effectively causing the other s-expressions to be ignored. Its implementation follows:

comment.l2:
```racket
(function ** (l)
	(with-continuation return
		{(make-continuation find (first last)
			(if [nil? [' last]]
				{return [' first]}
				{find [fst [' last]] [rst [' last]]})) [fst [' l]] [rst [' l]]}))
```

It is used as follows:

test.l2:
```racket
(** This is a comment, and the next thing is what is actually compiled: (begin))
```
The above example is compiled using the command `./bin/l2compile -pdc -program test demort.o - abbreviations.l2 - comment.l2 - test.l2`.

### Numbers:
Integer literals prove to be quite tedious in L2. The following function, `d`, implement decimal arithmetic by reading in an s-expression in base 10 and writing out the equivalent s-expression in base 2:

numbers.l2
```racket
(** Turns a 4-byte integer into base-2 s-expression representation of it.
(function binary->base2sexpr (binary)
	[lst [lst [-b-] [nil]] [lst (with-continuation return
		{(make-continuation write (count in out)
			(if [' count]
				{write [- [' count] (b 00000000000000000000000000000001)]
					[>> [' in] (b 00000000000000000000000000000001)]
					[lst (if [and [' in] (b 00000000000000000000000000000001)] [-1-] [-0-]) [' out]]}
				{return [' out]})) (b 00000000000000000000000000100000) [' binary] [nil]}) [nil]]]))

(function d (l)
	[binary->base2sexpr
		(** Turns the base-10 s-expression input into a 4-byte integer.
			(with-continuation return {(make-continuation read (in out)
				(if [nil? [' in]]
					{return [' out]}
					{read [rst [' in]] [+ [* [' out] (b 00000000000000000000000000001010)]
						(if [9? [fst [' in]]] (b 00000000000000000000000000001001)
						(if [8? [fst [' in]]] (b 00000000000000000000000000001000)
						(if [7? [fst [' in]]] (b 00000000000000000000000000000111)
						(if [6? [fst [' in]]] (b 00000000000000000000000000000110)
						(if [5? [fst [' in]]] (b 00000000000000000000000000000101)
						(if [4? [fst [' in]]] (b 00000000000000000000000000000100)
						(if [3? [fst [' in]]] (b 00000000000000000000000000000011)
						(if [2? [fst [' in]]] (b 00000000000000000000000000000010)
						(if [1? [fst [' in]]] (b 00000000000000000000000000000001)
							(b 00000000000000000000000000000000))))))))))]})) [fst [' l]] (b 00000000000000000000000000000000)}))])
```
`d` can be used as follows:

test.l2
```racket
[putchar (d 65)]
```
The above example is compiled using the command `./bin/l2compile -pdc -program test demort.o - abbreviations.l2 - numbers.l2 - test.l2`.

### Quoting
The `foo` example in the internal representation section shows how tedious writing a function that outputs a symbol can be. The backquote function reduces this tedium. It takes a single s-expression as its argument and, generally, it returns an s-expression that makes that s-expression. The exception to this rule is that if a sub-expression of its input s-expression is of the form `(, expr0)`, then the result of evaluating `expr0` is inserted into that position of the output s-expression. It can be implemented as follows:

backquote.l2:
```racket
(function ` (l)
	[(function aux (s)
		(if [nil? [' s]]
			[lst [sexpr [llllllst [-i-][-n-][-v-][-o-][-k-][-e-][nil]]]
				[lst [sexpr [lllst [-n-][-i-][-l-][nil]]] [nil]]]
		
		(if (if [lst? [' s]] (if [not [nil? [' s]]] (if [lst? [fst [' s]]] (if [not [nil? [fst [' s]]]]
			(if [,? [ffst [' s]]] [nil? [rfst [' s]]] (d 0)) (d 0)) (d 0)) (d 0)) (d 0))
					[frst [' s]]
		
		[lst [sexpr [llllllst [-i-][-n-][-v-][-o-][-k-][-e-][nil]]]
			[lst [sexpr [lllst [-l-][-s-][-t-][nil]]]
				[lst (if [lst? [fst [' s]]]
					[sexpr [aux [fst [' s]]]]
					[sexpr [lst
						[sexpr [llllllst [-i-][-n-][-v-][-o-][-k-][-e-][nil]]]
							[lst [sexpr [lst [---] [lst [fst [' s]] [lst [---] [nil]]]]] [nil]]]])
						[lst [sexpr [aux [rst [' s]]]] [nil]]]]]))) [fst [' l]]])
```
It can be used in the following two equivalent ways:

anotherfunction.l2:
```racket
(function make-A-function (l)
	(` (function A (,[nil]) [putchar (d 65)])))
```
```racket
(function make-A-function (l)
	(`(function A () [putchar (d 65)])))
```
Which can in turn be used as follows:

test.l2:
```racket
(make-A-function)
[A]
```
The above example is compiled using the command `./bin/l2compile -pdc -program test demort.o - abbreviations.l2 - numbers.l2 - backquote.l2 - anotherfunction.l2 - test.l2`.
