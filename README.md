# L2
## Introduction
L2 is an attempt to find the smallest most distilled programming language equivalent to C. The goal is to turn as much of C's preprocessor directives, control structures, statements, literals, and functions requiring compiler assistance (setjmp, longjmp, alloca, ...) into things definable inside L2. The language does not surject to all of C, its most glaring omission being that of a type-system. However, I reckon the result is still pretty interesting.

The approach taken to achieve this has been to make C's features more composable, more multipurpose, and, at least on one occasion, add a new feature so that a whole group of distinct features could be dropped. In particular, the most striking changes are that C's:
1. irregular syntax is replaced by S-expressions; because simple syntax composes well with a non-trivial preprocessor (and no, I have not merely transplanted Common Lisp's macros into C)
2. loop constructs is replaced with what I could only describe as a more structured variant of setjmp and longjmp without stack destruction (and no, there is no performance overhead associated with this)

The entirity of the language can be communicated in less than 3 pages. There are 10 language primitives and for each one of them I describe their syntax, what exactly they do in English, the i386 assembly they translate into, and an example usage of them. Following this comes a brief description of L2's internal representation and the 5 functions (loosely speaking) that manipulate it. Following this comes a sort of "glossary" that shows not only how C's constructs, but more exotic stuff like coroutines, Python's generators, and Scheme's lambdas can be defined in terms of L2.
