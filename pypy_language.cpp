#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

using namespace std;

const int MAX_LINES = 2000;
const int MAX_VARS = 150;
const int MAX_CELLS = 400;
const int MAX_FUNCTIONS = 80;
const int MAX_PARAMS = 12;
const int MAX_ARGS = 12;
const int MAX_TOKENS = 500;
const int MAX_CALL_DEPTH = 35;
const int MAX_LOOP_STEPS = 200000;

enum ValueType
{
    VAL_NULL,
    VAL_NUMBER,
    VAL_TEXT
};

enum VarKind
{
    VAR_SCALAR,
    VAR_ARRAY,
    VAR_MATRIX
};

enum TokenType
{
    TOK_END,
    TOK_NUMBER,
    TOK_TEXT,
    TOK_IDENT,
    TOK_OP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_LBRACKET,
    TOK_RBRACKET
};

struct Value
{
    int type;
    double number;
    string text;
};

struct Token
{
    int type;
    string text;
    double number;
};

struct ErrorState
{
    bool failed;
    int line;
    string message;
};

struct Variable
{
    bool used;
    string name;
    int kind;
    int rows;
    int cols;
    Value scalar;
    Value *cells;
};

struct Program
{
    string lines[MAX_LINES];
    int count;
};

struct FunctionDef
{
    bool used;
    string name;
    string params[MAX_PARAMS];
    int paramCount;
    int startLine;
    int endLine;
};

struct ExecResult
{
    bool hasReturn;
    Value value;
};

struct Environment;
struct Interpreter;

Value makeNull()
{
    Value value;
    value.type = VAL_NULL;
    value.number = 0;
    value.text = "";
    return value;
}

Value makeNumber(double number)
{
    Value value;
    value.type = VAL_NUMBER;
    value.number = number;
    value.text = "";
    return value;
}

Value makeText(string text)
{
    Value value;
    value.type = VAL_TEXT;
    value.number = 0;
    value.text = text;
    return value;
}

bool charIsSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f';
}

bool charIsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

bool charIsAlpha(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

bool charIsAlphaNumeric(char ch)
{
    return charIsAlpha(ch) || charIsDigit(ch);
}

char charToUpper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
    {
        return (char)(ch - 'a' + 'A');
    }
    return ch;
}

char charToLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

string trim(string text)
{
    int start = 0;
    int end = (int)text.length() - 1;
    while (start <= end && charIsSpace(text[start]))
    {
        start++;
    }
    while (end >= start && charIsSpace(text[end]))
    {
        end--;
    }
    if (start > end)
    {
        return "";
    }
    return text.substr(start, end - start + 1);
}

bool parseNumberText(string text, double *out)
{
    text = trim(text);
    if (text == "")
    {
        return false;
    }

    int i = 0;
    int sign = 1;
    if (text[i] == '+' || text[i] == '-')
    {
        if (text[i] == '-')
        {
            sign = -1;
        }
        i++;
    }

    double value = 0;
    bool hasDigit = false;
    while (i < (int)text.length() && charIsDigit(text[i]))
    {
        value = value * 10 + (text[i] - '0');
        hasDigit = true;
        i++;
    }

    if (i < (int)text.length() && text[i] == '.')
    {
        i++;
        double place = 0.1;
        while (i < (int)text.length() && charIsDigit(text[i]))
        {
            value = value + (text[i] - '0') * place;
            place = place / 10;
            hasDigit = true;
            i++;
        }
    }

    if (!hasDigit || i != (int)text.length())
    {
        return false;
    }
    *out = value * sign;
    return true;
}

string numberToString(double number)
{
    string text = to_string(number);
    while (text.length() > 1 && text[text.length() - 1] == '0')
    {
        text.erase(text.length() - 1, 1);
    }
    if (text.length() > 1 && text[text.length() - 1] == '.')
    {
        text.erase(text.length() - 1, 1);
    }
    if (text == "-0")
    {
        text = "0";
    }
    return text;
}

string valueToString(Value value)
{
    if (value.type == VAL_TEXT)
    {
        return value.text;
    }
    if (value.type == VAL_NUMBER)
    {
        return numberToString(value.number);
    }
    return "null";
}

bool isTruthy(Value value)
{
    if (value.type == VAL_NUMBER)
    {
        return value.number != 0;
    }
    if (value.type == VAL_TEXT)
    {
        return value.text.length() > 0;
    }
    return false;
}

bool isIdentifierStart(char ch)
{
    return charIsAlpha(ch) || ch == '_';
}

bool isIdentifierPart(char ch)
{
    return charIsAlphaNumeric(ch) || ch == '_';
}

bool isValidIdentifier(string name)
{
    if (name.length() == 0 || !isIdentifierStart(name[0]))
    {
        return false;
    }
    for (int i = 1; i < (int)name.length(); i++)
    {
        if (!isIdentifierPart(name[i]))
        {
            return false;
        }
    }
    if (name == "let" || name == "if" || name == "else" || name == "end" ||
        name == "while" || name == "repeat" || name == "func" || name == "return" ||
        name == "and" || name == "or" || name == "not" || name == "array" ||
        name == "matrix" || name == "set" || name == "say" || name == "ask")
    {
        return false;
    }
    return true;
}

bool startsWithWord(string line, string word)
{
    if (line.substr(0, word.length()) != word)
    {
        return false;
    }
    if ((int)line.length() == (int)word.length())
    {
        return true;
    }
    return charIsSpace(line[word.length()]);
}

string removeComment(string line)
{
    bool inString = false;
    bool escaped = false;
    for (int i = 0; i < (int)line.length(); i++)
    {
        char ch = line[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }
        }
        else
        {
            if (ch == '"')
            {
                inString = true;
            }
            else if (ch == '#')
            {
                return line.substr(0, i);
            }
            else if (ch == '/' && i + 1 < (int)line.length() && line[i + 1] == '/')
            {
                return line.substr(0, i);
            }
        }
    }
    return line;
}

void fail(ErrorState *error, int line, string message)
{
    if (!error->failed)
    {
        error->failed = true;
        error->line = line;
        error->message = message;
    }
}

bool isIntegerValue(double value)
{
    int asInt = (int)value;
    return value == (double)asInt;
}

double manualPower(double base, int exponent, bool *ok)
{
    double result = 1;
    int times = exponent < 0 ? -exponent : exponent;
    for (int i = 0; i < times; i++)
    {
        result = result * base;
    }
    if (exponent < 0)
    {
        if (result == 0)
        {
            *ok = false;
            return 0;
        }
        result = 1 / result;
    }
    *ok = true;
    return result;
}

double manualSqrt(double value, bool *ok)
{
    if (value < 0)
    {
        *ok = false;
        return 0;
    }
    if (value == 0)
    {
        *ok = true;
        return 0;
    }
    double guess = value;
    for (int i = 0; i < 30; i++)
    {
        guess = (guess + value / guess) / 2;
    }
    *ok = true;
    return guess;
}

struct Environment
{
    Variable *vars;
    Environment *parent;
};

void initEnvironment(Environment *env, Environment *parentEnv)
{
    env->parent = parentEnv;
    env->vars = new Variable[MAX_VARS];
    for (int i = 0; i < MAX_VARS; i++)
    {
        env->vars[i].used = false;
        env->vars[i].name = "";
        env->vars[i].kind = VAR_SCALAR;
        env->vars[i].rows = 0;
        env->vars[i].cols = 0;
        env->vars[i].scalar = makeNull();
        env->vars[i].cells = new Value[MAX_CELLS];
        for (int j = 0; j < MAX_CELLS; j++)
        {
            env->vars[i].cells[j] = makeNull();
        }
    }
}

void freeEnvironment(Environment *env)
{
    if (env->vars == NULL)
    {
        return;
    }
    for (int i = 0; i < MAX_VARS; i++)
    {
        delete[] env->vars[i].cells;
        env->vars[i].cells = NULL;
    }
    delete[] env->vars;
    env->vars = NULL;
}

Variable *envFindLocal(Environment *env, string name)
{
    for (int i = 0; i < MAX_VARS; i++)
    {
        if (env->vars[i].used && env->vars[i].name == name)
        {
            return &env->vars[i];
        }
    }
    return NULL;
}

Variable *envFindAny(Environment *env, string name)
{
    Variable *local = envFindLocal(env, name);
    if (local != NULL)
    {
        return local;
    }
    if (env->parent != NULL)
    {
        return envFindAny(env->parent, name);
    }
    return NULL;
}

