# PyPy C++ Developer Function Documentation

This document explains the source code function by function so another developer can understand how the PyPy interpreter works, where each feature is implemented, and how the files connect together.

The project has two main C++ files:

- `pypy_language.cpp`: the interpreter core. It contains values, tokens, variables, expression parsing, statement execution, built-in functions, and user-defined function calls.
- `pypy_terminal.cpp`: the executable wrapper. It includes `pypy_language.cpp`, provides `main()`, handles command-line arguments, script mode, terminal mode, save/load, and error printing.

## High-Level Execution Flow

1. `main()` in `pypy_terminal.cpp` receives command-line arguments.
2. If the user passes a `.pypy` file, `runScript()` creates an `Interpreter` record, initializes it, loads the file, and runs it.
3. If the user passes `--terminal`, `runTerminal()` starts a buffered terminal.
4. The `Interpreter` record stores loaded lines inside `Program`.
5. Before running code, `interpreterPreprocessFunctions()` scans all `func ... end` blocks and stores their locations.
6. `interpreterExecuteBlock()` walks line by line and handles statements such as `let`, `say`, `if`, `while`, `repeat`, `array`, `matrix`, `set`, `filewrite`, `fileappend`, and `return`.
7. Expressions are evaluated by `interpreterEvalExpression()`, which creates an `ExpressionParser` record.
8. `parserTokenize()` tokenizes the expression and parser precedence functions such as `parserParseOr()`, `parserParseAnd()`, `parserParseTerm()`, and `parserParseFactor()` parse it.
9. Built-in functions are handled by `parserCallBuiltIn()`.
10. User-defined functions are handled by `interpreterCallFunction()`.
11. Errors are recorded with `fail()` and printed by `printPyPyError()`.

## Fixed Limits

These constants keep the project simple and presentation-friendly. They also prevent many runaway cases.

| Constant | Meaning |
|---|---|
| `MAX_LINES` | Maximum source lines loaded into a program. |
| `MAX_VARS` | Maximum variables in one environment or scope. |
| `MAX_CELLS` | Maximum cells allocated for one array or matrix variable. |
| `MAX_FUNCTIONS` | Maximum user-defined functions. |
| `MAX_PARAMS` | Maximum parameters in one function definition. |
| `MAX_ARGS` | Maximum arguments in one function call. |
| `MAX_TOKENS` | Maximum tokens in one expression. |
| `MAX_CALL_DEPTH` | Maximum nested user-defined function calls. |
| `MAX_LOOP_STEPS` | Maximum loop iterations inside one executed block before stopping. |

## Data Types And Structures

### `enum ValueType`

Defines the runtime type of a PyPy value.

- `VAL_NULL`: represents no value.
- `VAL_NUMBER`: represents a numeric value stored as `double`.
- `VAL_TEXT`: represents a string value stored as `string`.

The language uses numbers for booleans: `1` means true and `0` means false.

### `enum VarKind`

Defines how a variable is stored.

- `VAR_SCALAR`: a normal single value.
- `VAR_ARRAY`: a one-dimensional indexed collection.
- `VAR_MATRIX`: a two-dimensional indexed collection.

### `enum TokenType`

Defines token categories used by the expression parser.

- `TOK_END`: sentinel token marking the end of an expression.
- `TOK_NUMBER`: numeric literal.
- `TOK_TEXT`: string literal.
- `TOK_IDENT`: identifier, keyword, variable name, or function name.
- `TOK_OP`: operator such as `+`, `-`, `*`, `/`, `==`, or `>=`.
- `TOK_LPAREN` and `TOK_RPAREN`: `(` and `)`.
- `TOK_COMMA`: comma between function arguments.
- `TOK_LBRACKET` and `TOK_RBRACKET`: `[` and `]`.

### `struct Value`

Stores one runtime value.

- `type`: one of the `ValueType` values.
- `number`: numeric storage used when `type == VAL_NUMBER`.
- `text`: string storage used when `type == VAL_TEXT`.

### `struct Token`

Stores one expression token.

- `type`: one of the `TokenType` values.
- `text`: original or processed token text.
- `number`: parsed numeric value for number tokens.

### `struct ErrorState`

Stores the first interpreter error.

- `failed`: true after an error happens.
- `line`: source line number where the error happened.
- `message`: human-readable error message.

The helper `fail()` only records the first error so later code does not overwrite the original problem.

### `struct Variable`

Stores a PyPy variable.

- `used`: whether the slot is active.
- `name`: variable name.
- `kind`: scalar, array, or matrix.
- `rows`: array size, or matrix row count.
- `cols`: matrix column count, or `1` for arrays.
- `scalar`: value used by scalar variables.
- `cells`: fixed-size `Value` array used by arrays and matrices.

For a matrix, cell `(row, col)` is stored at `row * cols + col`.

### `struct Program`

Stores loaded source lines.

- `lines`: fixed-size array of source lines.
- `count`: number of loaded lines.

### `struct FunctionDef`

Stores one user-defined function.

- `used`: whether the function slot is active.
- `name`: function name.
- `params`: fixed-size parameter name list.
- `paramCount`: number of parameters.
- `startLine`: line index where `func` appears.
- `endLine`: matching `end` line index.

Function bodies are not copied. The interpreter stores their start and end positions in the original program.

### `struct ExecResult`

Stores the result of executing a block.

- `hasReturn`: true if a `return` statement was hit.
- `value`: returned value.

This lets nested blocks stop immediately when a function returns.

## Global Helper Functions In `pypy_language.cpp`

### `Value makeNull()`

Creates a `Value` with type `VAL_NULL`.

How it works:

- Sets `type` to `VAL_NULL`.
- Sets `number` to `0`.
- Sets `text` to an empty string.
- Returns the initialized value.

Used when a statement or function has no meaningful value.

### `Value makeNumber(double number)`

Creates a numeric PyPy value.

How it works:

- Sets `type` to `VAL_NUMBER`.
- Stores the argument in `number`.
- Clears `text`.
- Returns the value.

