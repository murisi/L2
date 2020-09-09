# L2
L2 is a small statically typed programming language. Roughly speaking it looks like Scheme, it behaves like C, and it type-checks like ML. More precisely, L2 has the following characteristics:
 * S-expression syntax
 * [First-class functions](#function)
 * Types are S-expressions
 * [First-class types](#constrain) (at compile-time)
 * [Hindley-Milner type inference](#constraint-system)
 * Neither algebraic nor primitive data types are provided
 * Manual memory management
 * [Procedural unhygienic macros](#meta)
 * [Very general control flow](#jump): `break`=`return`=`longjmp`
 * [Exactly 10 language constructs](#expressions)
 * [Exactly 10 builtin functions](#internal-representation) (all of which are for S-expression manipulation.)

I recommend that you take a look at [the implementation of a self-hosting compiler for L2 that accompanies this project](src/compile.l2) and compare it to [the compiler for bootstrapping it written in C](bootstrap/compile.c) to get a feeling for what L2 is like.

There are [9 language primitives](#expressions) and for each one of them I describe their syntax, what exactly they do in English, the i386 assembly they translate into, and an example usage of them. Following this comes a listing of L2's syntactic sugar. Then comes a brief description of [L2's internal representation and the 9 functions that manipulate it](#internal-representation). After that comes a description of how [a meta-expression](#meta) is compiled. The above descriptions take about 8 pages and are essentially a complete description of L2. Then at the end there is a [list of reductions](#examplesreductions) that shows how some of C's constructs can be defined in terms of L2. Here, I have also demonstrated [closures](#closures) to hint at how more exotic things like coroutines and generators are possible using L2's [continuations](#jump).

### Contents
| **[Getting Started](#getting-started)** | [Expressions](#expressions) | [Examples](#examples) |
|:--- |:--- |:--- |
| [Building L2](#building-l2) | [Constrain](#constrain) | [Numbers](#numbers) |
| [The Compiler](#the-compiler) | [Literal](#literal) | [Commenting](#commenting) |
| **[Syntactic Sugar](#syntactic-sugar)** | [Storage](#storage) | [Backquoting](#backquoting) |
| **[Internal Representation](#internal-representation)** | [If](#if) | [Variable Binding](#variable-binding) |
| **[Constraint System](#constraint-system)** | [Function](#function) | [Boolean Expressions](#boolean-expressions) |
| | [Invoke](#invoke) | [Switch Expression](#switch-expression) |
| | [With](#with) | [Characters](#characters) |
| | [Continuation](#continuation) | [Strings](#strings) |
| | [Jump](#jump) | [Sequencing](#sequencing) |
| | [Meta](#meta) | [Conditional Compilation](#conditional-compilation) |
| | | [Assume](#assume) |
| | | [Fields](#fields) |
| | | [With Variables](#with-variables) |

## Getting Started
### Building L2
```shell
./build_bootstrap
./build_selfhost
```
In this project there are two implementations of L2 compilers. One implementation is the bootstrap compiler written in C, the other implementation is a self-hosting compiler written in L2. (The source code for the self-hosting compiler is larger because it has to define its own control flow, literals, and other such features that come built into C.) Both compilers produce identical object code (modulo padding bytes in the ELFs) when given identical inputs. **The bootstrap compiler needs a Linux distribution running on the x86-64 architecture with the GNU C compiler installed to be compiled successfully.** To bootstrap the L2 compiler, simply run the `bootstrap_compiler` script at the root of the repository. This will create a directory called `bin` containing the file `l2compile`. `l2compile` is a compiler of L2 code and its interface is described in the next section. To self-compile the L2 compiler, simply run the `selfcompile_compiler` script at the root of the repository. This will replace `l2compile` with a new compiler that has the same command line interface.

### The Compiler
```shell
./bin/l2compile source1.l2 ... - intrinsic1 ... - object1.o ...
```
In L2 top-level functions can be invoked at compile-time in addition to run-time. To enable this, the L2 compiler begins by loading the program into memory. For the parts of the program that are object files, the loading is straightforward. For the parts of the program that are L2 files, they cannot simply be compiled and loaded as they may also need to be preprocessed. Hence a lazy compilation scheme is implemented where an object file exposing the same global symbols as the L2 file is loaded, and only later on when one of its functions is actually used as a macro will the compilation of the corresponding L2 function actually be done. The important gain to doing this is that the aforementioned compilation now happens in the environment of the entire program, that is, the program can use its entire self to preprocess itself. Once the program is loaded in memory, its parts are linked together and to the compiler's interface for metaprogramming. And finally each part of the program source is compiled into an object file with the assistance of the copy of itself that has been loaded into memory.

## Expressions

### Constrain
```racket
(constrain expression0 sigfunction0)
```
Evaluates `expression0`. The resulting value of this expression then becomes that of `expression0`.

The `constrain` expression will be further explained in the [constraint system](#constraint-system) section.

### Literal
```racket
(literal b63b62...b0)
```
The resulting value is the 64 bit number specified in binary inside the brackets. Specifying less than or more than 64 bits is an error. Useful for implementing character and string literals, and numbers in other bases.

This expression is implemented by emitting an instruction to `mov` an immediate value into a memory location designated by the surrounding expression.

Say the expression `[putchar x]` prints the character `x`. Then `[putchar (literal 0...01100001)]` prints the text "a" to standard output.

### Storage
```racket
(storage storage0 expression1 expression2 ... expressionN)
```
If this expression occurs inside a function, then space enough for `N` contiguous values has already been reserved in its stack frame. If it is occuring outside a function, then static memory instead has been reserved. `storage0` is a reference to the beginning of this space. This expression evaluates each of its sub-expressions in an environment containing `storage0` and stores the resulting values in contiguous locations of memory beginning at `storage0` in the same order as they were specified. The resulting value of this expression is `storage0`.

`N` contiguous words must be reserved in the current function's stack-frame plan. The expression is implemented by first emitting the instructions for any of the subexpressions with the location of the resulting value fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. And then second emitting an instruction to `lea` of the beginning of the contiguous words into a memory location designated by the surrounding expression.

The expression `[putchar [get (storage _ (literal 0...01100001))]]`, for example, prints the text "a" to standard output.

### If
```racket
(if expression0 expression1 expression2)
```
If `expression0` is non-zero, then only `expression1` is evaluated and its resulting value becomes that of the whole expression. If `expression0` is zero, then only `expression2` is evaluated and its resulting value becomes that of the whole expression.

This expression is implemented by first emitting an instruction to `or` `expression0` with itself. Then an instruction to `je` to `expression2`'s label is emitted. Then the instructions for `expression1` are emitted with the location of the resulting value fixed to the same memory address designated for the resulting value of the `if` expression. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this `if` expression. Then the label for `expression2` is emitted. Then the instructions for `expression2` are emitted with the location of the resulting value fixed to the same memory address designated for the resulting value of the `if` expression.

The expression `[putchar (if (literal 0...0) (literal 0...01100001) (literal 0...01100010))]` prints the text "b" to standard output.

### Function
```racket
(function function0 (param1 param2 ... paramN) expression0)
```
Makes a function to be invoked with exactly `N` arguments. When the function is invoked, `expression0` is evaluated in an environment where `function0` is a reference to the function itself and `param1`, `param2`, up to `paramN` are the resulting values of evaluating the corresponding arguments in the invoke expression invoking this function. Once the evaluation is complete, control flow returns to the invoke expression and the invoke expression's resulting value is the resulting value of evaluating `expression0`. The resulting value of this function expression is a reference to the function.

This expression is implemented by first emitting an instruction to `mov` the address `function0` (a label to be emitted later) into the memory location designated by the surrounding expression. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this function. Then the label named `function0` is emitted. Then instructions to `push` each callee-saved register onto the stack are emitted. Then an instruction to push the frame-pointer onto the stack is emitted. Then an instruction to move the value of the stack-pointer into the frame-pointer is emitted. Then an instruction to `sub` from the stack-pointer the amount of words reserved on this function's stack-frame is emitted. After this the instructions for `expression0` are emitted with the location of the resulting value fixed to a word within the stack-pointer's drop. After this an instruction is emitted to `mov` the word from this location into the register `eax`. And finally, instructions are emitted to `leave` the current function's stack-frame, `pop` the callee-save registers, and `ret` to the address of the caller.

The expression `[putchar [(function my- (a b) [- b a]) (literal 0...01) (literal 0...01100011)]]` prints the text "b" to standard output.

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
The following invocation of it, `(invoke putchar (invoke - (literal 0...01100011) (literal 0...01)))`, prints the text "b" to standard output.

### With
```racket
(with continuation0 expression0)
```
Makes a continuation to the containing expression that is to be `jump`ed to with exactly one argument. Then `expression0` is evaluated in an environment where `continuation0` is a reference to the aforementioned continuation. The resulting value of this expression is that of `expression0` if its evaluation completes. If the continuation `continuation0` is `jump`ed to, then this `with` expression evaluates to the resulting value of the single argument within the responsible `jump` expression.

5+1 words must be reserved in the current function's stack-frame plan. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting instructions to store the program's state at `continuation0`, that is, instructions are emitted to `mov` `ebp`, the address of the instruction that should be executed after continuing (a label to be emitted later), `edi`, `esi`, and `ebx`, in that order, to the first 5 words at `continuation0`. After this, the instructions for `expression0` are emitted. Then an instruction to `jmp` to the end of the entire `with` expression is emitted in order to handle the case where `expression0`'s evaluation completes. Then the label for the first instruction of the continuation is emitted. And finally, an instruction is emitted to `mov` the resulting value of the continuation, the 6th word at `continuation0`, into the memory location designated by the surrounding expression.

#### Examples
Note that the expression `{continuation0 expression0}` `jump`s to the continuation reference given by `continuation0` with resulting value of evaluating `expression0` as its argument. With the note in mind, the expression `(begin [putchar (with ignore (begin {ignore (literal 0...01001110)} [foo] [foo] [foo]))] [bar])` prints the text "nbar" to standard output.

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
The following usage of it, `(with dest [allocate (literal 0...011) dest])`, evaluates to the address of the allocated memory. If allocate had just decreased `esp` and returned, it would have been invalid because L2 expects functions to preserve `esp`.

### Continuation
```racket
(continuation continuation0 (param1 param2 ... paramN) expression0)
```
Makes a continuation to be `jump`ed to with exactly `N` arguments. When the continuation is `jump`ed to, `expression0` is evaluated in an environment where `continuation0` is a reference to the continuation itself and `param1`, `param2`, up to `paramN` are the resulting values of evaluating the corresponding arguments in the `jump` expression `jump`ing to this function. Undefined behavior occurs if the evaluation of `expression0` completes - i.e. programmer must direct the control flow out of `continuation0` somewhere within `expression0`. The resulting value of this `continuation` expression is a reference to the continuation.

5+N words must be reserved in the current function's stack-frame plan. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting an instruction to `mov` the reference `continuation0` into the memory location designated by the surrounding expression. Instructions are then emitted to store the program's state at `continuation0`, that is, instructions are emitted to `mov` `ebp`, the address of the instruction that should be executed after continuing (a label to be emitted later), `edi`, `esi`, and `ebx`, in that order, to the first 5 words at `continuation0`. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this `continuation` expression. Then the label for the first instruction of the continuation is emitted. After this the instructions for `expression0` are emitted.

The expression `{(continuation forever (a b) (begin [putchar a] [putchar b] {forever [- a (literal 0...01)] [- b (literal 0...01)]})) (literal 0...01011010) (literal 0...01111010)}` prints the text "ZzYyXxWw"... to standard output.

### Jump
```racket
(jump continuation0 expression1 expression2 ... expressionN)
{continuation0 expression1 expression2 ... expressionN}
```
Both the above expressions are equivalent. Evaluates `continuation0`, `expression1`, `expression2`, up to `expressionN` in an unspecified order and then `jump`s to `continuation0`, a reference to a continuation, providing it with a local copies of `expression1` up to `expressionN` in order. The resulting value of this expression is unspecified.

`N+1` words must be reserved in the current function's stack-frame plan. The expression is implemented by emitting the instructions for any of the subexpressions with the location of the resulting value fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. Then an instruction to `mov` the first reserved word to 5 words from the beginning of the continuation is emitted, followed by an instruction to `mov` the second reserved word to an address immediately after that, and so on, ending with an instruction to `mov` the last reserved word into the last memory address of that area. The program's state, that is, `ebp`, the address of the instruction that should be executed after continuing, `edi`, `esi`, and `ebx`, in that order, are what is stored at the beginning of a continuation. Instructions to `mov` these values from the buffer into the appropriate registers and then set the program counter appropriately are, at last, emitted.

The expression `(begin (with cutter (jump (continuation cuttee () (begin [bar] [bar] (jump cutter (literal 0...0)) [bar] [bar] [bar])))) [foo])` prints the text "barbarfoo" to standard output.

#### An Optimization
Looking at the examples above where the continuation reference does not escape, `(with reference0 expression0)` behaves a lot like the pseudo-assembly `expression0 reference0:` and `(continuation reference0 (...) expression0)` behaves a lot like `reference0: expression0`. To be more precise, when references to a particular continuation only occur as the `continuation0` subexpression of a `jump` statement, we know that the continuation is constrained to the function in which it is declared, and hence there is no need to store or restore `ebp`, `edi`, `esi`, and `ebx`. Continuations, then, are how efficient iteration is achieved in L2.

## Syntactic Sugar
L2 has exactly 4 pieces of syntactic sugar, the first two of which were already seen in the [invoke](#invoke) and [jump](#jump) sections. They are detailed below:
1) `[frag1 frag2 ... fragN]` desugars to `(invoke frag1 frag2 ... fragN)`.
2) `{frag1 frag2 ... fragN}` desugars to `(jump frag1 frag2 ... fragN)`.
3) `frag1;frag2;...;fragN` desugars to `(frag1 frag2 ... fragN)`.
4) `frag1:frag2:...:fragN` desugars to `(frag1 frag2 ... fragN)`.

Note that `;` has a higher precedence than `:`, so `(a;b:c;d)` would desugar to `(((a b) (c d)))` and `a:b;c:d:e` would desugar to `(a (b c) d e)`. Also note that the latter two pieces of syntactic sugar are provided to enable convenient syntax for quasiquotation, unquote, numerical prefixes, accessing namespaces, and call-by-name expressions.

### Lexical Grammar
So L2 grammar is essentially S-expressions plus two infix list creation operators. To give a more precise description lexically valid L2 programs, L2's grammar is provided in Backus-Naur form below:
* `<program> = <space> (<fragment> <space>)*`
* `<fragment> = <fragment1> | <list1> | <list2>`
* `<list1> = <fragment1> (<space> ':' <space> <fragment1>)+`
* `<fragment1> = <fragment2> | <list2>`
* `<list2> = <fragment2> (<space> ';' <space> <fragment2>)+`
* `<fragment2> = <token> | <list> | <clist> | <slist>`
* `<list> = '(' <space> (<fragment> <space>)* ')'`
* `<clist> = '{' <space> (<fragment> <space>)* '}'`
* `<slist> = '[' <space> (<fragment> <space>)* ']'`
* `<token> = <any character that isn't a ; | : | { | [ | ( | ) | } | ] | <space>>+`

## Internal Representation
After substituting out the syntactic sugar defined in the [syntactic sugar](#syntactic-sugar) section, we find that all L2 programs are just fragments where a fragment is either a token or a list of fragments. And furthermore, every token can be seen as a list of its characters so that for example `foo` becomes `(f o o)`. The following functions that manipulate these fragments are not part of the L2 language and hence the compiler does not give references to them special treatment during compilation. However, when they are used in an L2 meta-program, undefined references to these functions are to be resolved by the compiler.

### `[lst x y b]`
`y` must be a list and `b` a buffer.

Makes a list where `x` is first and `y` is the rest in the buffer `b`.

Say that `a` is the fragment `foo` and `c` is the list `(bar)`. Then `[lst a c b]` is the fragment `(foo bar)`.
### `[token? x]`
`x` must be a fragment.

Evaluates to the one if `x` is also a token. Otherwise evaluates to zero.

Say that `a` is the fragment `foo`. Then `[token? a]` evaluates to `(literal 0...01)`.
### `[gentok b]`
`b` must be a buffer.

Generates in the buffer `b` a token distinct from those previously returned by this function.

Say the token `78sd686` has not previously been returned by `gentok`. Then the call `[gentok b]` might return `78sd686`.
### `[@fst x]`
`x` must be a list.

Evaluates to the first of `x`.

Say that `a` is the list `foo`. Then `[@fst a]` is the character `f`. This `f` is not a list but is a character.
### `[@rst x]`
`x` must be a list.

Evaluates to a list that is the rest of `x`.

Say that `a` is the list `foo`. Then `[@rst a]` is the fragment `oo`.
### `emt`
Evaluates to the empty list.

Say that `a` is the fragment `foo`. Then `[lst a emt]` is the fragment `(foo)`.
### `[emt? x]`
`x` must be a list.

Evaluates to the one if `x` is the empty list. Otherwise evaluates to zero.

`[emt? emt]` evaluates to `(literal 0...01)`.
### `-<character>-`
Evaluates to the character `<character>`.

Say that `b` is a buffer. Then the expression `[lst -f- [lst -o- [lst -o- emt b] b] b]` evaluates to the fragment `foo`.
### `[char= x y]`
`x` and `y` must be characters.

Evaluates to one if `x` is the same character as `y`, otherwise it evaluates to zero.

Say that `x` and `y` are the character `d`. Then `[char= x y]` evaluates to `(literal 0...01)`.

### `[var b]`
`b` must be a buffer.

Makes a new variable in the buffer `b`.

Say `a` is the list `(bar)`. Then `[lst [var b] a b]` is the fragment `(!1231 bar)`, where `!1231` is a representation of the variable.

### `[var? a]`
`a` must be a fragment.

Evaluates to one if `a` is also a variable. Otherwise evaluates to zero.

Say that `a` is the fragment `foo`. Then `[var? a]` evaluates to `(literal 0...00)`.

### `[var= x y]`

`x` and `y` must be variables.

Evaluates to one if `x` is the same character as `y`, otherwise it evaluates to zero.

Say `b` is a buffer. Then `[var= [var b] [var b]]` evaluates to `(literal 0...00)`.

## Expressions Continued
### Meta
```racket
(function0 expression1 ... expressionN)
```
If the above expression is not listed above, then the function `function0` is invoked with the (unevaluated) list of [fragments](#internal-representation) `(expression1 expression2 ... expressionN)` as its first argument and a buffer in which the replacement is to be constructed as its second argument. The fragment returned by this function then replaces the entire fragment `(function0 expression1 ... expressionN)`. The returned fragment must not contain any variable fragments. If the result of this replacement contains a meta-expression, then the above process is repeated. When this process terminates, the appropriate assembly code for the resulting expression is emitted.

Meta-expressions were already demonstrated in the [compiler section](#the-compiler).

## Examples
In the extensive list processing that follows in this section, the following functions prove to be convenient abbreviations:
#### abbreviations.l2
```racket
(function @frst (l) [@fst [@rst l]])
(function @ffrst (l) [@fst [@frst l]])
(function @frfrst (l) [@fst [@rst [@frst l]]])
(function @rrst (l) [@rst [@rst l]])
(function @rrrst (l) [@rst [@rrst l]])
(function @rfst (l) [@rst [@fst l]])
(function @frfst (l) [@fst [@rfst l]])
(function @frrfst (l) [@fst [@rst [@rfst l]]])
(function @frrst (l) [@fst [@rst [@rst l]]])
(function @frrrst (l) [@fst [@rst [@rst [@rst l]]]])
(function @frrrrst (l) [@fst [@rst [@rst [@rst [@rst l]]]]])
(function @frrrrrst (l) [@fst [@rst [@rst [@rst [@rst [@rst l]]]]]])
(function @ffst (l) [@fst [@fst l]])
(function llst (a b c r) [lst a [lst b c r] r])
(function lllst (a b c d r) [lst a [llst b c d r] r])
(function llllst (a b c d e r) [lst a [lllst b c d e r] r])
(function lllllst (a b c d e f r) [lst a [llllst b c d e f r] r])
(function llllllst (a b c d e f g r) [lst a [lllllst b c d e f g r] r])
(function lllllllst (a b c d e f g h r) [lst a [llllllst b c d e f g h r] r])
```

### Numbers
Integer literals prove to be quite tedious in L2 as can be seen from some of the examples in the expressions section. The following function, `#`, implements decimal arithmetic for x86-64 by reading in a token in base 10 and writing out the equivalent fragment in base 2:

#### numbers64.l2
```racket
(function =# (binary r)
  [lst [lllllllst -l- -i- -t- -e- -r- -a- -l- emt r]
    [lst (with return {(continuation write (count in out)
        (if count
          {write [- count (literal 0...01)]
            [>> in (literal 0...01)]
            [lst (if [land in (literal 0...01)]
              -1- -0-) out r]}
          {return out}))
        (literal 0...01000000) binary emt})
      emt r]r])

(function # (l r) [=#
  (with return {(continuation read (in out)
    (if [emt? in]
      {return out}
      {read [@rst in] [+ [* out (literal 0...01010)]
        (if [char= [@fst in] -9-] (literal 0...01001)
        (if [char= [@fst in] -8-] (literal 0...01000)
        (if [char= [@fst in] -7-] (literal 0...0111)
        (if [char= [@fst in] -6-] (literal 0...0110)
        (if [char= [@fst in] -5-] (literal 0...0101)
        (if [char= [@fst in] -4-] (literal 0...0100)
        (if [char= [@fst in] -3-] (literal 0...011)
        (if [char= [@fst in] -2-] (literal 0...010)
        (if [char= [@fst in] -1-] (literal 0...01)
          (literal 0...0))))))))))]}))
    [@fst l] (literal 0...0)}) r])
```
#### test2.l2
```racket
[putchar (# 65)]
```
##### or equivalently
```racket
[putchar #65]
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 test2.l2 - "bin/x86_64.o"
```

### Commenting
L2 has no built-in mechanism for commenting code written in it. The following comment function takes a list of fragments as its argument and returns an empty begin expression effectively causing its arguments to be ignored. Its implementation and use follows:

#### comments.l2
```racket
(function ignore (l r) [=# #123456789 r])
```

#### test3.l2
```racket
(ignore This is a comment, take no notice.)
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 test3.l2 - "bin/x86_64.o"
```

### Backquoting
The `foo` example in the internal representation section shows how tedious writing a function that outputs a token can be. The backquote function reduces this tedium. It takes a fragment and a buffer as its argument and, generally, it returns a fragment that makes that fragment. The exception to this rule is that if a sub-expression of its input fragment is of the form `(, expr0)`, then the fragment `expr0` is inserted verbatim into that position of the output fragment. Backquote can be implemented and used as follows:

#### backquote.l2
```racket
(function ` (l r)
  [(function aux (s t r)
    (if [emt? s] [lllst -e- -m- -t- emt r]

    (if (if [emt? s] #0 (if [token? s] #0 (if [emt? [@fst s]]
      #0 (if [char= [@ffst s] -,-] [emt? [@rfst s]] #0))))
          [@frst s]

    [lllllst [llllllst -i- -n- -v- -o- -k- -e- emt r]
      [lllst -l- -s- -t- emt r]
        (if [token? s]
            [lllst --- [@fst s] --- emt r]
            [aux [@fst s] t r])
          [aux [@rst s] t r] t emt r]))) [@fst l] [@frst l] r])
```
#### anotherfunction.l2:
```racket
(function make-A-function (l r)
  (` (function A (,emt) [putchar #65]) r))
```
##### or equivalently
```racket
(function make-A-function (l)
  (`(function A () [putchar #65])r))
```
#### test4.l2
```racket
[(make-A-function)]
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 anotherfunction.l2 test4.l2 - "bin/x86_64.o"
```

### Variable Binding
Variable binding is enabled by the `continuation` expression. `continuation` is special because, like `function`, it allows identifiers to be bound. Unlike `function`, however, expressions within `continuation` can directly access its parent function's variables. The `let` binding function implements the following transformation:
```racket
(let (params args) ... expr0)
->
(with let:return
  {(continuation let:aux (params ...)
    {let:return expr0}) vals ...})
```
It is implemented and used as follows:
#### let.l2
```racket
(ignore
  Reverses the given list. l is the list to be reversed. r is the buffer into
  which the reversed list will be put. Return value is the reversed list.)

(function meta:reverse (l r)
  (with return
    {(continuation _ (l reversed)
      (if [emt? l]
        {return reversed}
        {_ [@rst l] [lst [@fst l] reversed r]})) l emt}))

(ignore
  Maps the given list using the given function. l is the list to be mapped. ctx
  is always passed as a second argument to the mapper. mapper is the two argument
  function that will be supplied a list item as its first argument and ctx as its
  second argument and will return an argument that will be put into the corresponding
  position of another list. r is the buffer into which the list being constructed
  will be put. Return value is the mapped list.)

(function meta:map (l ctx mapper r)
  (with return
    {(continuation aux (in out)
      (if [emt? in]
        {return [meta:reverse out r]}
        {aux [@rst in] [lst [mapper [@fst in] ctx] out r]})) l emt}))

(function let (l r)
  (`(with let:return
    (,[llst (` jump r) (`(continuation let:aux (,[meta:map [@rst [meta:reverse l r]] (ignore) @fst r])
      {let:return (,[@fst [meta:reverse l r]])}) r) [meta:map [@rst [meta:reverse l r]] (ignore) @frst r] r])) r))
```
#### test5.l2
```
(let (x #12)
  (let (what? (function _ () [printf (" x is %i) x]))
    [what?]))
```
Note in the above code that `what?` is only able to access `x` because `x` is defined outside of all functions and hence is statically allocated. Also note that L2 permits identifier shadowing, so `let` expressions can be nested without worrying, for instance, about the impact of an inner `templet0` on an outer one.

#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 test5.l2 - "bin/x86_64.o"
```

### Boolean Expressions
The Boolean literals true and false are achieved using macros that return the same literal fragment regardless of the arguments supplied to them. Short-circut Boolean expressions are achieved through the `if` expression. The `if` expression is special because it has the property that only two out of its three sub-expressions are evaluated when it itself is evaluated. Now, the Boolean expressions implement the following transformations:
```racket
(false) -> (literal #0)

(true) -> (literal #1)

(or expr1 expr2 ... exprN)
->
(let (or:temp expr1) (if or:temp
  or:temp
  (let (or:temp expr2) (if or:temp
    or:temp
    ...
      (let (or:temp exprN) (if or:temp
        or:temp
        (false)))))))

(and expr1 expr2 ... exprN)
->
(let (and:temp expr1) (if and:temp
  (let (and:temp expr2) (if and:temp
    ...
      (let (and:temp exprN) (if and:temp
        (true)
        and:temp))
    and:temp))
  and:temp))

(not expr1)
->
(if expr1 (false) (true))
```
These transformations are implemented and used as follows:
#### boolean.l2
```racket
(function false (l r) [=# r #0])

(function true (l r) [=# r #1])

(function or (l r) (with return
  {(continuation loop (l sexpr)
      (if [emt? l]
        {return sexpr}
        {loop [@rst l] (`(let (or:temp (,[@fst l])) (if or:temp or:temp (, sexpr r)))r)}))
    [meta:reverse l r] (`(false)r)}))

(function and (l r) (with return
  {(continuation loop (l sexpr)
      (if [emt? l]
        {return sexpr}
        {loop [@rst l] (`(let (and:temp (,[@fst l])) (if and:temp (, sexpr r) and:temp))r)}))
    [meta:reverse l r] (`(true)r)}))

(function not (l r) (`(if (,[@fst l]) (false) (true))r))
```
#### test6.l2
```racket
(and (false) [/ #1 #0])
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 test6.l2 - "bin/x86_64.o"
```

### Switch Expression
Now we will implement a variant of the switch statement that is parameterized by an equality predicate. The `switch` selection function will, for example, do the following transformation:
```racket
(switch eq0 val0 (vals exprs) ... expr0)
->
(let (tempeq0 eq0) (tempval0 val0)
  (if [tempeq0 tempval0 vals1]
    exprs1
    (if [tempeq0 tempval0 vals2]
      exprs2
      ...
        (if [tempeq0 tempval0 valsN]
          exprsN
          expr0))))
```
It is implemented and used as follows:
#### switch.l2
```racket
(function switch (l r)
  (`(let (switch:= (,[@fst l])) (switch:val (,[@frst l]))
    (,(with return
      {(continuation aux (remaining else-clause)
        (if [emt? remaining]
          {return else-clause}
          {aux [@rst remaining]
            (`(if (,[lst (` or r) [meta:map [@rst [meta:reverse [@fst remaining] r]] r
                (function _ (e r) [llllst (` invoke r) (` switch:= r) (` switch:val r) e emt r]) r] r])
              (,[@fst [meta:reverse [@fst remaining] r]]) ,else-clause) r)}))
        [@rst [meta:reverse [@rrst l] r]] [@fst [meta:reverse l r]]})))r))
```
#### test7.l2
```
(switch = #10
  (#20 [printf (" d is 20!)])
  (#10 [printf (" d is 10!)])
  (#30 [printf (" d is 30!)])
  [printf (" s is something else.)])
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 test7.l2 - "bin/x86_64.o"
```

### Characters
With `#` implemented, a somewhat more readable implementation of characters is possible. The `char` function takes a singleton list containing a token of one character and returns its ascii encoding using the `#` expression. Its implementation and use follows:

#### characters.l2
```
(function meta:char-aux (l) (switch char= l
  (-!- #33) (-"- #34) (-#- #35) (-$- #36) (-%- #37) (-&- #38) (-'- #39) (-*- #42)
  (-+- #43) (-,- #44) (--- #45) (-.- #46) (-/- #47) (-0- #48) (-1- #49) (-2- #50)
  (-3- #51) (-4- #52) (-5- #53) (-6- #54) (-7- #55) (-8- #56) (-9- #57) (-:- #58)
  (-;- #59) (-<- #60) (-=- #61) (->- #62) (-?- #63) (-@- #64) (-A- #65) (-B- #66)
  (-C- #67) (-D- #68) (-E- #69) (-F- #70) (-G- #71) (-H- #72) (-I- #73) (-J- #74)
  (-K- #75) (-L- #76) (-M- #77) (-N- #78) (-O- #79) (-P- #80) (-Q- #81) (-R- #82)
  (-S- #83) (-T- #84) (-U- #85) (-V- #86) (-W- #87) (-X- #88) (-Y- #89) (-Z- #90)
  (-\- #92) (-^- #94) (-_- #95) (-`- #96) (-a- #97) (-b- #98) (-c- #99) (-d- #100)
  (-e- #101) (-f- #102) (-g- #103) (-h- #104) (-i- #105) (-j- #106) (-k- #107) (-l- #108)
  (-m- #109) (-n- #110) (-o- #111) (-p- #112) (-q- #113) (-r- #114) (-s- #115) (-t- #116)
  (-u- #117) (-v- #118) (-w- #119) (-x- #120) (-y- #121) (-z- #122) (-|- #124) (-~- #126)
  #0))

(function char (l r) [=# [meta:char-aux [@ffst l]] r])
```
#### test8.l2
```racket
[putchar (char A)]
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 test8.l2 - "bin/x86_64.o"
```

### Strings
The above exposition has purposefully avoided making strings because it is tedious to individually store each character literal in memory. The quote function takes a list of tokens and returns the sequence of operations required to write its ascii encoding into memory. (An extension to this rule occurs when instead of a token, a fragment that is a list of fragments is encountered. In this case the value of the fragment is taken as the character to be inserted.) These "operations" are essentially reserving enough storage for the bytes of the input, putting the characters into that memory, and returning the address of that memory. Because the stack-frame of a function is destroyed upon its return, strings implemented in this way should not be returned. Quote is implemented below:

#### strings.l2
```
(function " (l r)
  (let (buf (storage _ (ignore)))
    (loop add-word (str l) (index #0) (exprs emt) (instrs emt)
      (let (sub-index [rem index (unit)])
        (if [emt? str]
          (`(let (dquote:str (,[llst (` storage r) (` dquote:tmp r) [meta:reverse exprs r] r]))
              (,[lst (` do r) [meta:reverse [lst (`(constrain dquote:str (\ k (` string k)))r) instrs r]r]r]))r)
        
        (if (and [emt? [@fst str]] [emt? [@rst str]]) (do
          [setb [+ buf sub-index] (nul)]
          {add-word [@rst str] [+ index #1] [lst [=# $buf r] exprs r] instrs})
            
        (if (and [emt? [@fst str]] [token? [@frst str]]) (do
          [setb [+ buf sub-index] (space)]
          {add-word [@rst str] [+ index #1] (if [= sub-index #7] [lst [=# $buf r] exprs r] exprs) instrs})
        
        (if [emt? [@fst str]] {add-word [@rst str] index exprs instrs}
        
        (if [token? [@fst str]] (do
          [setb [+ buf sub-index] [meta:char-aux [@ffst str]]]
          {add-word [lst [@rfst str] [@rst str] r] [+ index #1] (if [= sub-index #7] [lst [=# $buf r] exprs r] exprs) instrs})
          
          {add-word [@rst str] [+ index #1] (if [= sub-index #7] [lst [=# $buf r] exprs r] exprs)
            [lst (`[setb [+ dquote:str (,[=# index r])] (,[@fst str])]r) instrs r]})))))))))
```
#### test9.l2
```
[printf (" This is how the quote macro is used. (# 10) Now we are on a new line because 10 is a line feed.)]
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 strings.l2 test9.l2 - "bin/x86_64.o"
```

### Sequencing
It is often desirable to sequence the execution of expressions. More specifically, we want a macro that executes its expressions in order and then evaluates to the result of evaluating its last expression. The following macro, `do`, implements the following transformation:
```racket
(do expr1 expr2 ... exprN)
->
(with do:return
  {(continuation do:cont (do:arg)
    {(continuation do:cont (do:arg)
      {...
        {(continuation do:cont (do:arg) {do:return do:arg}) exprN}...}) expr2}) expr1}))
```
It is implemented and used as follows:
#### do.l2
```racket
(function do (l r)
  (`(with do:return
    (,(loop make-do (acc (`{do:return do:arg}r)) (exprs [meta:reverse l r])
        (if [emt? exprs]
          acc
          {make-do (`{(continuation do:cont (do:arg) ,acc) (,[@fst exprs])}r) [@rst exprs]}))))r))
```
#### test10.l2
```racket
(do
  [putchar (char A)]
  [putchar (char B)]
  [putchar (char C)])
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 strings.l2 do.l2 test10.l2 - "bin/x86_64.o"
```
### Conditional Compilation
Up till now, references to functions defined elsewhere have been the only things used as the first subexpression of an expression. Sometimes, however, the clarity of the whole expression can be improved by inlining the function. The following code proves this in the context of conditional compilation.
#### test11.l2
```
((if [> #10 #20] @fst @frst)
  [printf (" I am not compiled!)]
  [printf (" I am the one compiled!)])
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 strings.l2 test11.l2 - "bin/x86_64.o"
```
### Assume
There are far fewer subtle ways to trigger undefined behaviors in L2 than in other unsafe languages because L2 does not have dereferencing, arithmetic operators, types, or other such functionality built in; the programmer has to implement this functionality themselves in [assembly routines callable from L2](assets/x86_64.s). This shift in responsibility means that any L2 compiler is freed up to treat invocations of undefined behaviors in L2 code as intentional. The following usage of undefined behavior within the function `assume` is inspired by [Regehr](https://blog.regehr.org/archives/1096). The function `assume`, which compiles `y` assuming that the condition `x` holds, implements the following transformation.
```racket
(assume x y)
->
(with return
  {(continuation tempas0 ()
    (if x {return y} (ignore)))})
```
This is implemented as follows:
#### assume.l2
```racket
(function assume (l r)
  (`(with assume:return
    {(continuation assume:tempas0 ()
      (if (,[@fst l]) {assume:return (,[@frst l])} (ignore)))})r))
```
#### test12.l2
```
(function foo (x y)
  (assume [not [= x y]] (begin
    [setb x (char A)]
    [setb y (char B)]
    [printf (" %c) [getb x]])))

[foo (" C) (" D)]
```
In the function `foo`, if `x` were equal to `y`, then the else branch of the `assume`'s `if` expression would be taken. Since this branch does nothing, `continuation`'s body expression would finish evaluating. But this is the undefined behavior stated in [the first paragraph of the description of the `continuation` expression](#continuation). Therefore an L2 compiler does not have to worry about what happens in the case that `x` equals `y`. In light of this and the fact that the `if` condition is pure, the whole `assume` expression can be replaced with the first branch of `assume`'s `if`  expression. And more importantly, the the first branch of `assume`'s `if` expression can be optimized assuming that `x` is not equal to `y`. Therefore, a hypothetical optimizing compiler would also replace the last `[getb x]`, a load from memory, with `(char A)`, a constant.

#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 strings.l2 assume.l2 test12.l2 - "bin/x86_64.o"
```
Note that the `assume` expression can also be used to achieve C's `restrict` keyword simply by making its condition the conjunction of inequalities on the memory locations of the extremeties of the "arrays" in question.

### Fields
L2 has no built-in mechanism for record and union types. The most naive way to do record types in L2 would be to create a getter function, setter function, and offset calculation function for every field where these functions simply access and/or mutate the desired memory locations. However this solution is untenable because of the amount of boilerplate that one would have to write. A better solution is to aggregate the offset, size, getter, and setter of each field into a higher-order macro that supplies this information into any macro that is passed to it. This way, generic getter, setter, address-of, offset-of, and sizeof functions can be defined once and used on any field. More concretely, the following transformations are what we want:
```racket
(offset-of expr0)
->
(expr0 offset-aux)

(offset-aux expr0 ...)
->
expr0

(size-of expr0)
->
(expr0 size-of-aux)

(size-of-aux expr0 expr1 ...)
->
expr1

(getter-of expr0)
->
(expr0 getter-of-aux)

(getter-of-aux expr0 expr1 expr2 ...)
->
expr2

(setter-of expr0)
->
(expr0 setter-of-aux)

(setter-of-aux expr0 expr1 expr2 expr3 ...)
->
expr3

(& expr0 expr1)
->
(expr0 &-aux expr1)

(&-aux expr0 expr1 expr2 expr3 expr4 ...)
->
[+ expr4 expr0]

(@ expr0 expr1)
->
(expr0 @-aux expr1)

(@-aux expr0 expr1 expr2 expr3 expr4 ...)
->
[expr2 [+ expr4 expr0]]

(setf expr0 expr1 expr2)
->
(expr0 setf-aux expr1 expr2)

(setf-aux expr0 expr1 expr2 expr3 expr4 expr5)
->
[expr3 [+ expr4 expr0] expr5]
```

Why? Because if we define the macro `car` by the transformation `(car expr0 exprs ...) -> (expr0 #0 #8 get8b set8b exprs ...)` and `cdr` by the transformation `(cdr expr0 exprs ...) -> (expr0 #8 #8 get8b set8b exprs ...)`, then we get the following outcomes:
```racket
(offset-of car) -> (car offset-of-aux) -> (offset-of-aux #0 #8 get8b set8b) -> #0
(size-of car) -> (car size-of-aux) -> (size-of-aux #0 #8 get8b set8b) -> #8
(getter-of car) -> (car getter-of-aux) -> (getter-of-aux #0 #8 get8b set8b) -> get8b
(setter-of car) -> (car setter-of-aux) -> (setter-of-aux #0 #8 get8b set8b) -> set8b
(& car expr) -> (car &-aux expr) -> (&-aux #0 #8 get8b set8b expr) -> [+ expr #0]
(@ car expr) -> (car @-aux expr) -> (@-aux #0 #8 get8b set8b expr) -> [get8b [+ expr #0]]
(setf car expr val) -> (car setf-aux expr val) -> (setf-aux #0 #8 get8b set8b expr val) -> [set8b [+ expr #0] val]

(offset-of cdr) -> ... -> #8
(size-of cdr) -> ... -> #8
(getter-of cdr) -> ... -> get8b
(setter-of cdr) -> ... -> set8b
(& cdr expr) -> ... -> [+ expr #8]
(@ cdr expr) -> ... -> [get8b [+ expr #8]]
(setf cdr expr val) -> ... -> [set8b [+ expr #8] val]
```
To recapitulate, we localized and separated out the definition of a field from the various operations that can be done on it. Since dozens of fields can potentially be used in a program, it makes sense to define a helper function, `mk-field`, that creates them. What follows is the implementation of this helper function and the aforementioned transformations:
#### fields.l2
```racket
(function offset-of (l r) (`((,[@fst l]) offset-of-aux)r))

(function offset-of-aux (l r) [@fst l])

(function size-of (l r) (`((,[@fst l]) size-of-aux)r))

(function size-of-aux (l r) [@frst l])

(function getter-of (l r) (`((,[@fst l]) getter-of-aux)r))

(function getter-of-aux (l r) [@frrst l])

(function setter-of (l r) (`((,[@fst l]) setter-of-aux)r))

(function setter-of-aux (l r) [@frrrst l])

(function & (l r) (`((,[@fst l]) &-aux (,[@frst l]))r))

(function &-aux (l r) (`[+ (,[@frrrrst l]) (,[@fst l])]r))

(function @ (l r) (`((,[@fst l]) @-aux (,[@frst l]))r))

(function @-aux (l r) (`[(,[@frrst l]) [+ (,[@frrrrst l]) (,[@fst l])]]r))

(function setf (l r) (`((,[@fst l]) setf-aux (,[@frst l]) (,[@frrst l]))r))

(function setf-aux (l r) (`[(,[@frrrst l]) [+ (,[@frrrrst l]) (,[@fst l])] (,[@frrrrrst l])]r))

(function mk-field (l r offset size)
  [lllllst [@fst l] [=# offset r] [=# size r]
    (switch = size (#1 (` get1b r)) (#2 (` get2b r)) (#4 (` get4b r)) (#8 (` get8b r)) (`(ignore)r))
    (switch = size (#1 (` set1b r)) (#2 (` set2b r)) (#4 (` set4b r)) (#8 (` set8b r)) (`(ignore)r))
    [@rst l] r])
```
#### somefields.l2
```racket
(function cons-cell (l r) [=# r #16])

(function car (l r) [mk-field l r #0 #8])

(function cdr (l r) [mk-field l r #8 #8])
```
#### test13.l2
```racket
(storage mycons (ignore) (ignore))
(setf car mycons (char A))
(setf cdr mycons (char a))
[putchar (@ car mycons)]
[putchar (@ cdr mycons)]
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 boolean.l2 switch.l2 characters.l2 strings.l2 assume.l2 fields.l2 somefields.l2 test13.l2 - "bin/x86_64.o"
```
Note that there is no struct definition in the code, there are only definitions of the fields we need to work with. The negative consequence of this is that we lose C's type safety and portability. The positive consequences are that we gain control over the struct packing, we are now able to use the same field definitions across several conceptually different structs, and that we can overlap our fields in completely arbitrary ways.

### With Variables
Due to its use of Hindley-Milner type inference, L2 supports parametric polymorphism. However, specifying type signatures unaided by macros turns out to be tedious because the desired type variables must be manually created in the supplied buffer, and then bound to a symbol. The macro `with-vars` reduces this tedium by implementing the following transformation:
```racket
(with-vars (vars ...) expr buffer)
->
(let (var0 [var buffer]) ... (varN [var buffer]) expr))
```
This is implemented and used as follows:
#### with-vars.l2
```racket
(function with-vars (l r)
  (let (bindings [meta:map3 [@fst l] [@frrst l] r (function _(e s r) (`(,e [var ,s])r)) r])
    [lst (` let r) [meta:reverse [lst [@frst l] [meta:reverse bindings r] r] r] r]))
```
#### test14.l2
```racket
(constrain nil (function _(r) (with-vars (a) (`(list ,a)r)r)))
```
#### shell
```shell
./bin/l2compile abbreviations.l2 comments.l2 numbers64.l2 backquote.l2 let.l2 with-vars.l2 test14.l2 - "bin/x86_64.o"
```
Note that the above code simply constrains the signature of the symbol `nil` to be of the form `(list !2342)`, where `!2342` is a representation of the (type) variable. Also note that the signature is contained within a function, this is because the compiler needs a way to supply a buffer to the fragment manipulation functions.

## Constraint System
L2 has a static constraint system based on Hindley-Milner type inference. Every expression is associated with exactly one fragment. This fragment is called the expression's signature. For a program to compile, its expressions when taken as a whole must pass the constraint check. The constraint check is specified below:
1. Partition all of the expressions of the program into strongly connected components, where dependency is determined as follows:
   * Every non-`continuation` expression is dependent upon its children
   * A symbol is mutually dependent with the expression that defines it
   * A `constrain` expression is in addition depended upon by its child
   * A `jump` expression's target depends upon the `jump` expression's arguments
2. Now iterate through the strongly connected components in topological order and for each component, do the following:
   1. Generate the constraint equations corresponding to each expression in the manner prescribed below
   2. Execute a unification algorithm on the constrain equations that has just been generated
   3. If the algorithm yields a most general unifier, then substitute in the solutions for the variable fragments corresponding to expressions within this component
   4. **If the algorithm does not yield a most general unifier, then the program fails the constraint check**
### Constrain
A constrain expression is provided to enable the programmer to directly constrain the signature of the contained expression. (Naturally, if the specified signature contains no variable fragments, then you are essentially fixing an expression's signature.) This is why the following constraints are generated for a constrain expression `(constrain b f)`:
* Let `g` be the expression's signature.
* Let `i` be the signature obtained from evaluating `f`.
* Let `h` be `b`'s signature.
* Then `g = i = h`.

For example, the following program will not pass the constraint check because `hello` does not unify with `world`:
```racket
(constrain someid (function _(r) (` hello r)))
(constrain someid (function _(r) (` world r)))
```
### Literal
No constraints are generated for a literal expression. The intuition behind this decision is that it is often desirable to reinterpret literals. For example, if the literal is the address of a function, it may be desirable to treat it like a function that can be invoked.

For example, the following program will pass the constraint check:
```racket
(constrain (literal 0...01) (function _(r) (` hello r)))
```
### Storage
No constraints are generated for a storage expression. The intuition behind this decision is that it is often desirable to reinterpret a sequence of bytes. For example, if the storage expression contains executable code, it may sometimes be desirable to treat it like a function that can be invoked.

For example, the following program will pass the constraint check:
```racket
(constrain (storage somestorage (literal 0...01)) (function _(r) (` hello r)))
```
### If
For an if expression, we want to capture the intuition that the resulting value of the entire expression can either be that of its consequent or that of its alternate, hence all their signatures must match up. Hence for an if expression `(if f p b)`, the following constraints are generated:
* Let `e` be the expression's signature.
* Let `g` be `p`'s signature.
* Let `i` be `b`'s signature.
* Then `e = g` and `g = i`.

For example, the following program will not pass the constraint check because the signatures of the branches of the `if` expression cannot be unified:
```racket
(if (literal 0...01)
  (constrain consequent (function _(r) (` hello r)))
  (constrain alternate (function _(r) (` world r))))
```
### Function
For a function expression, we want to capture the intuition that its signature is dependent on that of its parameters and body. Hence for a function expression `(function f (p1 p2 ... pN) b)`, the following constraint is generated:
* Let `g` be the expression's signature.
* Let `h1, h2, ..., hN` be the signatures corresponding to `p1, p2, ..., pN`.
* Let `i` be `b`'s signature.
* Then `g = (function (h1 h2 ... hN) i)`.

For example, the following program will not pass the constraint check because the supplied function signature has an incorrect parameter number:
```racket
(constrain (function (a) a) (function _(r) (`(function (sig sig) sig)r)))
```
### Invoke
For an invoke expression, we want to capture the intuition that the signatures of the function's parameters must match those of the arguments, and that signature of the function's body must match the signature of the entire invoke expression. Hence for an invoke expression `(invoke f a1 a2 ... aN)`, the following constraint is generated:
* Let `e` be the expression's signature.
* Let `g` be `f`'s signature.
* Let `h1, h2, ..., hN` be the signatures corresponding to `a1, a2, ..., aN`.
* Then `g = (function (h1 h2 ... hN) e)`.

For example, the following program will pass the constraint check because `id` is polymorphic:
```racket
(constrain id (function _(r) (with-vars (a) (`(function (,a) ,a)r)r)))
(function id (x) x)

[id (constrain (literal 0...01) (function _(r) (` hello r)))]
[id (constrain (literal 0...01) (function _(r) (` world r)))]
```
### With
For a with expression, we want to capture the intuition that the with expression's resulting value can be that of its body, hence the with expression's signature must match that of its body, or it can be that of the value its continuation is called with, hence the expression's signature must match that of its continuation parameter. Hence for a with expression `(with f b)`, the following constraints are generated:
* Let `e` be the expression's signature.
* Let `g` be `f`'s signature.
* Let `h` be `b`'s signature.
* Then `g = (continuation (e))` and `e = h`.
### Continuation
For a continuation expression, we want to capture the inution that its signature is dependent only on its parameters (since a continuation's body can never fully evaluate). Hence for a continuation expression `(continuation f (p1 p2 ... pN) b)`, the following constraint is generated:
* Let `g` be the expression's signature.
* Let `h1, h2, ..., hN` be the signatures corresponding to `p1, p2, ..., pN`.
* Then `g = (continuation (h1 h2 ... hN))`.

For example, the following program will not pass the constraint check because the supplied continuation signature has an incorrect parameter number:
```racket
(constrain (continuation (a) a) (function _(r) (`(continuation (sig sig) sig)r)))
```
### Jump
For a jump expression, we want to capture the intuition that the signatures of the continuation's parameters must match those of the jump expression's arguments (since a jump expression can never fully evaluate). Hence for an jump expression `(jump f a1 a2 ... aN)`, the following constraints are generated:
* Let `g` be `f`'s signature.
* Let `h1, h2, ..., hN` be the signatures corresponding to `a1, a2, ..., aN`.
* Then g = `(continuation (h1 h2 ... hN))`.

For example, the following program will not pass the constraint check because the `jump` has an incorrect parameter number:
```racket
(with cont {cont a b})
```
### Meta
For a meta expression, we want to capture the intuition that the meta-expression is indistinguishable from its expansion, and therefore that their signatures are the same. Hence for a meta expression `(f0 f1 f2 ... fN)`, the following constraints are generated:
* Let `g` be the expression's signature.
* Let `h` be the signature of the meta expression's expansion.
* Then `g = h`.