Variable *envAllocate(Environment *env, string name, ErrorState *error, int line)
{
    if (!isValidIdentifier(name))
    {
        fail(error, line, "Invalid identifier '" + name + "'.");
        return NULL;
    }
    Variable *existing = envFindLocal(env, name);
    if (existing != NULL)
    {
        return existing;
    }
    for (int i = 0; i < MAX_VARS; i++)
    {
        if (!env->vars[i].used)
        {
            env->vars[i].used = true;
            env->vars[i].name = name;
            env->vars[i].kind = VAR_SCALAR;
            env->vars[i].rows = 0;
            env->vars[i].cols = 0;
            env->vars[i].scalar = makeNull();
            for (int j = 0; j < MAX_CELLS; j++)
            {
                env->vars[i].cells[j] = makeNull();
            }
            return &env->vars[i];
        }
    }
    fail(error, line, "Variable table is full. Maximum variables per scope is " + numberToString(MAX_VARS) + ".");
    return NULL;
}

void envDeclareScalar(Environment *env, string name, Value value, ErrorState *error, int line)
{
    Variable *var = envAllocate(env, name, error, line);
    if (var == NULL)
    {
        return;
    }
    var->kind = VAR_SCALAR;
    var->rows = 0;
    var->cols = 0;
    var->scalar = value;
}

void envAssignScalar(Environment *env, string name, Value value, ErrorState *error, int line)
{
    if (!isValidIdentifier(name))
    {
        fail(error, line, "Invalid identifier '" + name + "'.");
        return;
    }
    Variable *var = envFindAny(env, name);
    if (var == NULL)
    {
        var = envAllocate(env, name, error, line);
    }
    if (var == NULL)
    {
        return;
    }
    if (var->kind != VAR_SCALAR)
    {
        fail(error, line, "'" + name + "' is not a scalar variable.");
        return;
    }
    var->scalar = value;
}

void envDeclareArray(Environment *env, string name, int size, ErrorState *error, int line)
{
    if (size < 1 || size > MAX_CELLS)
    {
        fail(error, line, "Array size must be between 1 and " + numberToString(MAX_CELLS) + ".");
        return;
    }
    Variable *var = envAllocate(env, name, error, line);
    if (var == NULL)
    {
        return;
    }
    var->kind = VAR_ARRAY;
    var->rows = size;
    var->cols = 1;
    var->scalar = makeNull();
    for (int i = 0; i < size; i++)
    {
        var->cells[i] = makeNull();
    }
}

void envDeclareMatrix(Environment *env, string name, int rows, int cols, ErrorState *error, int line)
{
    if (rows < 1 || cols < 1)
    {
        fail(error, line, "Matrix dimensions must be positive.");
        return;
    }
    if (rows * cols > MAX_CELLS)
    {
        fail(error, line, "Matrix has too many cells. Maximum total cells is " + numberToString(MAX_CELLS) + ".");
        return;
    }
    Variable *var = envAllocate(env, name, error, line);
    if (var == NULL)
    {
        return;
    }
    var->kind = VAR_MATRIX;
    var->rows = rows;
    var->cols = cols;
    var->scalar = makeNull();
    for (int i = 0; i < rows * cols; i++)
    {
        var->cells[i] = makeNull();
    }
}


struct ExpressionParser
{
    Token tokens[MAX_TOKENS];
    int count;
    int pos;
    int line;
    ErrorState *error;
    Environment *env;
    Interpreter *interpreter;
};

void initExpressionParser(ExpressionParser *parser, ErrorState *err, Environment *environment, Interpreter *owner, int lineNumber)
{
    parser->count = 0;
    parser->pos = 0;
    parser->line = lineNumber;
    parser->error = err;
    parser->env = environment;
    parser->interpreter = owner;
}

bool parserTokenize(ExpressionParser *parser, string expression);
Token parserPeek(ExpressionParser *parser);
bool parserMatchType(ExpressionParser *parser, int type);
bool parserMatchOp(ExpressionParser *parser, string op);
bool parserMatchIdent(ExpressionParser *parser, string ident);
bool parserNeedNumber(ExpressionParser *parser, Value value, double *out, string operation);
Value parserParseExpression(ExpressionParser *parser);
Value parserParseOr(ExpressionParser *parser);
Value parserParseAnd(ExpressionParser *parser);
Value parserParseEquality(ExpressionParser *parser);
Value parserParseComparison(ExpressionParser *parser);
Value parserParseTerm(ExpressionParser *parser);
Value parserParseFactor(ExpressionParser *parser);
Value parserParsePower(ExpressionParser *parser);
Value parserParseUnary(ExpressionParser *parser);
Value parserParsePrimary(ExpressionParser *parser);
Value parserCallBuiltIn(ExpressionParser *parser, string name, Value args[], int argCount);
Value interpreterCallFunction(Interpreter *interpreter, string name, Value args[], int argCount, int line);