Used for arithmetic results, comparison results, boolean results, random numbers, and numeric built-ins.

### `Value makeText(string text)`

Creates a text PyPy value.

How it works:

- Sets `type` to `VAL_TEXT`.
- Sets `number` to `0`.
- Stores the argument in `text`.
- Returns the value.

Used for string literals, input, file reads, and text built-ins.

### `bool charIsSpace(char ch)`

Checks whether a character is whitespace.

Recognized whitespace:

- Space
- Tab
- Newline
- Carriage return
- Vertical tab
- Form feed

Used by trimming, tokenizing, word matching, and input parsing.

### `bool charIsDigit(char ch)`

Returns true if `ch` is between `'0'` and `'9'`.

Used by number parsing and expression tokenization.

### `bool charIsAlpha(char ch)`

Returns true for ASCII letters `A-Z` or `a-z`.

Used by identifier validation and tokenization.

### `bool charIsAlphaNumeric(char ch)`

Returns true for letters or digits.

How it works:

- Calls `charIsAlpha(ch)`.
- Calls `charIsDigit(ch)`.
- Returns true if either one is true.

Used for identifier body characters.

### `char charToUpper(char ch)`

Converts a lowercase ASCII letter to uppercase.

How it works:

- If `ch` is between `'a'` and `'z'`, subtracts the lowercase offset and returns the uppercase character.
- Otherwise returns the original character.

Used by the `upper()` built-in.

### `char charToLower(char ch)`

Converts an uppercase ASCII letter to lowercase.

How it works:

- If `ch` is between `'A'` and `'Z'`, converts it to the matching lowercase character.
- Otherwise returns the original character.

Used by the `lower()` built-in.

### `string trim(string text)`

Removes whitespace from the beginning and end of a string.

How it works:

- Moves `start` forward while leading characters are whitespace.
- Moves `end` backward while trailing characters are whitespace.
- If the whole string is whitespace, returns an empty string.
- Otherwise returns the substring between `start` and `end`.

Used heavily for cleaning source lines, parsing commands, parsing function parameters, and parsing file command arguments.

### `bool parseNumberText(string text, double *out)`

Converts plain numeric text into a `double`.

How it works:

- Trims the input.
- Accepts optional `+` or `-`.
- Reads whole-number digits.
- Optionally reads one decimal point and decimal digits.
- Rejects empty values, invalid characters, and malformed numbers.
- Writes the parsed result into `*out`.
- Returns true on success and false on failure.

This is a manual parser, which keeps the implementation simple and avoids depending on advanced conversion functions.

### `string numberToString(double number)`

Converts a number to display text.

How it works:

- Uses `to_string(number)`.
- Removes trailing zeroes.
- Removes a trailing decimal point if needed.
- Converts `-0` to `0`.

Used by `valueToString()`, error messages, and algebra helpers.

### `string valueToString(Value value)`

Converts any PyPy value into text.

How it works:

- Text values return their stored text.
- Number values call `numberToString()`.
- Null values return `"null"`.

Used by output, string concatenation, file writing, and many built-ins.

### `bool isTruthy(Value value)`

Implements PyPy truthiness.

Rules:

- A number is true if it is not `0`.
- Text is true if it is not empty.
- Null is false.

Used by `if`, `while`, logical `and`, logical `or`, and `not`.

### `bool isIdentifierStart(char ch)`

Checks if a character can start an identifier.

Allowed:

- ASCII letter
- Underscore

### `bool isIdentifierPart(char ch)`

Checks if a character can appear after the first character of an identifier.

Allowed:

- ASCII letter
- Digit
- Underscore

### `bool isValidIdentifier(string name)`

Validates variable names, function names, and parameter names.

How it works:

- Rejects empty names.
- Requires the first character to pass `isIdentifierStart()`.
- Requires every remaining character to pass `isIdentifierPart()`.
- Rejects reserved words such as `let`, `if`, `else`, `end`, `while`, `repeat`, `func`, `return`, `and`, `or`, `not`, `array`, `matrix`, `set`, `say`, and `ask`.

This protects the interpreter from confusing keywords with variable names.

### `bool startsWithWord(string line, string word)`

Checks whether a line starts with a command word.

How it works:

- First checks whether `line` starts with the exact `word`.
- If the line length equals the word length, returns true.
- Otherwise requires the next character to be whitespace.

This prevents `ifx` from being treated as `if`.

### `string removeComment(string line)`

Removes comments from a source line while respecting strings.

How it works:

- Walks through the line character by character.
- Tracks whether the current position is inside a string literal.
- Tracks escape sequences inside strings.
- Outside strings, removes text starting from `#`.
- Outside strings, also removes text starting from `//`.
- Leaves comment-like text inside strings untouched.

Used by `interpreterCleanLine()`.

### `void fail(ErrorState *error, int line, string message)`

Records an interpreter error.

How it works:

- If `error->failed` is already true, it does nothing.
- Otherwise sets `failed`, `line`, and `message`.

This keeps the first error as the most useful error to show the user.

### `bool isIntegerValue(double value)`

Checks if a `double` represents a whole number.

How it works:

- Casts the value to `int`.
- Compares the original value with the casted value.

Used for array indexes, matrix indexes, repeat counts, modulo, exponents, and functions such as `int()`, `charat()`, and `randint()`.

### `double manualPower(double base, int exponent, bool *ok)`

Calculates integer exponent power manually.

How it works:

- Multiplies `base` by itself `abs(exponent)` times.
- If the exponent is negative, returns `1 / result`.
- If a negative exponent would divide by zero, sets `*ok` to false.
- Otherwise sets `*ok` to true.

Used by the `^` operator and `pow()` built-in.

### `double manualSqrt(double value, bool *ok)`

Calculates square root manually using Newton's method.

How it works:

- Rejects negative values by setting `*ok` to false.
- Returns `0` for input `0`.
- Starts with `guess = value`.
- Improves the guess 30 times using `(guess + value / guess) / 2`.
- Sets `*ok` to true and returns the approximation.

