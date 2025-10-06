# Arithmetic Expansion in Nutshell (nsh)

## Overview

Nutshell provides powerful expansion capabilities that allow you to dynamically generate and manipulate command arguments, perform calculations, and execute subshell commands.

---

## Arithmetic Expansion

Arithmetic expansion allows you to perform mathematical calculations directly in your commands. Nutshell supports a wide range of mathematical operations and follows standard operator precedence.

### Basic Syntax

```nsh
$ echo $((2 + 3))
5

$ echo $((10 * (3 + 2)))
50
```

---

## Supported Operators

### Arithmetic Operators
| Operator | Description |
|-----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo (integer remainder) |
| `**` | Exponentiation |

### Bitwise Operators
| Operator | Description |
|-----------|-------------|
| `&` | Bitwise AND |
| `|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT (unary) |
| `<<` | Left shift |
| `>>` | Right shift |

### Comparison Operators
| Operator | Description |
|-----------|-------------|
| `==` | Equal to |
| `!=` | Not equal to |
| `<` | Less than |
| `<=` | Less than or equal to |
| `>` | Greater than |
| `>=` | Greater than or equal to |

### Logical Operators
| Operator | Description |
|-----------|-------------|
| `&&` | Logical AND |
| `||` | Logical OR |
| `!` | Logical NOT (unary) |

### Unary Operators
| Operator | Description |
|-----------|-------------|
| `+` | Positive (unary plus) |
| `-` | Negative (unary minus) |

---

## Number Formats

### Decimal Numbers
```nsh
$ echo $((10 + 5.5))
15.5

$ echo $((3.14 * 2))
6.28
```

### Hexadecimal Numbers
```nsh
$ echo $((0xff + 0x10))
271

$ echo $((0x1F * 2))
62
```

---

## Variable Support
```nsh
$ NUM=10
$ echo $((NUM * 2))
20

$ echo $((NUM + $RANDOM))
# Random calculation
```

---

## Advanced Features

### Floating Point Arithmetic
```nsh
$ echo $((3.14 * 2.5))
7.85

$ echo $((10 / 3))
3.3333333333
```

### Complex Expressions
```nsh
$ echo $((2 ** 3 + (10 - 5) * 2))
18

$ echo $((3.14 * (5 ** 2)))
78.5
```

### Boolean Expressions
```nsh
$ echo $((5 > 3))
1

$ echo $((10 == 5 * 2))
1

$ echo $((!(5 < 3)))
1
```

---

## Puzzle Solving

Nutshell can solve algebraic puzzles using the `?` variable:

```nsh
$ echo $((? + 5 = 10))
5

$ echo $((2 * ? + 3 = 11))
4

$ echo $(((? + 1) * 2 = 12))
5
```

---

## Error Handling
```nsh
# Division by zero
$ echo $((10 / 0))
nsh: arithmetic error: 10 / 0: Division by zero
0

# Invalid expression
$ echo $((5 + * 3))
nsh: arithmetic error: 5 + * 3: Invalid expression
0
```

---

## Operator Precedence

From highest to lowest precedence:

1. `**` — Exponentiation  
2. `u+`, `u-`, `!`, `~` — Unary operators  
3. `*`, `/`, `%` — Multiplicative  
4. `+`, `-` — Additive  
5. `<<`, `>>` — Shift  
6. `<`, `<=`, `>`, `>=` — Relational  
7. `==`, `!=` — Equality  
8. `&` — Bitwise AND  
9. `^` — Bitwise XOR  
10. `|` — Bitwise OR  
11. `&&` — Logical AND  
12. `||` — Logical OR  

---

## Notes

- Arithmetic expansion uses **floating-point arithmetic** for precise calculations.  
- Parentheses can be used to **override operator precedence**.  
- Variables are automatically expanded within arithmetic expressions.  
- Error handling follows **nsh conventions** (returns 0 on error).  
- Puzzle solving supports **linear equations** with one unknown variable.  

---

Arithmetic expansion in Nutshell provides a powerful way to perform calculations directly in your shell commands, making it easier to write dynamic and flexible scripts.