bool parserTokenize(ExpressionParser *parser, string expression)
{
    Token *tokens = parser->tokens;
    int &count = parser->count;
    int &pos = parser->pos;
    int &line = parser->line;
    ErrorState *error = parser->error;
    count = 0;
    pos = 0;
    int i = 0;
    while (i < (int)expression.length())
    {
        char ch = expression[i];
        if (charIsSpace(ch))
        {
            i++;
            continue;
        }
        if (count >= MAX_TOKENS - 1)
        {
            fail(error, line, "Expression is too long.");
            return false;
        }
        if (charIsDigit(ch) || (ch == '.' && i + 1 < (int)expression.length() && charIsDigit(expression[i + 1])))
        {
            int start = i;
            bool dotSeen = false;
            while (i < (int)expression.length() && (charIsDigit(expression[i]) || expression[i] == '.'))
            {
                if (expression[i] == '.')
                {
                    if (dotSeen)
                    {
                        fail(error, line, "Invalid number literal.");
                        return false;
                    }
                    dotSeen = true;
                }
                i++;
            }
            tokens[count].type = TOK_NUMBER;
            tokens[count].text = expression.substr(start, i - start);
            if (!parseNumberText(tokens[count].text, &tokens[count].number))
            {
                fail(error, line, "Invalid number literal.");
                return false;
            }
            count++;
            continue;
        }
        if (isIdentifierStart(ch))
        {
            int start = i;
            while (i < (int)expression.length() && isIdentifierPart(expression[i]))
            {
                i++;
            }
            tokens[count].type = TOK_IDENT;
            tokens[count].text = expression.substr(start, i - start);
            tokens[count].number = 0;
            count++;
            continue;
        }
        if (ch == '"')
        {
            i++;
            string value = "";
            bool closed = false;
            while (i < (int)expression.length())
            {
                char current = expression[i];
                if (current == '\\')
                {
                    i++;
                    if (i >= (int)expression.length())
                    {
                        fail(error, line, "Unfinished string escape.");
                        return false;
                    }
                    char escaped = expression[i];
                    switch (escaped)
                    {
                    case 'n':
                        value += '\n';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    case '"':
                        value += '"';
                        break;
                    case '\\':
                        value += '\\';
                        break;
                    default:
                        fail(error, line, "Unknown string escape '\\" + string(1, escaped) + "'.");
                        return false;
                    }
                }
                else if (current == '"')
                {
                    closed = true;
                    i++;
                    break;
                }
                else
                {
                    value += current;
                }
                i++;
            }
            if (!closed)
            {
                fail(error, line, "Unclosed string literal.");
                return false;
            }
            tokens[count].type = TOK_TEXT;
            tokens[count].text = value;
            tokens[count].number = 0;
            count++;
            continue;
        }
        if (i + 1 < (int)expression.length())
        {
            string two = expression.substr(i, 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=")
            {
                tokens[count].type = TOK_OP;
                tokens[count].text = two;
                tokens[count].number = 0;
                count++;
                i += 2;
                continue;
            }
        }
        switch (ch)
        {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '^':
        case '<':
        case '>':
        case '=':
            tokens[count].type = TOK_OP;
            tokens[count].text = string(1, ch);
            tokens[count].number = 0;
            count++;
            i++;
            break;
        case '(':
            tokens[count].type = TOK_LPAREN;
            tokens[count].text = "(";
            tokens[count].number = 0;
            count++;
            i++;
            break;
        case ')':
            tokens[count].type = TOK_RPAREN;
            tokens[count].text = ")";
            tokens[count].number = 0;
            count++;
            i++;
            break;
        case '[':
            tokens[count].type = TOK_LBRACKET;
            tokens[count].text = "[";
            tokens[count].number = 0;
            count++;
            i++;
            break;
        case ']':
            tokens[count].type = TOK_RBRACKET;
            tokens[count].text = "]";
            tokens[count].number = 0;
            count++;
            i++;
            break;
        case ',':
            tokens[count].type = TOK_COMMA;
            tokens[count].text = ",";
            tokens[count].number = 0;
            count++;
            i++;
            break;
        default:
            fail(error, line, "Unexpected character '" + string(1, ch) + "' in expression.");
            return false;
        }
    }
    tokens[count].type = TOK_END;
    tokens[count].text = "";
    tokens[count].number = 0;
    count++;
    return true;
}

Token parserPeek(ExpressionParser *parser)
{
    Token *tokens = parser->tokens;
    int &pos = parser->pos;
    return tokens[pos];
}

bool parserMatchType(ExpressionParser *parser, int type)
{
    Token *tokens = parser->tokens;
    int &pos = parser->pos;
    if (tokens[pos].type == type)
    {
        pos++;
        return true;
    }
    return false;
}

bool parserMatchOp(ExpressionParser *parser, string op)
{
    Token *tokens = parser->tokens;
    int &pos = parser->pos;
    if (tokens[pos].type == TOK_OP && tokens[pos].text == op)
    {
        pos++;
        return true;
    }
    return false;
}

bool parserMatchIdent(ExpressionParser *parser, string ident)
{
    Token *tokens = parser->tokens;
    int &pos = parser->pos;
    if (tokens[pos].type == TOK_IDENT && tokens[pos].text == ident)
    {
        pos++;
        return true;
    }
    return false;
}

bool parserNeedNumber(ExpressionParser *parser, Value value, double *out, string operation)
{
    int &line = parser->line;
    ErrorState *error = parser->error;
    if (value.type != VAL_NUMBER)
    {
        fail(error, line, operation + " requires a number.");
        return false;
    }
    *out = value.number;
    return true;
}

Value parserParseExpression(ExpressionParser *parser)
{
    return parserParseOr(parser);
}

Value parserParseOr(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    Value left = parserParseAnd(parser);
    while (!error->failed && parserMatchIdent(parser, "or"))
    {
        Value right = parserParseAnd(parser);
        left = makeNumber(isTruthy(left) || isTruthy(right) ? 1 : 0);
    }
    return left;
}

Value parserParseAnd(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    Value left = parserParseEquality(parser);
    while (!error->failed && parserMatchIdent(parser, "and"))
    {
        Value right = parserParseEquality(parser);
        left = makeNumber(isTruthy(left) && isTruthy(right) ? 1 : 0);
    }
    return left;
}

Value parserParseEquality(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    int &pos = parser->pos;
    Value left = parserParseComparison(parser);
    while (!error->failed && (parserPeek(parser).type == TOK_OP && (parserPeek(parser).text == "==" || parserPeek(parser).text == "!=")))
    {
        string op = parserPeek(parser).text;
        pos++;
        Value right = parserParseComparison(parser);
        bool equal = false;
        if (left.type == VAL_TEXT || right.type == VAL_TEXT)
        {
            equal = valueToString(left) == valueToString(right);
        }
        else
        {
            equal = left.number == right.number;
        }
        left = makeNumber(op == "==" ? (equal ? 1 : 0) : (!equal ? 1 : 0));
    }
    return left;
}

Value parserParseComparison(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    int &pos = parser->pos;
    Value left = parserParseTerm(parser);
    while (!error->failed && (parserPeek(parser).type == TOK_OP && (parserPeek(parser).text == "<" || parserPeek(parser).text == "<=" || parserPeek(parser).text == ">" || parserPeek(parser).text == ">=")))
    {
        string op = parserPeek(parser).text;
        pos++;
        Value right = parserParseTerm(parser);
        if (left.type == VAL_TEXT || right.type == VAL_TEXT)
        {
            string a = valueToString(left);
            string b = valueToString(right);
            if (op == "<")
                left = makeNumber(a < b ? 1 : 0);
            else if (op == "<=")
                left = makeNumber(a <= b ? 1 : 0);
            else if (op == ">")
                left = makeNumber(a > b ? 1 : 0);
            else
                left = makeNumber(a >= b ? 1 : 0);
        }
        else
        {
            if (op == "<")
                left = makeNumber(left.number < right.number ? 1 : 0);
            else if (op == "<=")
                left = makeNumber(left.number <= right.number ? 1 : 0);
            else if (op == ">")
                left = makeNumber(left.number > right.number ? 1 : 0);
            else
                left = makeNumber(left.number >= right.number ? 1 : 0);
        }
    }
    return left;
}

Value parserParseTerm(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    int &pos = parser->pos;
    Value left = parserParseFactor(parser);
    while (!error->failed && (parserPeek(parser).type == TOK_OP && (parserPeek(parser).text == "+" || parserPeek(parser).text == "-")))
    {
        string op = parserPeek(parser).text;
        pos++;
        Value right = parserParseFactor(parser);
        if (op == "+")
        {
            if (left.type == VAL_TEXT || right.type == VAL_TEXT)
            {
                left = makeText(valueToString(left) + valueToString(right));
            }
            else
            {
                left = makeNumber(left.number + right.number);
            }
        }
        else
        {
            double a, b;
            if (!parserNeedNumber(parser, left, &a, "Subtraction") || !parserNeedNumber(parser, right, &b, "Subtraction"))
            {
                return makeNull();
            }
            left = makeNumber(a - b);
        }
    }
    return left;
}

Value parserParseFactor(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    int &pos = parser->pos;
    int &line = parser->line;
    Value left = parserParsePower(parser);
    while (!error->failed && (parserPeek(parser).type == TOK_OP && (parserPeek(parser).text == "*" || parserPeek(parser).text == "/" || parserPeek(parser).text == "%")))
    {
        string op = parserPeek(parser).text;
        pos++;
        Value right = parserParsePower(parser);
        double a, b;
        if (!parserNeedNumber(parser, left, &a, "Arithmetic") || !parserNeedNumber(parser, right, &b, "Arithmetic"))
        {
            return makeNull();
        }
        if ((op == "/" || op == "%") && b == 0)
        {
            fail(error, line, "Division by zero.");
            return makeNull();
        }
        if (op == "*")
        {
            left = makeNumber(a * b);
        }
        else if (op == "/")
        {
            left = makeNumber(a / b);
        }
        else
        {
            if (!isIntegerValue(a) || !isIntegerValue(b))
            {
                fail(error, line, "Modulo requires whole numbers.");
                return makeNull();
            }
            left = makeNumber((int)a % (int)b);
        }
    }
    return left;
}

Value parserParsePower(ExpressionParser *parser)
{
    ErrorState *error = parser->error;
    int &line = parser->line;
    Value left = parserParseUnary(parser);
    if (!error->failed && parserMatchOp(parser, "^"))
    {
        Value right = parserParsePower(parser);
        double base, exponent;
        if (!parserNeedNumber(parser, left, &base, "Exponent") || !parserNeedNumber(parser, right, &exponent, "Exponent"))
        {
            return makeNull();
        }
        if (!isIntegerValue(exponent))
        {
            fail(error, line, "Exponent operator ^ requires a whole-number exponent.");
            return makeNull();
        }
        bool ok = true;
        double answer = manualPower(base, (int)exponent, &ok);
        if (!ok)
        {
            fail(error, line, "Exponent cannot divide by zero.");
            return makeNull();
        }
        return makeNumber(answer);
    }
    return left;
}

Value parserParseUnary(ExpressionParser *parser)
{
    if (parserMatchOp(parser, "-"))
    {
        Value value = parserParseUnary(parser);
        double number;
        if (!parserNeedNumber(parser, value, &number, "Unary minus"))
        {
            return makeNull();
        }
        return makeNumber(-number);
    }
    if (parserMatchIdent(parser, "not"))
    {
        Value value = parserParseUnary(parser);
        return makeNumber(isTruthy(value) ? 0 : 1);
    }
    return parserParsePrimary(parser);
}



struct Interpreter
{
    Program program;
    FunctionDef funcs[MAX_FUNCTIONS];
    Environment globals;
    ErrorState error;
    int callDepth;
    int randomSeed;
};

void initInterpreter(Interpreter *interpreter)
{
    initEnvironment(&interpreter->globals, NULL);
    interpreter->program.count = 0;
    interpreter->callDepth = 0;
    interpreter->randomSeed = 12345;
    interpreter->error.failed = false;
    interpreter->error.line = 0;
    interpreter->error.message = "";
    for (int i = 0; i < MAX_FUNCTIONS; i++)
    {
        interpreter->funcs[i].used = false;
        interpreter->funcs[i].name = "";
        interpreter->funcs[i].paramCount = 0;
        interpreter->funcs[i].startLine = 0;
        interpreter->funcs[i].endLine = 0;
    }
}

void freeInterpreter(Interpreter *interpreter)
{
    freeEnvironment(&interpreter->globals);
}

bool interpreterLoadFile(Interpreter *interpreter, string path);
string interpreterCleanLine(Interpreter *interpreter, int index);
FunctionDef *interpreterFindFunction(Interpreter *interpreter, string name);
int interpreterAllocateFunction(Interpreter *interpreter);
bool interpreterParseFunctionHeader(Interpreter *interpreter, string line, FunctionDef *target, int lineNumber);
bool interpreterIsBlockStart(Interpreter *interpreter, string line);
int interpreterFindMatchingEnd(Interpreter *interpreter, int start, int stop);
int interpreterFindElseOrEnd(Interpreter *interpreter, int start, int stop, bool *hasElse);
bool interpreterPreprocessFunctions(Interpreter *interpreter);
bool interpreterEvalExpression(Interpreter *interpreter, string expression, Environment *env, int line, Value *out);
int interpreterFindTopLevelAssignment(Interpreter *interpreter, string line);
int interpreterSplitTopLevelComma(Interpreter *interpreter, string line);
int interpreterCheckedIndex(Interpreter *interpreter, Value value, int max, int line, string subject);
bool interpreterParseNameAndBracket(Interpreter *interpreter, string text, string *name, string *first, string *second);
bool interpreterHandleArrayDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber);
bool interpreterHandleMatrixDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber);
bool interpreterHandleSet(Interpreter *interpreter, string line, Environment *env, int lineNumber);
bool interpreterHandleFileWrite(Interpreter *interpreter, string line, Environment *env, int lineNumber, bool append);
ExecResult interpreterExecuteBlock(Interpreter *interpreter, int start, int end, Environment *env);
Value interpreterCallFunction(Interpreter *interpreter, string name, Value args[], int argCount, int line);
bool interpreterRun(Interpreter *interpreter);