Used by `sqrt()` and `quadratic_roots()`.

## `Environment` Data And Functions

`Environment` stores variables for one scope. The global interpreter has one global environment. Each user-defined function call creates a local environment with the global environment as its parent.

### `void initEnvironment(Environment *env, Environment *parentEnv)`

Initializes a variable scope.

How it works:

- Stores the parent environment pointer.
- Allocates `MAX_VARS` variable slots.
- Initializes each slot as unused.
- Allocates `MAX_CELLS` cells for each variable slot.
- Initializes all scalar values and cells to null.

Why it matters:

- Arrays and matrices use the same `Variable` structure as scalars.
- Every variable slot has cell storage ready, keeping the code simple.

### `void freeEnvironment(Environment *env)`

Frees memory owned by a variable scope.

How it works:

- Deletes the `cells` array for every variable slot.
- Deletes the `vars` array.

This prevents memory leaks when local function environments are destroyed.

### `Variable *envFindLocal(Environment *env, string name)`

Searches only the current environment for a variable.

How it works:

- Loops through `vars`.
- Returns the matching used variable slot.
- Returns `NULL` if no local variable matches.

Used when declaring variables and avoiding duplicate local allocation.

### `Variable *envFindAny(Environment *env, string name)`

Searches the current environment and then parent scopes.

How it works:

- Calls `findLocal(name)`.
- If found, returns that variable.
- If not found and `parent` exists, recursively searches the parent.
- Returns `NULL` if the variable does not exist anywhere.

Used when reading variables, assigning variables, and resolving variables inside functions.

### `Variable *envAllocate(Environment *env, string name, ErrorState *error, int line)`

Creates or returns a variable slot in the current environment.

How it works:

- Validates the identifier.
- If a local variable with the same name already exists, returns it.
- Otherwise finds the first unused variable slot.
- Marks it used and resets its scalar and cells.
- If no slot is free, records an error.

Important behavior:

- Allocation is local only.
- If the same local name already exists, it reuses the slot.

### `void envDeclareScalar(Environment *env, string name, Value value, ErrorState *error, int line)`

Declares or replaces a scalar variable in the current environment.

How it works:

- Calls `allocate()`.
- Sets `kind` to `VAR_SCALAR`.
- Clears row and column counts.
- Stores the scalar value.

Used by `let` statements and function parameters.

### `void envAssignScalar(Environment *env, string name, Value value, ErrorState *error, int line)`

Assigns a scalar variable.

How it works:

- Validates the identifier.
- Searches current and parent environments with `envFindAny()`.
- If not found, allocates a new local variable.
- Rejects assignment if the variable is an array or matrix.
- Stores the new scalar value.

Used by normal assignment, `ask`, and `input`-style logic.

### `void envDeclareArray(Environment *env, string name, int size, ErrorState *error, int line)`

Declares a one-dimensional array.

How it works:

- Checks that `size` is between `1` and `MAX_CELLS`.
- Allocates the variable slot.
- Sets `kind` to `VAR_ARRAY`.
- Stores `rows = size` and `cols = 1`.
- Clears array cells to null.

Used by `array name[size]`.

### `void envDeclareMatrix(Environment *env, string name, int rows, int cols, ErrorState *error, int line)`

Declares a two-dimensional matrix.

How it works:

- Requires positive row and column counts.
- Requires `rows * cols <= MAX_CELLS`.
- Allocates the variable slot.
- Sets `kind` to `VAR_MATRIX`.
- Stores row and column counts.
- Clears all used matrix cells to null.

Used by `matrix name[rows][cols]`.

## `ExpressionParser` Data And Functions

`ExpressionParser` converts a single expression string into tokens, then parses those tokens into a `Value`.

It is a recursive descent parser. Each parse function represents one precedence level.

Precedence from lowest to highest:

1. `or`
2. `and`
3. `==`, `!=`
4. `<`, `<=`, `>`, `>=`
5. `+`, `-`
6. `*`, `/`, `%`
7. `^`
8. unary `-`, `not`
9. literals, variables, calls, parentheses, array/matrix access

### `void initExpressionParser(ExpressionParser *parser, ErrorState *err, Environment *environment, Interpreter *owner, int lineNumber)`

Initializes parsing state for one expression.

How it works:

- Resets token count and parser position.
- Stores the source line number.
- Stores pointers to the shared error state, current environment, and owning interpreter.

The interpreter pointer is required so built-ins can call user-defined functions or update interpreter state such as `randomSeed`.

### `bool parserTokenize(ExpressionParser *parser, string expression)`

Splits expression text into tokens.

How it works:

- Skips whitespace.
- Reads number literals, including decimals.
- Reads identifiers.
- Reads string literals and supports escapes: `\n`, `\t`, `\"`, and `\\`.
- Reads two-character operators: `==`, `!=`, `<=`, `>=`.
- Reads one-character operators: `+`, `-`, `*`, `/`, `%`, `^`, `<`, `>`, `=`.
- Reads parentheses, brackets, and commas.
- Records an error for invalid characters, invalid numbers, unclosed strings, bad escapes, or too many tokens.
- Adds a final `TOK_END` token.

### `Token parserPeek(ExpressionParser *parser)`

Returns the current token without consuming it.

Used by parse functions to decide what to do next.

### `bool parserMatchType(ExpressionParser *parser, int type)`

Consumes the current token if its type matches.

How it works:

- Compares `tokens[pos].type` with `type`.
- If equal, increments `pos` and returns true.
- Otherwise returns false.

### `bool parserMatchOp(ExpressionParser *parser, string op)`

Consumes the current token if it is a matching operator.

Used for operators like unary `-` and power `^`.

### `bool parserMatchIdent(ExpressionParser *parser, string ident)`

Consumes the current token if it is a matching identifier.

Used for keyword-like expression operators such as `and`, `or`, and `not`.

### `bool parserNeedNumber(ExpressionParser *parser, Value value, double *out, string operation)`

Validates that a value is numeric.

How it works:

