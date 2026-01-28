# Wyn Language Grammar Specification

This document defines the formal grammar of the Wyn programming language using Extended Backus-Naur Form (EBNF) notation.

For detailed information about types, see [Type System Specification](type-system.md).
For memory management details, see [Memory Model Specification](memory-model.md).

## EBNF Notation

- `|` denotes alternation
- `()` denotes grouping
- `[]` denotes optional elements
- `{}` denotes zero or more repetitions
- `+` denotes one or more repetitions
- `*` denotes zero or more repetitions

## Lexical Elements

### Identifiers
```ebnf
identifier = letter { letter | digit | "_" } ;
letter = "a" | "b" | ... | "z" | "A" | "B" | ... | "Z" ;
digit = "0" | "1" | ... | "9" ;
```

### Literals
```ebnf
integer_literal = digit { digit } ;
float_literal = digit { digit } "." digit { digit } ;
string_literal = '"' { string_char } '"' ;
string_char = any_char - '"' | '\"' ;
boolean_literal = "true" | "false" ;
```

### Keywords
```ebnf
keyword = "fn" | "struct" | "enum" | "if" | "else" | "while" | "for" 
        | "return" | "let" | "var" | "mut" | "true" | "false" | "null"
        | "async" | "await" | "match" | "impl" | "trait" | "type"
        | "import" | "export" | "pub" | "priv" ;
```

## Program Structure

### Program
```ebnf
program = { item } ;

item = function_def
     | struct_def
     | enum_def
     | trait_def
     | impl_block
     | type_alias
     | import_stmt
     | export_stmt ;
```

### Function Definition
```ebnf
function_def = ["pub"] "fn" identifier ["<" type_params ">"] 
               "(" [param_list] ")" ["->" type] block ;

param_list = param { "," param } ;
param = identifier ":" type ;
type_params = identifier { "," identifier } ;
```

### Struct Definition
```ebnf
struct_def = ["pub"] "struct" identifier ["<" type_params ">"] 
             "{" [field_list] "}" ;

field_list = field { "," field } ;
field = identifier ":" type ;
```

### Enum Definition
```ebnf
enum_def = ["pub"] "enum" identifier ["<" type_params ">"] 
           "{" variant_list "}" ;

variant_list = variant { "," variant } ;
variant = identifier ["(" type_list ")"] ;
type_list = type { "," type } ;
```

## Type System

### Types
```ebnf
type = primitive_type
     | array_type
     | generic_type
     | function_type
     | tuple_type
     | optional_type ;

primitive_type = "int" | "float" | "bool" | "string" | "void" ;
array_type = "[" type "]" ;
generic_type = identifier ["<" type_args ">"] ;
type_args = type { "," type } ;
function_type = "fn" "(" [type_list] ")" ["->" type] ;
tuple_type = "(" type_list ")" ;
optional_type = type "?" ;
```

## Statements

### Statement
```ebnf
statement = expression_stmt
          | let_stmt
          | var_stmt
          | assignment_stmt
          | if_stmt
          | while_stmt
          | for_stmt
          | match_stmt
          | return_stmt
          | break_stmt
          | continue_stmt
          | block ;

expression_stmt = expression ";" ;
let_stmt = "let" identifier [":" type] "=" expression ";" ;
var_stmt = "var" identifier [":" type] "=" expression ";" ;
assignment_stmt = lvalue "=" expression ";" ;
return_stmt = "return" [expression] ";" ;
break_stmt = "break" ";" ;
continue_stmt = "continue" ";" ;
```

### Control Flow
```ebnf
if_stmt = "if" expression block ["else" (if_stmt | block)] ;
while_stmt = "while" expression block ;
for_stmt = "for" identifier "in" expression block ;

match_stmt = "match" expression "{" match_arms "}" ;
match_arms = match_arm { "," match_arm } ;
match_arm = pattern "=>" (expression | block) ;
```

### Block
```ebnf
block = "{" { statement } "}" ;
```

## Expressions

### Expression
```ebnf
expression = assignment_expr ;

assignment_expr = logical_or_expr ["=" assignment_expr] ;
logical_or_expr = logical_and_expr { "||" logical_and_expr } ;
logical_and_expr = equality_expr { "&&" equality_expr } ;
equality_expr = relational_expr { ("==" | "!=") relational_expr } ;
relational_expr = additive_expr { ("<" | ">" | "<=" | ">=") additive_expr } ;
additive_expr = multiplicative_expr { ("+" | "-") multiplicative_expr } ;
multiplicative_expr = unary_expr { ("*" | "/" | "%") unary_expr } ;
unary_expr = [("!" | "-" | "&" | "*")] postfix_expr ;
postfix_expr = primary_expr { postfix_op } ;
postfix_op = "[" expression "]"
           | "." identifier
           | "(" [arg_list] ")"
           | "?" ;
```

### Primary Expressions
```ebnf
primary_expr = identifier
             | literal
             | "(" expression ")"
             | array_literal
             | struct_literal
             | lambda_expr
             | async_expr
             | await_expr ;

literal = integer_literal
        | float_literal
        | string_literal
        | boolean_literal
        | "null" ;

array_literal = "[" [expression_list] "]" ;
expression_list = expression { "," expression } ;

struct_literal = identifier "{" [field_inits] "}" ;
field_inits = field_init { "," field_init } ;
field_init = identifier ":" expression ;

lambda_expr = "|" [param_list] "|" (expression | block) ;
async_expr = "async" block ;
await_expr = "await" expression ;
```

## Patterns

### Pattern
```ebnf
pattern = literal_pattern
        | identifier_pattern
        | wildcard_pattern
        | struct_pattern
        | enum_pattern
        | tuple_pattern ;

literal_pattern = literal ;
identifier_pattern = identifier ;
wildcard_pattern = "_" ;
struct_pattern = identifier "{" [pattern_fields] "}" ;
pattern_fields = pattern_field { "," pattern_field } ;
pattern_field = identifier ":" pattern ;
enum_pattern = identifier ["(" pattern_list ")"] ;
pattern_list = pattern { "," pattern } ;
tuple_pattern = "(" pattern_list ")" ;
```

## Examples

### Function with Generics
```wyn
fn max<T>(a: T, b: T) -> T {
    if a > b {
        return a;
    }
    return b;
}
```

### Struct with Methods
```wyn
struct Point {
    x: int,
    y: int
}

impl Point {
    fn new(x: int, y: int) -> Point {
        return Point { x: x, y: y };
    }
    
    fn distance(&self) -> float {
        return sqrt(self.x * self.x + self.y * self.y);
    }
}
```

### Enum with Pattern Matching
```wyn
enum Result<T, E> {
    Ok(T),
    Err(E)
}

fn handle_result(r: Result<int, string>) -> int {
    match r {
        Ok(value) => return value,
        Err(msg) => {
            print("Error: " + msg);
            return -1;
        }
    }
}
```

### Async Function
```wyn
async fn fetch_data(url: string) -> Result<string, string> {
    let response = await http_get(url);
    match response {
        Ok(data) => return Ok(data),
        Err(e) => return Err("Failed to fetch: " + e)
    }
}
```