bool interpreterLoadFile(Interpreter *interpreter, string path)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    ifstream file(path.c_str());
    if (!file)
    {
        fail(&error, 0, "Cannot open source file '" + path + "'.");
        return false;
    }
    string line;
    while (getline(file, line))
    {
        if (program.count >= MAX_LINES)
        {
            fail(&error, program.count, "Program is too long. Maximum lines is " + numberToString(MAX_LINES) + ".");
            return false;
        }
        program.lines[program.count] = line;
        program.count++;
    }
    return true;
}

string interpreterCleanLine(Interpreter *interpreter, int index)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    return trim(removeComment(program.lines[index]));
}

FunctionDef *interpreterFindFunction(Interpreter *interpreter, string name)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    for (int i = 0; i < MAX_FUNCTIONS; i++)
    {
        if (funcs[i].used && funcs[i].name == name)
        {
            return &funcs[i];
        }
    }
    return NULL;
}

int interpreterAllocateFunction(Interpreter *interpreter)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    for (int i = 0; i < MAX_FUNCTIONS; i++)
    {
        if (!funcs[i].used)
        {
            funcs[i].used = true;
            return i;
        }
    }
    return -1;
}

bool interpreterParseFunctionHeader(Interpreter *interpreter, string line, FunctionDef *target, int lineNumber)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    int open = (int)line.find('(');
    int close = (int)line.rfind(')');
    if (open < 0 || close < 0 || close < open)
    {
        fail(&error, lineNumber, "Function must look like: func name(a, b)");
        return false;
    }
    string name = trim(line.substr(4, open - 4));
    if (!isValidIdentifier(name))
    {
        fail(&error, lineNumber, "Invalid function name '" + name + "'.");
        return false;
    }
    if (interpreterFindFunction(interpreter, name) != NULL)
    {
        fail(&error, lineNumber, "Function '" + name + "' is already defined.");
        return false;
    }
    target->name = name;
    target->paramCount = 0;
    string params = trim(line.substr(open + 1, close - open - 1));
    if (trim(line.substr(close + 1)) != "")
    {
        fail(&error, lineNumber, "Unexpected text after function parameter list.");
        return false;
    }
    if (params == "")
    {
        return true;
    }
    int start = 0;
    while (start <= (int)params.length())
    {
        int comma = (int)params.find(',', start);
        if (comma < 0)
        {
            comma = (int)params.length();
        }
        string param = trim(params.substr(start, comma - start));
        if (!isValidIdentifier(param))
        {
            fail(&error, lineNumber, "Invalid parameter name '" + param + "'.");
            return false;
        }
        if (target->paramCount >= MAX_PARAMS)
        {
            fail(&error, lineNumber, "Too many parameters. Maximum is " + numberToString(MAX_PARAMS) + ".");
            return false;
        }
        for (int i = 0; i < target->paramCount; i++)
        {
            if (target->params[i] == param)
            {
                fail(&error, lineNumber, "Duplicate parameter '" + param + "'.");
                return false;
            }
        }
        target->params[target->paramCount] = param;
        target->paramCount++;
        start = comma + 1;
        if (comma == (int)params.length())
        {
            break;
        }
    }
    return true;
}

bool interpreterIsBlockStart(Interpreter *interpreter, string line)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    return startsWithWord(line, "if") || startsWithWord(line, "while") ||
           startsWithWord(line, "repeat") || startsWithWord(line, "func");
}

int interpreterFindMatchingEnd(Interpreter *interpreter, int start, int stop)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    int depth = 0;
    for (int i = start + 1; i < stop; i++)
    {
        string line = interpreterCleanLine(interpreter, i);
        if (line == "")
        {
            continue;
        }
        if (interpreterIsBlockStart(interpreter, line))
        {
            depth++;
        }
        else if (line == "end")
        {
            if (depth == 0)
            {
                return i;
            }
            depth--;
        }
    }
    return -1;
}

int interpreterFindElseOrEnd(Interpreter *interpreter, int start, int stop, bool *hasElse)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    int depth = 0;
    *hasElse = false;
    for (int i = start + 1; i < stop; i++)
    {
        string line = interpreterCleanLine(interpreter, i);
        if (line == "")
        {
            continue;
        }
        if (interpreterIsBlockStart(interpreter, line))
        {
            depth++;
        }
        else if (line == "end")
        {
            if (depth == 0)
            {
                return i;
            }
            depth--;
        }
        else if (line == "else" && depth == 0)
        {
            *hasElse = true;
            return i;
        }
    }
    return -1;
}

bool interpreterPreprocessFunctions(Interpreter *interpreter)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    for (int i = 0; i < program.count; i++)
    {
        string line = interpreterCleanLine(interpreter, i);
        if (startsWithWord(line, "func"))
        {
            int slot = interpreterAllocateFunction(interpreter);
            if (slot < 0)
            {
                fail(&error, i + 1, "Function table is full. Maximum functions is " + numberToString(MAX_FUNCTIONS) + ".");
                return false;
            }
            if (!interpreterParseFunctionHeader(interpreter, line, &funcs[slot], i + 1))
            {
                return false;
            }
            int endLine = interpreterFindMatchingEnd(interpreter, i, program.count);
            if (endLine < 0)
            {
                fail(&error, i + 1, "Function '" + funcs[slot].name + "' is missing its closing end.");
                return false;
            }
            funcs[slot].startLine = i;
            funcs[slot].endLine = endLine;
            i = endLine;
        }
    }
    return true;
}

bool interpreterEvalExpression(Interpreter *interpreter, string expression, Environment *env, int line, Value *out)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    ExpressionParser parser;
    initExpressionParser(&parser, &error, env, interpreter, line);
    if (!parserTokenize(&parser, expression))
    {
        return false;
    }
    *out = parserParseExpression(&parser);
    if (error.failed)
    {
        return false;
    }
    if (parserPeek(&parser).type != TOK_END)
    {
        fail(&error, line, "Unexpected token '" + parserPeek(&parser).text + "' after expression.");
        return false;
    }
    return true;
}

int interpreterFindTopLevelAssignment(Interpreter *interpreter, string line)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    bool inString = false;
    bool escaped = false;
    int parens = 0;
    int brackets = 0;
    for (int i = 0; i < (int)line.length(); i++)
    {
        char ch = line[i];
        if (inString)
        {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                inString = false;
            continue;
        }
        if (ch == '"')
        {
            inString = true;
        }
        else if (ch == '(')
        {
            parens++;
        }
        else if (ch == ')')
        {
            parens--;
        }
        else if (ch == '[')
        {
            brackets++;
        }
        else if (ch == ']')
        {
            brackets--;
        }
        else if (ch == '=' && parens == 0 && brackets == 0)
        {
            char before = i > 0 ? line[i - 1] : '\0';
            char after = i + 1 < (int)line.length() ? line[i + 1] : '\0';
            if (before != '=' && before != '<' && before != '>' && before != '!' && after != '=')
            {
                return i;
            }
        }
    }
    return -1;
}