- If the value is not `VAL_NUMBER`, records an error such as `"Arithmetic requires a number."`.
- Otherwise writes the number into `*out`.

Used by arithmetic parse functions.

### `Value parserParseExpression(ExpressionParser *parser)`

Entry point for parsing an expression.

How it works:

- Calls `parserParseOr()`.

All expression parsing starts here.

### `Value parserParseOr(ExpressionParser *parser)`

Parses logical `or`.

How it works:

- Parses the left side with `parserParseAnd()`.
- While the next token is `or`, parses the right side.
- Converts both sides using `isTruthy()`.
- Returns `1` or `0`.

### `Value parserParseAnd(ExpressionParser *parser)`

Parses logical `and`.

How it works:

- Parses the left side with `parserParseEquality()`.
- While the next token is `and`, parses the right side.
- Returns numeric boolean `1` or `0`.

### `Value parserParseEquality(ExpressionParser *parser)`

Parses `==` and `!=`.

How it works:

- Parses each side with `parserParseComparison()`.
- If either side is text, compares both sides as strings with `valueToString()`.
- Otherwise compares numeric values.
- Returns `1` for true and `0` for false.

### `Value parserParseComparison(ExpressionParser *parser)`

Parses `<`, `<=`, `>`, and `>=`.

How it works:

- Parses each side with `parserParseTerm()`.
- If either side is text, compares string versions.
- Otherwise compares numbers.
- Returns numeric boolean `1` or `0`.

### `Value parserParseTerm(ExpressionParser *parser)`

Parses addition and subtraction.

How it works:

- Parses each side with `parserParseFactor()`.
- For `+`, if either side is text, concatenates string versions.
- For numeric `+`, adds numbers.
- For `-`, requires both sides to be numbers.

This is why `"Score: " + score` works.

### `Value parserParseFactor(ExpressionParser *parser)`

Parses multiplication, division, and modulo.

How it works:

- Parses each side with `parserParsePower()`.
- Requires both operands to be numbers.
- Rejects division or modulo by zero.
- For `%`, requires both operands to be whole numbers.

### `Value parserParsePower(ExpressionParser *parser)`

Parses exponentiation with `^`.

How it works:

- Parses the base with `parserParseUnary()`.
- If `^` appears, recursively calls `parserParsePower()` for the exponent.
- Requires both operands to be numbers.
- Requires the exponent to be a whole number.
- Calls `manualPower()`.

The recursive call makes exponentiation right-associative.

### `Value parserParseUnary(ExpressionParser *parser)`

Parses unary operators.

Supported:

- Unary minus: `-x`
- Logical not: `not x`

How it works:

- For `-`, recursively parses the next unary expression and negates the number.
- For `not`, recursively parses the next unary expression and returns `1` if false, `0` if true.
- Otherwise calls `parserParsePrimary()`.

### `Value parserParsePrimary(ExpressionParser *parser)`

Parses the smallest expression units.

Handled cases:

- Number literals
- String literals
- Parenthesized expressions
- Boolean constants: `true`, `false`
- Null constant: `null`
- Function calls
- Built-in function calls
- Variable reads
- Array reads
- Matrix reads

How function calls work:

- Reads the identifier.
- If the next token is `(`, parses comma-separated arguments.
- Enforces `MAX_ARGS`.
- Calls `parserCallBuiltIn(parser, name, args, argCount)`.
- If the name is not a built-in, `parserCallBuiltIn()` forwards to `interpreterCallFunction()`.

How array and matrix reads work:

- Finds the variable in the current environment.
- If `[` follows the name, parses an index expression.
- Arrays require one index.
- Matrices require two indexes.
- Indexes must be whole numbers and in range.

### `Value parserCallBuiltIn(ExpressionParser *parser, string name, Value args[], int argCount)`

Implements all built-in functions and forwards unknown names to user-defined functions.

This function is documented in detail later in the built-ins section.

## `Interpreter` Data And Functions

`Interpreter` owns the loaded program, global environment, function table, error state, call depth, and random seed.

### `void initInterpreter(Interpreter *interpreter)`

Initializes a complete interpreter instance.

How it works:

- Initializes the global environment with no parent.
- Resets program line count.
- Resets call depth.
- Sets default random seed to `12345`.
- Clears the error state.
- Marks all function table slots unused.

### `void freeInterpreter(Interpreter *interpreter)`

Frees memory owned by the interpreter.

How it works:

- Calls `freeEnvironment(&interpreter->globals)`.
- Releases the variable cell storage allocated for the global environment.

### `bool interpreterLoadFile(Interpreter *interpreter, string path)`

Loads a script file into the interpreter.

How it works:

- Opens the file using `ifstream`.
- If opening fails, records an error.
- Reads each line into `program.lines`.
- Enforces `MAX_LINES`.
- Updates `program.count`.

Used by script mode in `runScript()`.

### `string interpreterCleanLine(Interpreter *interpreter, int index)`

Returns a source line ready for execution.

How it works:

- Reads `program.lines[index]`.
- Calls `removeComment()`.
- Calls `trim()`.
- Returns the cleaned line.

This removes empty space and comments before statement parsing.

### `FunctionDef *interpreterFindFunction(Interpreter *interpreter, string name)`

Searches the function table.

How it works:

- Loops over `funcs`.
- Returns the matching used function.
- Returns `NULL` if not found.

Used by `interpreterParseFunctionHeader()` and `interpreterCallFunction()`.

### `int interpreterAllocateFunction(Interpreter *interpreter)`

Finds an unused function table slot.

How it works:

- Loops over `funcs`.
- Marks the first unused slot as used.
- Returns its index.
- Returns `-1` if the table is full.

Used during preprocessing.

### `bool interpreterParseFunctionHeader(Interpreter *interpreter, string line, FunctionDef *target, int lineNumber)`

Parses a function definition line.

Expected format:

```pypy
func name(a, b)
```

How it works:

