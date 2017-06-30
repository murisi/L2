# Contributing
Thank you for your interest in the L2 project. I apologize for taking so long to write this document - I did not really know what to do with the programming language I had designed. You can contact me by email on murisit@gmail.com.

## What Now?
I think a good goal for now for this project is to determine whether the language is usable, rather than merely just being a curiosity. In that direction, I think we should try to bootstrap an L2 compiler. I reckon this should be as easy as:
 * [Exposing more of the GNU assembler interface](#7) that the current L2 compiler uses as its backend so that the L2 source code being compiled can be linked to various shared libraries.
 * [Getting the current C compiler to emit debugging information](#9) so that debugging the L2 compiler we will be making in L2 is not impossible.
 * [Accumulating test code into a test folder](#10) so that we immediately know when the compiler is broken. When this folder is in place, please do not submit code that does not pass these tests.
 * [Converting the L2 compiler written in C into one written in L2](#11). I reckon this should not be too difficult as L2 is only superficially different from C.

## Possible Failures
It is very possible that the above effort could fail. I reckon we could fail to meet the above goals because:
 * We end up spending an inordinate amount of time tracking down bugs that would have been prevented by a type system like C's. I'm hoping that the bugs caused by L2 being untyped and unsafe are not subtle, and that they occur very close to the erronous lines of L2 code. I want the experience of running GNU Debugger on L2 code to be as enlightening/informative as running Lisp code in an REPL.
 * The language is too tedious to read or write. If this is so, hopefully we can figure out why this is the case and do something to mitigate. However, I hope that this is not the case and that the total lines of L2 code that we write (excluding macros to implement iteration and selection statements) will be less than or equal to the total lines of C code.