int interpreterSplitTopLevelComma(Interpreter *interpreter, string line)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    bool inString = false;
    bool escaped = false;
    int parens = 0;
    int brackets = 0;
    for (int i = 0; i < (int)line.length(); i++)
    {
        char ch = line[i];
        if (inString)
        {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                inString = false;
            continue;
        }
        if (ch == '"')
        {
            inString = true;
        }
        else if (ch == '(')
        {
            parens++;
        }
        else if (ch == ')')
        {
            parens--;
        }
        else if (ch == '[')
        {
            brackets++;
        }
        else if (ch == ']')
        {
            brackets--;
        }
        else if (ch == ',' && parens == 0 && brackets == 0)
        {
            return i;
        }
    }
    return -1;
}

int interpreterCheckedIndex(Interpreter *interpreter, Value value, int max, int line, string subject)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    if (value.type != VAL_NUMBER || !isIntegerValue(value.number))
    {
        fail(&error, line, subject + " index must be a whole number.");
        return -1;
    }
    int index = (int)value.number;
    if (index < 0 || index >= max)
    {
        fail(&error, line, subject + " index " + numberToString(index) + " is out of bounds.");
        return -1;
    }
    return index;
}

bool interpreterParseNameAndBracket(Interpreter *interpreter, string text, string *name, string *first, string *second)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    *name = "";
    *first = "";
    *second = "";
    int open1 = (int)text.find('[');
    int close1 = (int)text.find(']', open1 + 1);
    if (open1 < 0 || close1 < 0)
    {
        return false;
    }
    *name = trim(text.substr(0, open1));
    *first = trim(text.substr(open1 + 1, close1 - open1 - 1));
    string rest = trim(text.substr(close1 + 1));
    if (rest != "")
    {
        if (rest[0] != '[')
        {
            return false;
        }
        int close2 = (int)rest.find(']', 1);
        if (close2 < 0)
        {
            return false;
        }
        *second = trim(rest.substr(1, close2 - 1));
        if (trim(rest.substr(close2 + 1)) != "")
        {
            return false;
        }
    }
    return true;
}

bool interpreterHandleArrayDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    string rest = trim(line.substr(5));
    string name, first, second;
    if (!interpreterParseNameAndBracket(interpreter, rest, &name, &first, &second) || second != "")
    {
        fail(&error, lineNumber, "Array declaration must look like: array name[size]");
        return false;
    }
    Value sizeValue;
    if (!interpreterEvalExpression(interpreter, first, env, lineNumber, &sizeValue))
    {
        return false;
    }
    if (sizeValue.type != VAL_NUMBER || !isIntegerValue(sizeValue.number))
    {
        fail(&error, lineNumber, "Array size must be a whole number.");
        return false;
    }
    envDeclareArray(env, name, (int)sizeValue.number, &error, lineNumber);
    return !error.failed;
}

bool interpreterHandleMatrixDeclaration(Interpreter *interpreter, string line, Environment *env, int lineNumber)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    string rest = trim(line.substr(6));
    string name, first, second;
    if (!interpreterParseNameAndBracket(interpreter, rest, &name, &first, &second) || second == "")
    {
        fail(&error, lineNumber, "Matrix declaration must look like: matrix name[rows][cols]");
        return false;
    }
    Value rowValue, colValue;
    if (!interpreterEvalExpression(interpreter, first, env, lineNumber, &rowValue) ||
        !interpreterEvalExpression(interpreter, second, env, lineNumber, &colValue))
    {
        return false;
    }
    if (rowValue.type != VAL_NUMBER || colValue.type != VAL_NUMBER ||
        !isIntegerValue(rowValue.number) || !isIntegerValue(colValue.number))
    {
        fail(&error, lineNumber, "Matrix dimensions must be whole numbers.");
        return false;
    }
    envDeclareMatrix(env, name, (int)rowValue.number, (int)colValue.number, &error, lineNumber);
    return !error.failed;
}

bool interpreterHandleSet(Interpreter *interpreter, string line, Environment *env, int lineNumber)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    string rest = trim(line.substr(3));
    int eq = interpreterFindTopLevelAssignment(interpreter, rest);
    if (eq < 0)
    {
        fail(&error, lineNumber, "Set statement must look like: set data[index] = value");
        return false;
    }
    string target = trim(rest.substr(0, eq));
    string expression = trim(rest.substr(eq + 1));
    string name, first, second;
    if (!interpreterParseNameAndBracket(interpreter, target, &name, &first, &second))
    {
        fail(&error, lineNumber, "Set target must be an array or matrix cell.");
        return false;
    }
    Variable *var = envFindAny(env, name);
    if (var == NULL)
    {
        fail(&error, lineNumber, "Unknown array or matrix '" + name + "'.");
        return false;
    }
    Value newValue;
    if (!interpreterEvalExpression(interpreter, expression, env, lineNumber, &newValue))
    {
        return false;
    }
    Value firstValue, secondValue;
    if (!interpreterEvalExpression(interpreter, first, env, lineNumber, &firstValue))
    {
        return false;
    }
    if (var->kind == VAR_ARRAY)
    {
        if (second != "")
        {
            fail(&error, lineNumber, "'" + name + "' is a one-dimensional array.");
            return false;
        }
        int index = interpreterCheckedIndex(interpreter, firstValue, var->rows, lineNumber, "Array");
        if (index < 0)
        {
            return false;
        }
        var->cells[index] = newValue;
        return true;
    }
    if (var->kind == VAR_MATRIX)
    {
        if (second == "")
        {
            fail(&error, lineNumber, "'" + name + "' is a matrix and needs two indexes.");
            return false;
        }
        if (!interpreterEvalExpression(interpreter, second, env, lineNumber, &secondValue))
        {
            return false;
        }
        int row = interpreterCheckedIndex(interpreter, firstValue, var->rows, lineNumber, "Matrix row");
        int col = interpreterCheckedIndex(interpreter, secondValue, var->cols, lineNumber, "Matrix column");
        if (row < 0 || col < 0)
        {
            return false;
        }
        var->cells[row * var->cols + col] = newValue;
        return true;
    }
    fail(&error, lineNumber, "'" + name + "' is not an array or matrix.");
    return false;
}

bool interpreterHandleFileWrite(Interpreter *interpreter, string line, Environment *env, int lineNumber, bool append)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    string rest = trim(line.substr(append ? 10 : 9));
    int comma = interpreterSplitTopLevelComma(interpreter, rest);
    if (comma < 0)
    {
        fail(&error, lineNumber, string(append ? "fileappend" : "filewrite") + " must look like: command path, value");
        return false;
    }
    Value path, data;
    if (!interpreterEvalExpression(interpreter, trim(rest.substr(0, comma)), env, lineNumber, &path) ||
        !interpreterEvalExpression(interpreter, trim(rest.substr(comma + 1)), env, lineNumber, &data))
    {
        return false;
    }
    string filePath = valueToString(path);
    ofstream file;
    if (append)
    {
        file.open(filePath.c_str(), ios::app);
    }
    else
    {
        file.open(filePath.c_str());
    }
    if (!file)
    {
        fail(&error, lineNumber, "Cannot write to file '" + filePath + "'.");
        return false;
    }
    file << valueToString(data);
    return true;
}