- Finds the opening `(` and last closing `)`.
- Extracts and validates the function name.
- Rejects duplicate function names.
- Extracts the parameter list.
- Rejects extra text after `)`.
- Splits parameters by comma.
- Validates each parameter name.
- Rejects duplicate parameters.
- Enforces `MAX_PARAMS`.
- Stores everything in `target`.

### `bool interpreterIsBlockStart(Interpreter *interpreter, string line)`

Checks if a cleaned line starts a block.

Block starters:

- `if`
- `while`
- `repeat`
- `func`

Used by block matching functions.

### `int interpreterFindMatchingEnd(Interpreter *interpreter, int start, int stop)`

Finds the matching `end` for a block.

How it works:

- Starts scanning after `start`.
- Tracks nested block depth.
- Increases depth for nested `if`, `while`, `repeat`, or `func`.
- Decreases depth for nested `end`.
- Returns the first `end` at depth zero.
- Returns `-1` if no match exists.

Used for functions, loops, repeat blocks, and else blocks.

### `int interpreterFindElseOrEnd(Interpreter *interpreter, int start, int stop, bool *hasElse)`

Finds the top-level `else` or `end` for an `if` block.

How it works:

- Scans after the `if` line.
- Tracks nested block depth.
- If it finds `else` at depth zero, sets `*hasElse = true` and returns that line.
- If it finds `end` at depth zero, sets `*hasElse = false` and returns that line.
- Returns `-1` if neither is found.

Used by `interpreterExecuteBlock()` when running `if` statements.

### `bool interpreterPreprocessFunctions(Interpreter *interpreter)`

Scans the whole program and records user-defined functions.

How it works:

- Loops through every program line.
- When it finds `func`, allocates a function table slot.
- Parses the function header.
- Finds the matching `end`.
- Stores start and end line indexes.
- Skips to the end of the function body so nested lines are not scanned as top-level functions.

Important behavior:

- Function bodies are skipped during normal top-level execution.
- Functions can be called after preprocessing because their locations are known before execution begins.

### `bool interpreterEvalExpression(Interpreter *interpreter, string expression, Environment *env, int line, Value *out)`

Evaluates a PyPy expression.

How it works:

- Creates an `ExpressionParser`.
- Tokenizes the expression.
- Parses the expression.
- Checks that all tokens were consumed.
- Writes the result into `*out`.
- Returns false if tokenization, parsing, or trailing token checks fail.

Almost every statement that needs a value calls this function.

### `int interpreterFindTopLevelAssignment(Interpreter *interpreter, string line)`

Finds an assignment operator at top level.

How it works:

- Scans the line character by character.
- Tracks strings, escape sequences, parentheses, and brackets.
- Only accepts `=` when not inside a string, parentheses, or brackets.
- Rejects comparison operators such as `==`, `<=`, `>=`, and `!=`.
- Returns the index of the assignment `=`.
- Returns `-1` if there is no top-level assignment.

Used for `let`, `set`, and normal assignment.

### `int interpreterSplitTopLevelComma(Interpreter *interpreter, string line)`

Finds a comma that separates top-level arguments.

How it works:

- Scans the line while tracking strings, escapes, parentheses, and brackets.
- Returns the first comma not inside any nested expression.
- Returns `-1` if no top-level comma exists.

Used by `filewrite` and `fileappend`.

### `int interpreterCheckedIndex(Interpreter *interpreter, Value value, int max, int line, string subject)`

Validates an array or matrix index.

How it works:

- Requires a numeric whole-number value.
- Casts it to `int`.
- Checks that it is between `0` and `max - 1`.
- Records an error if invalid.
- Returns the valid index or `-1` on failure.

Used by `interpreterHandleSet()`.

### `bool interpreterParseNameAndBracket(Interpreter *interpreter, string text, string *name, string *first, string *second)`

Parses array or matrix syntax.

Supported patterns:

```pypy
name[index]
name[row][col]
```

How it works:

- Finds the first `[` and matching `]`.
- Extracts the name and first index expression.
- Checks if another bracket group exists.
- If yes, extracts the second index expression.
- Rejects unexpected trailing text.

Used by array declarations, matrix declarations, and `set`.

### `bool interpreterHandleArrayDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber)`

Handles `array name[size]`.

How it works:

- Removes the `array` keyword.
- Parses the name and size expression.
- Rejects matrix-style syntax.
- Evaluates the size expression.
- Requires the size to be a whole number.
- Calls `envDeclareArray()`.

### `bool interpreterHandleMatrixDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber)`

Handles `matrix name[rows][cols]`.

How it works:

- Removes the `matrix` keyword.
- Parses name, row expression, and column expression.
- Requires both dimensions to exist.
- Evaluates row and column expressions.
- Requires both dimensions to be whole numbers.
- Calls `envDeclareMatrix()`.

### `bool interpreterHandleSet(Interpreter *interpreter, string line, Environment *env, int lineNumber)`

Handles assignment into arrays and matrices.

Expected forms:

```pypy
set arr[index] = value
set mat[row][col] = value
```

How it works:

- Removes the `set` keyword.
- Finds the top-level assignment operator.
- Parses the target name and bracket expressions.
- Finds the variable in the environment.
- Evaluates the right-hand value.
- Evaluates index expressions.
- For arrays, requires one index.
- For matrices, requires two indexes.
- Uses `interpreterCheckedIndex()` for bounds checking.
- Stores the value into the correct cell.

### `bool interpreterHandleFileWrite(Interpreter *interpreter, string line, Environment *env, int lineNumber, bool append)`

Handles `filewrite` and `fileappend`.

Expected forms:

```pypy
filewrite "path.txt", "data"
fileappend "path.txt", "more data"
```

How it works:

- Removes either `filewrite` or `fileappend`.
- Splits the remaining text at a top-level comma.
- Evaluates the path expression.
- Evaluates the data expression.
- Converts both to strings.
- Opens the file normally for write mode or with `ios::app` for append mode.
- Writes the data.
- Records an error if the file cannot be opened.

### `ExecResult interpreterExecuteBlock(Interpreter *interpreter, int start, int end, Environment *env)`

