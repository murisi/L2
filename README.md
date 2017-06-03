# L2
## Introduction
L2 is an attempt to find the smallest most distilled programming language equivalent to C. The goal is to turn as much of C's preprocessor directives, control structures, statements, literals, and functions requiring compiler assistance (setjmp, longjmp, alloca, ...) into things definable inside L2. The language does not surject to all of C, its most glaring omission being that of a type-system. However, I reckon the result is still pretty interesting.

The approach taken to achieve this has been to make C's features more composable, more multipurpose, and, at least on one occasion, add a new feature so that a whole group of distinct features could be dropped. In particular, the most striking changes are that C's:
1. irregular syntax is replaced by S-expressions; because simple syntax composes well with a non-trivial preprocessor (and no, I have not merely transplanted Common Lisp's macros into C)
2. loop constructs is replaced with what I could only describe as a more structured variant of setjmp and longjmp without stack destruction (and no, there is no performance overhead associated with this)

The entirity of the language can be communicated in less than 3 pages. There are 10 language primitives and for each one of them I describe their syntax, what exactly they do in English, the i386 assembly they translate into, and an example usage of them. Following this comes a brief description of L2's internal representation and the 5 functions (loosely speaking) that manipulate it. Following this comes a sort of "glossary" that shows how not only C's constructs, but more exotic stuff like coroutines, Python's generators, and Scheme's lambdas can be defined in terms of L2.

## Primitives
### Begin
```scheme
(begin expression1 expression2 ... expressionN)
```
Evaluates its subexpressions sequentially from left to right. That is, it evaluates `expression1`, then `expression2`, and so on, ending with the execution of `expressionN`. Specifying zero subexpressions is valid. The return value is unspecified.

This expression is implemented by emitting the instructions for `expression1`, then emitting the instructions for `expression2` immediately afterwords and so on, ending with the emission of `expressionN`.

Say the expression `[foo]` prints the text "foo" to standard output and the expression `[bar]` prints the text "bar" to standard output. Then `(begin [foo] [bar] [foo] [foo] [foo])` prints the text "foobarfoofoofoo" to standard output.

### Binary
```scheme
(b b31b30...b0)
```
The return value is the 32 bit number specified in binary inside the brackets. Specifying less than or more than 32 bits is an error. Useful for implementing character and string literals, and numbers in other bases.

This expression is implemented by loading an immediate value into a memory location determined by the surrounding expression. The pseudo code is `movl 0bb31b30...b0, N(%ebp)`.

Say the expression `[putchar x]` prints the character `x`. Then `[putchar (b 00000000000000000000000001100001)]` prints the text "a" to standard output.

### Function
```scheme
(function function0 (reference1 reference2 ... referenceN) expression0)
```
Makes a function to be invoked with exactly `N` arguments. When the function is invoked, `expression0` is evaluated in an environment where `function0` is a reference to the function itself and `reference1`, `reference2`, up to `referenceN` are references to the results of evaluating the corresponding arguments in the invoke expression invoking this function. Once the evaluation is complete, control flow returns to the invoke expression and the invoke expression's result is the result of evaluating `expression0`. The result of this function expression is a reference to the function.

This expression is implemented by first emitting an instruction to `mov` the address `function0` (a label to be emitted later) into the memory location determined by the surrounding expression. Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this function. Then the label named `function0` is emitted. Then instructios to `push` each callee-saved register onto the stack are emitted. Then an instruction to push the frame-pointer onto the stack is emitted. Then an instruction to move the value of the stack-pointer into the frame-pointer is emitted. Then an instruction to `sub` from the stack-pointer the amount of words reserved on this function's stack-frame is emitted. After this the instructions for `expression0` are emitted with the location of the result fixed to a word within the stack-pointer's drop. After this an instruction is emitted to `mov` the word from this location into the register `eax`. And finally, instructions are emitted to `leave` the current function's stack-frame, `pop` the callee-save registers, and `ret` to the address of the caller.

Say the expression `[& x]` evaluates to the value at the reference `x`. Then `[putchar [(function my- (a b) [- [& b] [& a]]) (b 00000000000000000000000000000001) (b 00000000000000000000000001100011)]]` prints the text "b" to standard output.

### Invoke
```scheme
(invoke function0 expression1 expression2 ... expressionN)
[function0 expression1 expression2 ... expressionN]
```
Both the above expressions are equivalent. Evaluates `function0`, `expression1`, `expression2`, up to `expressionN` in an unspecified order and then invokes `function0`, a reference to a function, providing it with the results of evaluating `expression1` up to `expressionN`, in order. The result of this expression is determined by the function being invoked.

`N+1` words must be reserved in the current function's stack-frame plan. The expression is implemented by emitting the instructions for any of the subexpressions with the location of the result fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. Then an instruction to `push` the last reserved word onto the stack is emitted, followed by the second last, and so on, ending with an instruction to `push` the first reserved word onto the stack. A `call` instruction with the zeroth reserved word as the operand is then emitted. An `add` instruction that pops N words off the stack is then emitted. Then an instruction is emitted to `mov` the register `eax` into a memory location determined by the surrounding expression.

Say a function with the reference `-` is defined to return the result of subtracting its second parameter from its first. Then `(invoke putchar (invoke - (b 00000000000000000000000001100011) (b 00000000000000000000000000000001)))` prints the text "b" to standard output.

### With-Continuation
```scheme
(with-continuation continuation0 expression0)
```
Makes a continuation to the containing expression that is to be continued to with exactly one argument. Then `expression0` is evaluated in an environment where `continuation0` is a reference to the aforementioned continuation. The result of this expression is unspecified if the evaluation of `expression0` completes. If the continuation `continuation0` is continued to, then this `with-continuation` expression evaluates to the result of evaluating the single argument within the continue expression.

An implementation-defined number of words (5 on the i386) must be reserved in the current function's stack-frame plan. Right after these words is reserved another word in which to receive the continuation argument. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting instructions to store the program's state at `continuation0`, that is, instructions are emitted to `mov` the callee-save registers, the frame-pointer, and the address of the first instruction to be executed after a continue statement (a label to be emitted later). After this, the instructions for `expression0` are emitted. Then the label for the first instruction of the continuation is emitted. And finally, an instruction is emitted to `mov` the result of the continuation, the single word right after the reserved bock, into the memory location determined by the surrounding expression.

Note that the expression `{continuation0 expression0}` continues to the continuation reference by `continuation0` with result of evaluating `expression0` as its argument. With the note in mind, the expression `(begin [putchar (with-continuation ignore (begin {ignore (b 00000000000000000000000001001110)} [foo] [foo] [foo]))] [bar])` prints the text "nbar" to standard output.

### Make-Continuation
```scheme
(make-continuation continuation0 (reference1 reference2 ... referenceN) expression0)
```
Makes a continuation to be continued to with exactly `N` arguments. When the continuation is continued to, `expression0` is evaluated in an environment where `continuation0` is a reference to the continuation itself and `reference1`, `reference2`, up to `referenceN` are references to the results of evaluating the corresponding arguments in the continue expression continuing to this function. Undefined behavior results if the evaluation of `expression0` completes - i.e. programmer must direct the control flow out of `continuation0` somewhere within `expression0`. The result of this make-continuation expression is a reference to the continuation.

An implementation-defined number of words (5 on the i386) must be reserved in the current function's stack-frame plan. Right after these words is reserved another N words in which to receive the continuation arguments. Call the reference to the first word of the reservation `continuation0`. This expression is implemented by first emitting an instruction to `mov` the reference `continuation0` into the memory location determined by the surrounding expression. Instructions are then emitted to store the program's state at `continuation0`, that is, instructions are emitted to `mov` the callee-save registers, the frame-pointer, and the address of the first instruction to be executed after a continue statement (a label to be emitted later). Then an instruction is emitted to `jmp` to the end of all the instructions that are emitted for this make-continuation expression. Then the label for the first instruction of the continuation is emitted. After this the instructions for `expression0` are emitted.

The expression `{(make-continuation forever (a b) (begin [putchar [& a]] [putchar [& b]] {forever [- [& a] (b 00000000000000000000000000000001)] [- [& b] (b 00000000000000000000000000000001)]})) (b 00000000000000000000000001011010) (b 00000000000000000000000001111010)}` prints the text "ZzYyXxWw"... to standard output.

### Continue
```scheme
(continue continuation0 expression1 expression2 ... expressionN)
{continuation0 expression1 expression2 ... expressionN}
```
Both the above expressions are equivalent. Evaluates `continuation0`, `expression1`, `expression2`, up to `expressionN` in an unspecified order and then continues to `continuation0`, a reference to a continuation, providing it with a local copies of `expression1` up to `expressionN` in order. The result of this expression is unspecified.

`N+1` words must be reserved in the current function's stack-frame plan. The expression is implemented by emitting the instructions for any of the subexpressions with the location of the result fixed to the corresponding reserved word. The same is done with the remaining expressions repeatedly until the instructions for all the subexpressions have been emitted. Then an instruction to `mov` the first reserved word to an implementation-defined offset from the beginning of the continuation is emitted, followed by an instruction to `mov` the second reserved word to an address immediately after that, and so on, ending with an instruction to `mov` the last reserved word into the last memory address of that area. The program's state, that is, the callee-saved registers, the frame-pointer, and the address of the instruction that would have been executed next (a total of 5 words on the i386) is what is stored at the beginning of a continuation. Instructions to `mov` these values from the buffer into the appropiate registers and then set the program counter appropiately are, at last, emitted.

The expression `(begin (with-continuation cutter (continue (make-continuation cuttee () (begin [bar] [bar] (continue cutter (b 00000000000000000000000000000000)) [bar] [bar] [bar])))) [foo])` prints the text "barbarfoo" to standard output.
