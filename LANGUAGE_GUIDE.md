# PyPy Language Guide

PyPy is line based. Every command starts at the beginning of a line. Blocks are closed with `end`.

## Execution Modes

Script mode:

```powershell
.\pypy_language.exe examples\showcase.pypy
```

Terminal mode:

```powershell
.\pypy_language.exe --terminal
```

Terminal mode stores code in a buffer. Nothing runs when you type `say`, `let`, `if`, or any other language line. The whole program runs only when you type `execute`.

Terminal commands are `execute`, `show`, `clear`, `save path`, `load path`, `help`, and `exit`.

## Core Commands

```pypy
let x = 10
x = x + 5
say "x = " + x
ask name "Name: "
let city = input("City: ")
```

`ask` stores text directly in a variable. `input()` returns text inside an expression, so it can be converted or combined:

```pypy
let age = number(input("Age: "))
```

## Blocks

```pypy
if x > 10
    say "large"
else
    say "small"
end

while x > 0
    let x = x - 1
end

repeat 3
    say "hello"
end
```

## Functions

```pypy
func power2(n)
    return n * n
end

say power2(8)
```

## Data

```pypy
array names[3]
set names[0] = "A"

matrix board[3][3]
set board[1][1] = "X"
```

## Files

```pypy
filewrite "log.txt", "start\n"
fileappend "log.txt", "done\n"
say fileread("log.txt")
```

## Built-Ins

## BODMAS And Algebra

PyPy follows BODMAS/DMAS expression order:

```pypy
say 2 + 3 * 4
say (2 + 3) * 4
say 2 + 3 * 4 ^ 2
```

Order:

- Brackets: `( ... )`
- Orders/exponents: `^`
- Division, multiplication, modulo: `/`, `*`, `%`
- Addition and subtraction: `+`, `-`
- Comparisons and logic

Algebra uses variables and explicit operators:

```pypy
let x = 4
let y = 3 * x ^ 2 + 2 * x - 5
say y
say linear_y(2, 5, 3)
say solve_linear(2, -10)
say quadratic_roots(1, -5, 6)
```

PyPy does not use implicit multiplication, so write `3 * x`, not `3x`.

Input and conversion:

```pypy
let name = input("Name: ")
say str(55)
say number("12.5")
say int("12.9")
say bool(name)
say type(name)
```

Text:

```pypy
say upper("pypy")
say lower("PyPy")
say strip("  text  ")
say substr("abcdef", 1, 3)
say contains("abcdef", "cd")
say replace("one two", "two", "three")
say repeattext("*", 5)
```

Math and random:

```pypy
say abs(-5)
say min(3, 9)
say max(3, 9)
say clamp(99, 0, 10)
say sqrt(81)
say pow(2, 8)
say seed(7)
say randint(1, 10)
```

File helpers:

```pypy
say fileexists("log.txt")
say filesize("log.txt")
say linecount("log.txt")
```

## Error Examples

```powershell
.\pypy_language.exe examples\error_divide_by_zero.pypy
.\pypy_language.exe examples\error_bounds.pypy
```

These examples intentionally fail and print the line number plus the bad source line.