Executes a range of program lines.

This is the main statement interpreter.

How it works:

- Loops from `start` to `end - 1`.
- Cleans each line with `cleanLine()`.
- Skips blank lines.
- Skips function definitions because they were already preprocessed.
- Handles statements by checking command words.
- Evaluates expressions where needed.
- Calls itself recursively for nested `if`, `while`, and `repeat` bodies.
- Returns early if a `return` statement is hit.
- Stops if the global error state fails.

Statement handling:

- `let`: declares a scalar in the current environment.
- `say` or `print`: evaluates an expression and prints it.
- `ask`: prints an optional prompt and stores user input.
- `array`: calls `interpreterHandleArrayDeclaration()`.
- `matrix`: calls `interpreterHandleMatrixDeclaration()`.
- `set`: calls `interpreterHandleSet()`.
- `filewrite`: calls `interpreterHandleFileWrite(..., false)`.
- `fileappend`: calls `interpreterHandleFileWrite(..., true)`.
- `if`: finds `else` or `end`, evaluates the condition, executes the correct branch.
- `while`: finds matching `end`, evaluates condition repeatedly, executes body.
- `repeat`: evaluates repeat count once and runs body that many times.
- `return`: packages the return value in `ExecResult`.
- bare assignment: assigns scalar variables.
- bare expression: evaluates the expression and discards the result.

Loop protection:

- Uses `loopSteps`.
- If steps pass `MAX_LOOP_STEPS`, records an infinite-loop-style error.

### `Value interpreterCallFunction(Interpreter *interpreter, string name, Value args[], int argCount, int line)`

Calls a user-defined function.

How it works:

- Finds the function by name.
- Checks argument count.
- Enforces `MAX_CALL_DEPTH`.
- Increments `callDepth`.
- Creates a local environment with the global environment as parent.
- Declares each parameter as a local scalar.
- Executes the function body with `interpreterExecuteBlock()`.
- Decrements `callDepth`.
- Returns the explicit returned value if present.
- Returns null if the function has no return.

Important behavior:

- Function local variables do not leak into globals.
- Functions can read globals through the parent environment.

### `bool interpreterRun(Interpreter *interpreter)`

Runs the loaded program.

How it works:

- Calls `interpreterPreprocessFunctions()`.
- Executes the whole program with `interpreterExecuteBlock(interpreter, 0, program.count, &globals)`.
- Returns true if no error occurred.

## Built-In Functions In `parserCallBuiltIn()`

`parserCallBuiltIn()` receives a function name and already evaluated arguments. If the name is known, it executes the built-in. If the name is unknown, it calls `interpreterCallFunction()` so user-defined functions work using the same call syntax.

### `input(prompt?)`

Reads a line from standard input.

How it works:

- Accepts zero or one argument.
- If one argument exists, prints it as a prompt.
- Reads a full line with `getline(cin, inputText)`.
- Returns the input as text.

### `len(value)`

Returns the length of the value after converting it to string.

### `text(value)` and `str(value)`

Converts any value to text with `valueToString()`.

### `number(value)` and `float(value)`

Converts text or number to a numeric value.

How it works:

- If the input is already a number, returns it.
- Otherwise trims the text and parses it with `parseNumberText()`.
- Records an error for empty or invalid numeric text.

### `int(value)`

Converts a value to a number, then truncates it to an integer.

How it works:

- Uses the same conversion as `number()`.
- Casts the result to `int`.
- Returns it as a PyPy number.

### `bool(value)`

Returns `1` if the value is truthy, otherwise `0`.

Uses the same truth rules as `if` and `while`.

### `type(value)`

Returns the type name as text:

- `"number"`
- `"text"`
- `"null"`

### `abs(number)`

Returns the positive version of a number.

### `floor(number)`

Returns the largest whole number not greater than the input.

Implementation detail:

- Uses integer casting.
- For negative decimals, subtracts one after casting.

### `ceil(number)`

Returns the smallest whole number not less than the input.

Implementation detail:

- Uses integer casting.
- For positive decimals, adds one after casting.

### `round(number)`

Rounds to the nearest whole number.

Implementation detail:

- Adds `0.5` for positive numbers.
- Subtracts `0.5` for negative numbers.
- Casts to `int`.

### `sqrt(number)`

Returns square root.

How it works:

- Calls `manualSqrt()`.
- Rejects negative inputs.

### `min(a, b)`

Returns the smaller of two numbers.

### `max(a, b)`

Returns the larger of two numbers.

### `clamp(value, low, high)`

Restricts a number to a range.

How it works:

- Rejects when `low > high`.
- Returns `low` if the value is too small.
- Returns `high` if the value is too large.
- Otherwise returns the original value.

### `pow(base, exponent)`

Raises a number to a whole-number exponent.

How it works:

- Requires the exponent to be a whole number.
- Calls `manualPower()`.
- Rejects cases where a negative exponent would divide by zero.

### `linear_y(slope, x, intercept)`

Calculates a linear equation result.

Formula:

```text
y = slope * x + intercept
```

### `solve_linear(a, b)`

Solves:

```text
ax + b = 0
```

How it works:

- If `a == 0` and `b == 0`, returns `"infinite solutions"`.
- If `a == 0` and `b != 0`, returns `"no solution"`.
- Otherwise returns text like `"x = 5"`.

### `quadratic_roots(a, b, c)`

Solves:

```text
ax^2 + bx + c = 0
```

How it works:

- If `a == 0`, falls back to `solve_linear(b, c)`.
- Calculates the discriminant `b * b - 4 * a * c`.
- If the discriminant is negative, returns `"no real roots"`.
- Calculates the square root with `manualSqrt()`.
- Returns one root if both roots are equal.
- Otherwise returns text like `"x1 = 3, x2 = 2"`.

### `upper(value)`

Converts ASCII lowercase letters to uppercase.

How it works:

- Converts the value to text.
- Loops through each character.
- Calls `charToUpper()`.

### `lower(value)`

Converts ASCII uppercase letters to lowercase.