ExecResult interpreterExecuteBlock(Interpreter *interpreter, int start, int end, Environment *env)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    ExecResult result;
    result.hasReturn = false;
    result.value = makeNull();
    int loopSteps = 0;

    for (int i = start; i < end && !error.failed; i++)
    {
        string line = interpreterCleanLine(interpreter, i);
        int lineNumber = i + 1;
        if (line == "")
        {
            continue;
        }
        if (startsWithWord(line, "func"))
        {
            int functionEnd = interpreterFindMatchingEnd(interpreter, i, end);
            if (functionEnd < 0)
            {
                fail(&error, lineNumber, "Function body is missing end.");
                return result;
            }
            i = functionEnd;
            continue;
        }
        if (startsWithWord(line, "let"))
        {
            string rest = trim(line.substr(3));
            int eq = interpreterFindTopLevelAssignment(interpreter, rest);
            if (eq < 0)
            {
                fail(&error, lineNumber, "Let statement must look like: let name = expression");
                return result;
            }
            string name = trim(rest.substr(0, eq));
            string expression = trim(rest.substr(eq + 1));
            Value value;
            if (interpreterEvalExpression(interpreter, expression, env, lineNumber, &value))
            {
                envDeclareScalar(env, name, value, &error, lineNumber);
            }
            continue;
        }
        if (startsWithWord(line, "say") || startsWithWord(line, "print"))
        {
            int skip = startsWithWord(line, "say") ? 3 : 5;
            string expression = trim(line.substr(skip));
            Value value;
            if (expression == "")
            {
                cout << endl;
            }
            else if (interpreterEvalExpression(interpreter, expression, env, lineNumber, &value))
            {
                cout << valueToString(value) << endl;
            }
            continue;
        }
        if (startsWithWord(line, "ask"))
        {
            string rest = trim(line.substr(3));
            int split = 0;
            while (split < (int)rest.length() && !charIsSpace(rest[split]))
            {
                split++;
            }
            string name = trim(rest.substr(0, split));
            string prompt = trim(rest.substr(split));
            if (!isValidIdentifier(name))
            {
                fail(&error, lineNumber, "Ask needs a valid variable name.");
                return result;
            }
            if (prompt != "")
            {
                Value promptValue;
                if (!interpreterEvalExpression(interpreter, prompt, env, lineNumber, &promptValue))
                {
                    return result;
                }
                cout << valueToString(promptValue);
            }
            string input;
            getline(cin, input);
            envAssignScalar(env, name, makeText(input), &error, lineNumber);
            continue;
        }
        if (startsWithWord(line, "array"))
        {
            interpreterHandleArrayDeclaration(interpreter, line, env, lineNumber);
            continue;
        }
        if (startsWithWord(line, "matrix"))
        {
            interpreterHandleMatrixDeclaration(interpreter, line, env, lineNumber);
            continue;
        }
        if (startsWithWord(line, "set"))
        {
            interpreterHandleSet(interpreter, line, env, lineNumber);
            continue;
        }
        if (startsWithWord(line, "filewrite"))
        {
            interpreterHandleFileWrite(interpreter, line, env, lineNumber, false);
            continue;
        }
        if (startsWithWord(line, "fileappend"))
        {
            interpreterHandleFileWrite(interpreter, line, env, lineNumber, true);
            continue;
        }
        if (startsWithWord(line, "if"))
        {
            string condition = trim(line.substr(2));
            if (condition == "")
            {
                fail(&error, lineNumber, "If statement needs a condition.");
                return result;
            }
            bool hasElse = false;
            int split = interpreterFindElseOrEnd(interpreter, i, end, &hasElse);
            if (split < 0)
            {
                fail(&error, lineNumber, "If statement is missing end.");
                return result;
            }
            int finalEnd = split;
            if (hasElse)
            {
                finalEnd = interpreterFindMatchingEnd(interpreter, split, end);
                if (finalEnd < 0)
                {
                    fail(&error, split + 1, "Else block is missing end.");
                    return result;
                }
            }
            Value conditionValue;
            if (!interpreterEvalExpression(interpreter, condition, env, lineNumber, &conditionValue))
            {
                return result;
            }
            if (isTruthy(conditionValue))
            {
                result = interpreterExecuteBlock(interpreter, i + 1, split, env);
            }
            else if (hasElse)
            {
                result = interpreterExecuteBlock(interpreter, split + 1, finalEnd, env);
            }
            if (result.hasReturn || error.failed)
            {
                return result;
            }
            i = finalEnd;
            continue;
        }
        if (startsWithWord(line, "while"))
        {
            string condition = trim(line.substr(5));
            if (condition == "")
            {
                fail(&error, lineNumber, "While statement needs a condition.");
                return result;
            }
            int blockEnd = interpreterFindMatchingEnd(interpreter, i, end);
            if (blockEnd < 0)
            {
                fail(&error, lineNumber, "While statement is missing end.");
                return result;
            }
            while (!error.failed)
            {
                loopSteps++;
                if (loopSteps > MAX_LOOP_STEPS)
                {
                    fail(&error, lineNumber, "Loop limit reached. Check for an infinite loop.");
                    return result;
                }
                Value conditionValue;
                if (!interpreterEvalExpression(interpreter, condition, env, lineNumber, &conditionValue))
                {
                    return result;
                }
                if (!isTruthy(conditionValue))
                {
                    break;
                }
                result = interpreterExecuteBlock(interpreter, i + 1, blockEnd, env);
                if (result.hasReturn || error.failed)
                {
                    return result;
                }
            }
            i = blockEnd;
            continue;
        }
        if (startsWithWord(line, "repeat"))
        {
            string timesText = trim(line.substr(6));
            int blockEnd = interpreterFindMatchingEnd(interpreter, i, end);
            if (blockEnd < 0)
            {
                fail(&error, lineNumber, "Repeat statement is missing end.");
                return result;
            }
            Value countValue;
            if (!interpreterEvalExpression(interpreter, timesText, env, lineNumber, &countValue))
            {
                return result;
            }
            if (countValue.type != VAL_NUMBER || !isIntegerValue(countValue.number))
            {
                fail(&error, lineNumber, "Repeat count must be a whole number.");
                return result;
            }
            int repeatCount = (int)countValue.number;
            if (repeatCount < 0)
            {
                fail(&error, lineNumber, "Repeat count cannot be negative.");
                return result;
            }
            for (int count = 0; count < repeatCount && !error.failed; count++)
            {
                loopSteps++;
                if (loopSteps > MAX_LOOP_STEPS)
                {
                    fail(&error, lineNumber, "Loop limit reached.");
                    return result;
                }
                result = interpreterExecuteBlock(interpreter, i + 1, blockEnd, env);
                if (result.hasReturn || error.failed)
                {
                    return result;
                }
            }
            i = blockEnd;
            continue;
        }
        if (startsWithWord(line, "return"))
        {
            string expression = trim(line.substr(6));
            if (expression == "")
            {
                result.value = makeNull();
            }
            else if (!interpreterEvalExpression(interpreter, expression, env, lineNumber, &result.value))
            {
                return result;
            }
            result.hasReturn = true;
            return result;
        }
        if (line == "else" || line == "end")
        {
            fail(&error, lineNumber, "Unexpected '" + line + "'.");
            return result;
        }

        int eq = interpreterFindTopLevelAssignment(interpreter, line);
        if (eq >= 0)
        {
            string name = trim(line.substr(0, eq));
            string expression = trim(line.substr(eq + 1));
            Value value;
            if (interpreterEvalExpression(interpreter, expression, env, lineNumber, &value))
            {
                envAssignScalar(env, name, value, &error, lineNumber);
            }
            continue;
        }

        Value unused;
        interpreterEvalExpression(interpreter, line, env, lineNumber, &unused);
    }
    return result;
}

Value interpreterCallFunction(Interpreter *interpreter, string name, Value args[], int argCount, int line)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    FunctionDef *function = interpreterFindFunction(interpreter, name);
    if (function == NULL)
    {
        fail(&error, line, "Unknown function '" + name + "'.");
        return makeNull();
    }
    if (argCount != function->paramCount)
    {
        fail(&error, line, "Function '" + name + "' expects " + numberToString(function->paramCount) +
                          " argument(s), got " + numberToString(argCount) + ".");
        return makeNull();
    }
    if (callDepth >= MAX_CALL_DEPTH)
    {
        fail(&error, line, "Maximum function call depth reached.");
        return makeNull();
    }
    callDepth++;
    Environment local;
    initEnvironment(&local, &globals);
    for (int i = 0; i < argCount; i++)
    {
        envDeclareScalar(&local, function->params[i], args[i], &error, line);
    }
    ExecResult result = interpreterExecuteBlock(interpreter, function->startLine + 1, function->endLine, &local);
    freeEnvironment(&local);
    callDepth--;
    if (error.failed)
    {
        return makeNull();
    }
    if (result.hasReturn)
    {
        return result.value;
    }
    return makeNull();
}

bool interpreterRun(Interpreter *interpreter)
{
    Program &program = interpreter->program;
    FunctionDef *funcs = interpreter->funcs;
    Environment &globals = interpreter->globals;
    ErrorState &error = interpreter->error;
    int &callDepth = interpreter->callDepth;
    int &randomSeed = interpreter->randomSeed;
    if (!interpreterPreprocessFunctions(interpreter))
    {
        return false;
    }
    interpreterExecuteBlock(interpreter, 0, program.count, &globals);
    return !error.failed;
}


