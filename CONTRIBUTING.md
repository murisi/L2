# Contributing
Thank you for your interest in the L2 project. I apologize for taking so long to write this document - I did not really know what to do with the programming language I had designed. You can contact me by email on murisit@gmail.com.

## What Now?
I think a good goal for now for this project is to determine whether the language is usable, rather than merely just being a curiosity. In that direction, I think we should try to bootstrap an L2 compiler. I reckon this should be as easy as:
 * [Exposing more of the GNU assembler interface](#7) that the current L2 compiler uses as its backend so that the L2 source code being compiled can be linked to various shared libraries.
 * [Getting the current C compiler to emit debugging information](#9) so that debugging the L2 compiler we will be making in L2 is not impossible.
 * [Accumulating test code into a test folder](#10) so that we immediately know when the compiler is broken. When this folder is in place, please do not submit code that does not pass these tests.
 * [Converting the L2 compiler written in C into one written in L2](#11). I reckon this should not be too difficult as L2 is only superficially different from C.

## Measuring Success and Failure
It is very possible that the above effort could fail. Here is how I'm measuring success and failure:
 * Failure: We spend an inordinate amount of time tracking down bugs that would have been prevented by a type system like C's.
 * Success: Bugs caused by L2 being untyped and unsafe are not subtle, and occur very close to the erronous lines of L2 code.
 * Success: Experience of running GNU Debugger on L2 code to be as enlightening/informative as running Lisp code in an REPL.
 * Failure: It is more tedious to read or write L2 code than C code.
 * Success: Amount of code in L2 compiler written in L2 (excluding macros to implement C's builtins) is less than or equal to that of compiler written in C.