How it works:

- Converts the value to text.
- Loops through each character.
- Calls `charToLower()`.

### `strip(value)`

Trims whitespace from both ends of the value after converting it to text.

### `charat(text, index)`

Returns a one-character string from a text value.

How it works:

- Requires index to be a whole number.
- Checks bounds.
- Returns `string(1, text[index])`.

### `ord(character)`

Returns the ASCII code of one character.

How it works:

- Converts the argument to text.
- Requires exactly one character.
- Casts it to `unsigned char`.
- Returns the numeric code.

### `chr(code)`

Returns the character for an ASCII code.

How it works:

- Requires a whole number.
- Requires the code to be between `0` and `127`.
- Casts the number to `char`.

### `substr(text, start, length)`

Returns a substring.

How it works:

- Converts the first argument to text.
- Requires `start` and `length` to be whole numbers.
- Rejects negative values and starts beyond the end.
- If requested length goes past the end, it shortens the length.
- Returns `text.substr(start, length)`.

### `find(text, part)`

Returns the first index of `part` inside `text`.

Returns `-1` if not found.

### `contains(text, part)`

Returns `1` if `part` exists inside `text`, otherwise `0`.

### `startswith(text, part)`

Returns `1` if `text` begins with `part`, otherwise `0`.

### `endswith(text, part)`

Returns `1` if `text` ends with `part`, otherwise `0`.

### `count(text, part)`

Counts non-overlapping occurrences of `part` inside `text`.

How it works:

- Rejects empty `part`.
- Scans the text left to right.
- When a match is found, increments total and skips by `part.length()`.
- Otherwise moves forward one character.

### `replace(text, old, new)`

Replaces all non-overlapping occurrences of `old` with `new`.

How it works:

- Rejects empty `old`.
- Scans the text left to right.
- Adds `new` to the result when `old` matches.
- Otherwise copies the current character.

### `repeattext(text, count)`

Repeats text a fixed number of times.

How it works:

- Requires `count` to be a whole number.
- Requires `0 <= count <= 10000`.
- Appends the text in a loop.

### `seed(value)`

Sets the interpreter random seed.

How it works:

- Requires a whole number.
- Stores it in `interpreter->randomSeed`.
- Converts negative seeds to positive.
- Returns the stored seed.

### `rand()`

Returns a pseudo-random decimal between `0` and `1`.

How it works:

- Updates the interpreter seed using a linear congruential formula.
- Uses `randomSeed % 1000000`.
- Divides by `1000000.0`.

### `randint(low, high)`

Returns a pseudo-random whole number in a range.

How it works:

- Updates the interpreter seed using the same formula as `rand()`.
- Requires whole-number low and high values.
- Rejects `low > high`.
- Uses modulo to fit the number into the requested range.

### `fileexists(path)`

Returns `1` if the file can be opened for reading, otherwise `0`.

### `fileread(path)`

Reads an entire file into text.

How it works:

- Opens the file.
- Reads every line with `getline()`.
- Rebuilds the file contents with `\n` between lines.
- Returns the full text.

### `filesize(path)`

Returns the length of the text read from the file.

Important detail:

- This is not raw byte size from the filesystem.
- It is the length of the reconstructed text used by `fileread()`.

### `linecount(path)`

Returns how many lines are read from the file.

### Unknown built-in name

If no built-in matches, the function ends with:

```cpp
return interpreterCallFunction(interpreter, name, args, argCount, line);
```

That means user-defined functions share the same call syntax as built-ins.

## `pypy_terminal.cpp` Functions

This file includes `pypy_language.cpp` and provides the actual executable entry point.

### `void printPyPyError(Interpreter *interpreter)`

Prints the current interpreter error to standard error.

How it works:

- Prints `"PyPy error"`.
- If the error has a line number, prints `" at line X"`.
- Prints the error message.
- If the source line exists, prints the original source line below the message.

This is what makes errors useful during demos.

### `void printUsage()`

Prints command-line help.

Shows:

- Script mode usage.
- Terminal mode usage.
- Help flag.
- Terminal commands.

Called when no arguments are provided or when `--help` or `-h` is used.

### `bool runScript(string path)`

Runs one `.pypy` script file.

How it works:

- Creates an `Interpreter` record.
- Calls `initInterpreter(&interpreter)`.
- Calls `interpreterLoadFile(&interpreter, path)`.
- Prints an error and returns false if loading fails.
- Calls `interpreterRun(&interpreter)`.
- Prints an error and returns false if execution fails.
- Calls `freeInterpreter(&interpreter)` before returning.
- Returns true on success.

### `void copyBufferToProgram(Interpreter *interpreter, string buffer[], int count)`

Copies terminal-buffered code into an interpreter program.

How it works:

- Sets `interpreter->program.count`.
- Copies each buffered line into `interpreter->program.lines`.

Used before executing terminal mode code.

### `void runBufferedProgram(string buffer[], int count)`

Runs the current terminal buffer as a program.

How it works:

- Creates a new `Interpreter` record.
- Calls `initInterpreter(&program)`.
- Copies the buffer into it.
- Calls `interpreterRun(&program)`.
- Prints interpreter errors if execution fails.
- Calls `freeInterpreter(&program)` before returning.

Important behavior:

- Every `execute` runs in a fresh interpreter.
- Variables from one terminal execution do not automatically survive into the next execution unless the user keeps the code in the buffer.

### `void printTerminalHelp()`

Prints terminal-mode commands.

Shows:

- `execute`
- `show`
- `clear`
- `save path`
- `load path`
- `help`
- `exit`

Also explains that normal lines are stored and not executed immediately.

### `void saveBuffer(string buffer[], int count, string path)`

Saves terminal-buffered code to a file.

How it works:

- Rejects empty paths.
- Opens the target file with `ofstream`.
- Writes each buffered line followed by `endl`.
- Prints a success or failure message.

Used by terminal command:

```text
save path
```

### `void loadBuffer(string buffer[], int *count, string path)`

Loads source code from a file into the terminal buffer.