Value parserCallBuiltIn(ExpressionParser *parser, string name, Value args[], int argCount)
{
    int &line = parser->line;
    ErrorState *error = parser->error;
    Interpreter *interpreter = parser->interpreter;
    if (name == "input")
    {
        if (argCount > 1)
        {
            fail(error, line, "input() expects zero or one prompt argument.");
            return makeNull();
        }
        if (argCount == 1)
        {
            cout << valueToString(args[0]);
        }
        string inputText;
        getline(cin, inputText);
        return makeText(inputText);
    }

    if (name == "len")
    {
        if (argCount != 1)
        {
            fail(error, line, "len() expects one argument.");
            return makeNull();
        }
        return makeNumber((double)valueToString(args[0]).length());
    }

    if (name == "text" || name == "str")
    {
        if (argCount != 1)
        {
            fail(error, line, name + "() expects one argument.");
            return makeNull();
        }
        return makeText(valueToString(args[0]));
    }

    if (name == "number" || name == "float" || name == "int")
    {
        if (argCount != 1)
        {
            fail(error, line, name + "() expects one argument.");
            return makeNull();
        }
        double value = 0;
        if (args[0].type == VAL_NUMBER)
        {
            value = args[0].number;
        }
        else
        {
            string text = trim(valueToString(args[0]));
            if (text == "" || !parseNumberText(text, &value))
            {
                fail(error, line, "Cannot convert '" + text + "' to number.");
                return makeNull();
            }
        }
        if (name == "int")
        {
            return makeNumber((int)value);
        }
        return makeNumber(value);
    }

    if (name == "bool")
    {
        if (argCount != 1)
        {
            fail(error, line, "bool() expects one argument.");
            return makeNull();
        }
        return makeNumber(isTruthy(args[0]) ? 1 : 0);
    }

    if (name == "type")
    {
        if (argCount != 1)
        {
            fail(error, line, "type() expects one argument.");
            return makeNull();
        }
        if (args[0].type == VAL_NUMBER)
        {
            return makeText("number");
        }
        if (args[0].type == VAL_TEXT)
        {
            return makeText("text");
        }
        return makeText("null");
    }

    if (name == "abs" || name == "floor" || name == "ceil" || name == "round" || name == "sqrt")
    {
        if (argCount != 1 || args[0].type != VAL_NUMBER)
        {
            fail(error, line, name + "() expects one number.");
            return makeNull();
        }
        double value = args[0].number;
        int whole = (int)value;
        if (name == "abs")
        {
            return makeNumber(value < 0 ? -value : value);
        }
        if (name == "floor")
        {
            if (value < 0 && value != (double)whole)
            {
                whole--;
            }
            return makeNumber(whole);
        }
        if (name == "ceil")
        {
            if (value > 0 && value != (double)whole)
            {
                whole++;
            }
            return makeNumber(whole);
        }
        if (name == "round")
        {
            if (value >= 0)
            {
                return makeNumber((int)(value + 0.5));
            }
            return makeNumber((int)(value - 0.5));
        }
        bool ok = true;
        double answer = manualSqrt(value, &ok);
        if (!ok)
        {
            fail(error, line, "sqrt() cannot take a negative number.");
            return makeNull();
        }
        return makeNumber(answer);
    }

    if (name == "min" || name == "max")
    {
        if (argCount != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER)
        {
            fail(error, line, name + "() expects two numbers.");
            return makeNull();
        }
        if (name == "min")
        {
            return makeNumber(args[0].number < args[1].number ? args[0].number : args[1].number);
        }
        return makeNumber(args[0].number > args[1].number ? args[0].number : args[1].number);
    }

    if (name == "clamp")
    {
        if (argCount != 3 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER || args[2].type != VAL_NUMBER)
        {
            fail(error, line, "clamp() expects value, low, high.");
            return makeNull();
        }
        if (args[1].number > args[2].number)
        {
            fail(error, line, "clamp() low value cannot be greater than high value.");
            return makeNull();
        }
        double value = args[0].number;
        if (value < args[1].number)
        {
            value = args[1].number;
        }
        if (value > args[2].number)
        {
            value = args[2].number;
        }
        return makeNumber(value);
    }

    if (name == "pow")
    {
        if (argCount != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER || !isIntegerValue(args[1].number))
        {
            fail(error, line, "pow() expects a number and a whole-number exponent.");
            return makeNull();
        }
        bool ok = true;
        double result = manualPower(args[0].number, (int)args[1].number, &ok);
        if (!ok)
        {
            fail(error, line, "pow() cannot divide by zero.");
            return makeNull();
        }
        return makeNumber(result);
    }

    if (name == "linear_y")
    {
        if (argCount != 3 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER || args[2].type != VAL_NUMBER)
        {
            fail(error, line, "linear_y() expects slope, x, intercept.");
            return makeNull();
        }
        return makeNumber(args[0].number * args[1].number + args[2].number);
    }

    if (name == "solve_linear")
    {
        if (argCount != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER)
        {
            fail(error, line, "solve_linear() expects a and b for ax + b = 0.");
            return makeNull();
        }
        if (args[0].number == 0)
        {
            if (args[1].number == 0)
            {
                return makeText("infinite solutions");
            }
            return makeText("no solution");
        }
        return makeText("x = " + numberToString(-args[1].number / args[0].number));
    }

    if (name == "quadratic_roots")
    {
        if (argCount != 3 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER || args[2].type != VAL_NUMBER)
        {
            fail(error, line, "quadratic_roots() expects a, b, c for ax^2 + bx + c = 0.");
            return makeNull();
        }
        double a = args[0].number;
        double b = args[1].number;
        double c = args[2].number;
        if (a == 0)
        {
            Value linearArgs[2];
            linearArgs[0] = makeNumber(b);
            linearArgs[1] = makeNumber(c);
            return parserCallBuiltIn(parser, "solve_linear", linearArgs, 2);
        }
        double discriminant = b * b - 4 * a * c;
        if (discriminant < 0)
        {
            return makeText("no real roots");
        }
        bool ok = true;
        double root = manualSqrt(discriminant, &ok);
        if (!ok)
        {
            return makeText("no real roots");
        }
        double x1 = (-b + root) / (2 * a);
        double x2 = (-b - root) / (2 * a);
        if (x1 == x2)
        {
            return makeText("x = " + numberToString(x1));
        }
        return makeText("x1 = " + numberToString(x1) + ", x2 = " + numberToString(x2));
    }

    if (name == "upper" || name == "lower" || name == "strip")
    {
        if (argCount != 1)
        {
            fail(error, line, name + "() expects one argument.");
            return makeNull();
        }
        string text = valueToString(args[0]);
        if (name == "strip")
        {
            return makeText(trim(text));
        }
        for (int i = 0; i < (int)text.length(); i++)
        {
            text[i] = name == "upper" ? charToUpper(text[i]) : charToLower(text[i]);
        }
        return makeText(text);
    }

    if (name == "charat" || name == "ord")
    {
        if (argCount != 2 && name == "charat")
        {
            fail(error, line, "charat() expects text and index.");
            return makeNull();
        }
        if (argCount != 1 && name == "ord")
        {
            fail(error, line, "ord() expects one character.");
            return makeNull();
        }
        string text = valueToString(args[0]);
        if (name == "ord")
        {
            if ((int)text.length() != 1)
            {
                fail(error, line, "ord() expects exactly one character.");
                return makeNull();
            }
            return makeNumber((unsigned char)text[0]);
        }
        if (args[1].type != VAL_NUMBER || !isIntegerValue(args[1].number))
        {
            fail(error, line, "charat() index must be a whole number.");
            return makeNull();
        }
        int index = (int)args[1].number;
        if (index < 0 || index >= (int)text.length())
        {
            fail(error, line, "charat() index out of bounds.");
            return makeNull();
        }
        return makeText(string(1, text[index]));
    }

    if (name == "chr")
    {
        if (argCount != 1 || args[0].type != VAL_NUMBER || !isIntegerValue(args[0].number))
        {
            fail(error, line, "chr() expects one whole number.");
            return makeNull();
        }
        int code = (int)args[0].number;
        if (code < 0 || code > 127)
        {
            fail(error, line, "chr() only supports ASCII codes 0 through 127.");
            return makeNull();
        }
        return makeText(string(1, (char)code));
    }

    if (name == "substr")
    {
        if (argCount != 3 || args[1].type != VAL_NUMBER || args[2].type != VAL_NUMBER ||
            !isIntegerValue(args[1].number) || !isIntegerValue(args[2].number))
        {
            fail(error, line, "substr() expects text, start, length.");
            return makeNull();
        }
        string text = valueToString(args[0]);
        int start = (int)args[1].number;
        int length = (int)args[2].number;
        if (start < 0 || length < 0 || start > (int)text.length())
        {
            fail(error, line, "substr() start and length are out of bounds.");
            return makeNull();
        }
        if (start + length > (int)text.length())
        {
            length = (int)text.length() - start;
        }
        return makeText(text.substr(start, length));
    }

    if (name == "find" || name == "contains" || name == "startswith" || name == "endswith" || name == "count")
    {
        if (argCount != 2)
        {
            fail(error, line, name + "() expects two arguments.");
            return makeNull();
        }
        string text = valueToString(args[0]);
        string part = valueToString(args[1]);
        int found = (int)text.find(part);
        if (name == "find")
        {
            return makeNumber(found < 0 ? -1 : found);
        }
        if (name == "contains")
        {
            return makeNumber(found < 0 ? 0 : 1);
        }
        if (name == "startswith")
        {
            if ((int)part.length() > (int)text.length())
            {
                return makeNumber(0);
            }
            return makeNumber(text.substr(0, part.length()) == part ? 1 : 0);
        }
        if (name == "endswith")
        {
            if ((int)part.length() > (int)text.length())
            {
                return makeNumber(0);
            }
            return makeNumber(text.substr(text.length() - part.length()) == part ? 1 : 0);
        }
        if (part == "")
        {
            fail(error, line, "count() cannot count empty text.");
            return makeNull();
        }
        int total = 0;
        int at = 0;
        while (at <= (int)text.length() - (int)part.length())
        {
            if (text.substr(at, part.length()) == part)
            {
                total++;
                at += (int)part.length();
            }
            else
            {
                at++;
            }
        }
        return makeNumber(total);
    }

    if (name == "replace")
    {
        if (argCount != 3)
        {
            fail(error, line, "replace() expects text, old, new.");
            return makeNull();
        }
        string text = valueToString(args[0]);
        string oldText = valueToString(args[1]);
        string newText = valueToString(args[2]);
        if (oldText == "")
        {
            fail(error, line, "replace() old text cannot be empty.");
            return makeNull();
        }
        string result = "";
        int i = 0;
        while (i < (int)text.length())
        {
            if (i <= (int)text.length() - (int)oldText.length() && text.substr(i, oldText.length()) == oldText)
            {
                result += newText;
                i += (int)oldText.length();
            }
            else
            {
                result += text[i];
                i++;
            }
        }
        return makeText(result);
    }

    if (name == "repeattext")
    {
        if (argCount != 2 || args[1].type != VAL_NUMBER || !isIntegerValue(args[1].number))
        {
            fail(error, line, "repeattext() expects text and whole-number count.");
            return makeNull();
        }
        int count = (int)args[1].number;
        if (count < 0 || count > 10000)
        {
            fail(error, line, "repeattext() count must be between 0 and 10000.");
            return makeNull();
        }
        string result = "";
        string text = valueToString(args[0]);
        for (int i = 0; i < count; i++)
        {
            result += text;
        }
        return makeText(result);
    }

    if (name == "seed")
    {
        if (argCount != 1 || args[0].type != VAL_NUMBER || !isIntegerValue(args[0].number))
        {
            fail(error, line, "seed() expects one whole number.");
            return makeNull();
        }
        interpreter->randomSeed = (int)args[0].number;
        if (interpreter->randomSeed < 0)
        {
            interpreter->randomSeed = -interpreter->randomSeed;
        }
        return makeNumber(interpreter->randomSeed);
    }

    if (name == "rand" || name == "randint")
    {
        long nextSeed = (long)interpreter->randomSeed * 1103515245L + 12345L;
        interpreter->randomSeed = (int)(nextSeed & 2147483647L);
        if (name == "rand")
        {
            if (argCount != 0)
            {
                fail(error, line, "rand() expects no arguments.");
                return makeNull();
            }
            return makeNumber((interpreter->randomSeed % 1000000) / 1000000.0);
        }
        if (argCount != 2 || args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER ||
            !isIntegerValue(args[0].number) || !isIntegerValue(args[1].number))
        {
            fail(error, line, "randint() expects low and high whole numbers.");
            return makeNull();
        }
        int low = (int)args[0].number;
        int high = (int)args[1].number;
        if (low > high)
        {
            fail(error, line, "randint() low value cannot be greater than high value.");
            return makeNull();
        }
        int span = high - low + 1;
        return makeNumber(low + (interpreter->randomSeed % span));
    }

    if (name == "fileexists")
    {
        if (argCount != 1)
        {
            fail(error, line, "fileexists() expects one path argument.");
            return makeNull();
        }
        ifstream file(valueToString(args[0]).c_str());
        return makeNumber(file ? 1 : 0);
    }

    if (name == "fileread" || name == "filesize" || name == "linecount")
    {
        if (argCount != 1)
        {
            fail(error, line, name + "() expects one path argument.");
            return makeNull();
        }
        string path = valueToString(args[0]);
        ifstream file(path.c_str());
        if (!file)
        {
            fail(error, line, "Cannot read file '" + path + "'.");
            return makeNull();
        }
        string all = "";
        string each;
        bool first = true;
        int lines = 0;
        while (getline(file, each))
        {
            lines++;
            if (!first)
            {
                all += "\n";
            }
            first = false;
            all += each;
        }
        if (name == "fileread")
        {
            return makeText(all);
        }
        if (name == "linecount")
        {
            return makeNumber(lines);
        }
        return makeNumber((double)all.length());
    }

    return interpreterCallFunction(interpreter, name, args, argCount, line);
}

