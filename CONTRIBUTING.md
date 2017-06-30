# Contributing
Thank you for your interest in the L2 project. I apologize for taking so long to write this document - I did not really know what to do with the programming language I had designed until now.

 * [What Now?](#what-now)
 * [Measuring Usability](#measuring-usability)
 * [Measuring Advantageousness](#measuring-advantageousness)
 * [Project Style](#project-style)

## What Now?
I think a good goal for now for this project is to determine whether the language is usable, rather than merely just being a curiosity. In that direction, I think we should try to bootstrap an L2 compiler. I reckon this should be as easy as:
 * [Exposing more of the GNU assembler interface](#7) that the current L2 compiler uses as its backend so that the L2 source code being compiled can be linked to various shared libraries.
 * [Getting the current C compiler to emit debugging information](#9) so that debugging the L2 compiler we will be making in L2 is not impossible.
 * [Accumulating test code into a test folder](#10) so that we immediately know when the compiler is broken. When this folder is in place, please do not submit code that does not pass these tests.
 * [Converting the L2 compiler written in C into one written in L2](#11). I reckon this should not be too difficult as L2 is only superficially different from C.

## Measuring Usability
The L2 project is an experiment and it is very possible that the above effort could fail. Here is how I'm measuring usability:
 * Failure: We spend an inordinate amount of time tracking down bugs that would have been prevented by a type system like C's.
 * Success: Bugs caused by L2 being untyped and unsafe are not subtle, and occur very close to the erronous lines of L2 code.
 * Success: Experience of running GNU Debugger on L2 code to be as enlightening/informative as running Lisp code in an REPL.
 * Failure: It is more tedious to read or write L2 code than C code.
 * Success: Amount of code in L2 compiler written in L2 (excluding macros to implement C's builtins) is less than or equal to that of compiler written in C.

## Measuring Advantageousness
If the L2 programming language is usable, then the natural question is: Is using L2 ever advantageous? Some thoughts:
 * Pthreads maybe easier to use in L2 than in C because
   * L2 functions are valid anywhere an expression is
   * L2 supports static non-global variables
 * Threads maybe easier to do in L2 than in C because
   * Threads could be made out of L2 continuations instead of functions
   * This way, thread access to local function variables does not have to be done through void pointer
 * Higher order functions may be easier to do in L2 than in C because
   * Functions like `map`, `exists`, `fold_left` can be parameterized using L2 continuations instead of functions
   * This way, the continuations can access local variables of the function calling the higher order function
   * Unlike C functions, the continuations are valid anywhere an expression is
 * Exception handling may be easier to do correctly than in L2 than in C, C++, and Java
   * Functions that could fail would be written as if they do not fail
     * That is, they return exactly the information needed in the non-exceptional case
       * This way, the return values of functions do not need to be checked for errors clouding normal control flow
   * The difference is that they would take an error continuation as an argument
     * If there is an exceptional circumstance, this function would continue to the error continuation
     * This way, there cannot be such a thing as an uncaught exception - the handler is an argument to the function
     * This way, functions can pass on the error handlers they received as arguments onto their child calls
     * Unlike if the error handler had been an L2 function, L2 continuations can access local variables of the caller in order to
       * Deallocate memory allocated locally that is no longer needed
       * In turn follow the error continuation of the caller - thus achieving a parallel control flow for errors
 * Libraries written in L2 may be less awkward than those written in C because
   * Compiled libraries can add macros to the programming language
   * Libraries that detect platform can be written (in L2, C, or any other language), and then be called at compile-time by an L2 macro
   * L2's control flow is more general than C's allowing the programmer to jump up and down between live functions on the stack
 * Perhaps L2 has less undefined/unexpected behaviors than C?
   * For one, L2's untypedness means that it does not have any potentially unexpected conversion rules
   * For another, L2's untypedness eliminates the undefined behaviors stemming from conversions between differently sized types
 * Perhaps L2 is more scriptable than C because of the context-freeness of its primitive expressions?
 * Perhaps L2 is easier to teach/learn because its specification is small?

## Project Style
 * The things that need/you want to be implemented should be put in the issue tracker
 * Disagreement with what/how things should be implemented go to the issue tracker
 * Out-of-the-box thinking is encouraged - I honestly do not know where this project is going to go
 * If you would like to solve an issue, comment as such on the issue tracker
 * Try to do small pull requests as you do your hacking so that I can detect errors early
 * If the above does not answer all questions, you can contact me at murisit@gmail.com