How it works:

- Rejects empty paths.
- Opens the file with `ifstream`.
- Clears the existing buffer by setting `*count = 0`.
- Reads lines into the buffer.
- Stops at `MAX_LINES`.
- Prints how many lines were loaded.

Used by terminal command:

```text
load path
```

### `void runTerminal()`

Starts the buffered PyPy terminal.

How it works:

- Creates a fixed-size string buffer.
- Prints a startup message.
- Enters an input loop.
- Shows the prompt `pypy-buffer>`.
- Reads user input with `getline()`.
- Trims the input to detect commands.
- Handles terminal commands immediately.
- Stores any non-command line in the buffer as PyPy source code.

Handled commands:

- `exit` or `:exit`: leave terminal mode.
- `help`: show terminal help.
- `clear`: empty the buffer.
- `show`: display buffered lines with line numbers.
- `save path`: write the buffer to a file.
- `load path`: load a file into the buffer.
- `execute`: run the whole buffer.

Important behavior:

- Normal PyPy lines do not run immediately.
- Code runs only when the user types `execute`.

### `int main(int argc, char *argv[])`

Program entry point.

How it works:

- If no arguments are provided, prints usage and returns `1`.
- If the first argument is `--help` or `-h`, prints usage and returns `0`.
- If the first argument is `--terminal` or `-t`, starts terminal mode and returns `0`.
- Otherwise treats the first argument as a script path.
- Calls `runScript(first)`.
- Returns `0` on successful script execution and `1` on failure.

## Statement Implementation Summary

This section maps PyPy language syntax to the C++ function that implements it.

| PyPy syntax | Implemented by |
|---|---|
| `let name = expression` | `interpreterExecuteBlock()` plus `envDeclareScalar()` |
| `name = expression` | `interpreterExecuteBlock()` plus `envAssignScalar()` |
| `say expression` | `interpreterExecuteBlock()` |
| `print expression` | `interpreterExecuteBlock()` |
| `ask name prompt` | `interpreterExecuteBlock()` |
| `if ... else ... end` | `interpreterExecuteBlock()`, `interpreterFindElseOrEnd()`, `interpreterFindMatchingEnd()` |
| `while ... end` | `interpreterExecuteBlock()`, `interpreterFindMatchingEnd()` |
| `repeat ... end` | `interpreterExecuteBlock()`, `interpreterFindMatchingEnd()` |
| `func name(...) ... end` | `interpreterPreprocessFunctions()`, `interpreterParseFunctionHeader()`, `interpreterCallFunction()` |
| `return expression` | `interpreterExecuteBlock()` and `ExecResult` |
| `array name[size]` | `interpreterHandleArrayDeclaration()` and `envDeclareArray()` |
| `matrix name[rows][cols]` | `interpreterHandleMatrixDeclaration()` and `envDeclareMatrix()` |
| `set arr[index] = value` | `interpreterHandleSet()` |
| `set mat[row][col] = value` | `interpreterHandleSet()` |
| `filewrite path, value` | `interpreterHandleFileWrite(..., false)` |
| `fileappend path, value` | `interpreterHandleFileWrite(..., true)` |
| Built-in function calls | `parserCallBuiltIn()` |
| User-defined function calls | `parserCallBuiltIn()` then `interpreterCallFunction()` |

## Expression Implementation Summary

| Expression feature | Implemented by |
|---|---|
| Tokenization | `parserTokenize()` |
| Parentheses | `parserParsePrimary()` |
| Function calls | `parserParsePrimary()` and `parserCallBuiltIn()` |
| Variables | `parserParsePrimary()` and `envFindAny()` |
| Arrays and matrices | `parserParsePrimary()` |
| Unary `-` | `parserParseUnary()` |
| `not` | `parserParseUnary()` |
| Power `^` | `parserParsePower()` and `manualPower()` |
| `*`, `/`, `%` | `parserParseFactor()` |
| `+`, `-` | `parserParseTerm()` |
| Comparisons | `parserParseComparison()` |
| Equality | `parserParseEquality()` |
| `and` | `parserParseAnd()` |
| `or` | `parserParseOr()` |

## How To Add A New Built-In Function

1. Open `parserCallBuiltIn()`.
2. Add a new `if (name == "your_function")` block before the final user-defined function fallback.
3. Validate `argCount`.
4. Validate argument types.
5. Return a `Value` using `makeNumber()`, `makeText()`, or `makeNull()`.
6. Use `fail(error, line, "...")` for invalid usage.
7. Add an example to `examples/builtins_showcase.pypy`.
8. Add the function to `LANGUAGE_GUIDE.md` and `README.md`.

Example pattern:

```cpp
if (name == "double")
{
    if (argCount != 1 || args[0].type != VAL_NUMBER)
    {
        fail(error, line, "double() expects one number.");
        return makeNull();
    }
    return makeNumber(args[0].number * 2);
}
```

## How To Add A New Statement

1. Open `interpreterExecuteBlock()`.
2. Add a new `if (startsWithWord(line, "keyword"))` block.
3. Parse the rest of the line after the keyword.
4. Use `interpreterEvalExpression()` for values.
5. Use `fail(&error, lineNumber, "...")` for invalid syntax.
6. `continue` after handling the statement.
7. If the statement creates a block, update `interpreterIsBlockStart()` and use `interpreterFindMatchingEnd()`.
8. Add examples and documentation.

## Common Debugging Tips

- If a line is not recognized, check `interpreterExecuteBlock()` first.
- If an expression fails, check `parserTokenize()` and the `parserParse...()` precedence chain.
- If variables behave unexpectedly inside functions, check `envFindAny()` and `interpreterCallFunction()`.
- If a block closes incorrectly, check `interpreterIsBlockStart()`, `interpreterFindMatchingEnd()`, and `interpreterFindElseOrEnd()`.
- If file commands fail, check `interpreterHandleFileWrite()` and the file built-ins.
- If a built-in name is not recognized, remember the final fallback tries to call a user-defined function with the same name.