Value parserParsePrimary(ExpressionParser *parser)
{
    Token *tokens = parser->tokens;
    int &pos = parser->pos;
    int &line = parser->line;
    ErrorState *error = parser->error;
    Environment *env = parser->env;
    if (parserMatchType(parser, TOK_NUMBER))
    {
        return makeNumber(tokens[pos - 1].number);
    }
    if (parserMatchType(parser, TOK_TEXT))
    {
        return makeText(tokens[pos - 1].text);
    }
    if (parserMatchType(parser, TOK_LPAREN))
    {
        Value value = parserParseExpression(parser);
        if (!parserMatchType(parser, TOK_RPAREN))
        {
            fail(error, line, "Missing closing parenthesis.");
            return makeNull();
        }
        return value;
    }
    if (parserPeek(parser).type == TOK_IDENT)
    {
        string name = parserPeek(parser).text;
        pos++;
        if (name == "true")
        {
            return makeNumber(1);
        }
        if (name == "false")
        {
            return makeNumber(0);
        }
        if (name == "null")
        {
            return makeNull();
        }
        if (parserMatchType(parser, TOK_LPAREN))
        {
            Value args[MAX_ARGS];
            int argCount = 0;
            if (!parserMatchType(parser, TOK_RPAREN))
            {
                while (!error->failed)
                {
                    if (argCount >= MAX_ARGS)
                    {
                        fail(error, line, "Too many function arguments.");
                        return makeNull();
                    }
                    args[argCount] = parserParseExpression(parser);
                    argCount++;
                    if (parserMatchType(parser, TOK_RPAREN))
                    {
                        break;
                    }
                    if (!parserMatchType(parser, TOK_COMMA))
                    {
                        fail(error, line, "Expected comma or closing parenthesis in argument list.");
                        return makeNull();
                    }
                }
            }
            return parserCallBuiltIn(parser, name, args, argCount);
        }
        Variable *var = envFindAny(env, name);
        if (var == NULL)
        {
            fail(error, line, "Unknown variable '" + name + "'.");
            return makeNull();
        }
        if (parserMatchType(parser, TOK_LBRACKET))
        {
            Value first = parserParseExpression(parser);
            if (!parserMatchType(parser, TOK_RBRACKET))
            {
                fail(error, line, "Missing closing bracket.");
                return makeNull();
            }
            if (var->kind == VAR_ARRAY)
            {
                if (first.type != VAL_NUMBER || !isIntegerValue(first.number))
                {
                    fail(error, line, "Array index must be a whole number.");
                    return makeNull();
                }
                int index = (int)first.number;
                if (index < 0 || index >= var->rows)
                {
                    fail(error, line, "Array index out of bounds.");
                    return makeNull();
                }
                return var->cells[index];
            }
            if (var->kind == VAR_MATRIX)
            {
                if (!parserMatchType(parser, TOK_LBRACKET))
                {
                    fail(error, line, "Matrix access needs two indexes.");
                    return makeNull();
                }
                Value second = parserParseExpression(parser);
                if (!parserMatchType(parser, TOK_RBRACKET))
                {
                    fail(error, line, "Missing closing bracket.");
                    return makeNull();
                }
                if (first.type != VAL_NUMBER || second.type != VAL_NUMBER ||
                    !isIntegerValue(first.number) || !isIntegerValue(second.number))
                {
                    fail(error, line, "Matrix indexes must be whole numbers.");
                    return makeNull();
                }
                int row = (int)first.number;
                int col = (int)second.number;
                if (row < 0 || row >= var->rows || col < 0 || col >= var->cols)
                {
                    fail(error, line, "Matrix index out of bounds.");
                    return makeNull();
                }
                return var->cells[row * var->cols + col];
            }
            fail(error, line, "'" + name + "' is not indexable.");
            return makeNull();
        }
        if (var->kind != VAR_SCALAR)
        {
            fail(error, line, "'" + name + "' needs an index.");
            return makeNull();
        }
        return var->scalar;
    }
    fail(error, line, "Expected a value.");
    return makeNull();
}
