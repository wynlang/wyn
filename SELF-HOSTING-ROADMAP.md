# Wyn Self-Hosting Roadmap

**Goal**: Rewrite Wyn compiler in Wyn, bootstrapped from C implementation

**Strategy**: Incremental replacement with TDD, parallel testing, clean architecture

---

## Epic 1: Foundation & Lexer (Week 1)

**Goal**: Self-hosted lexer that tokenizes Wyn source code

### Task 1.1: Project Structure ✅
- [ ] Create `self-hosted/` directory
- [ ] Create `self-hosted/src/` for source
- [ ] Create `self-hosted/tests/` for tests
- [ ] Create `self-hosted/docs/` for documentation
- [ ] Set up test runner with spawn/await

### Task 1.2: Token Types & Data Structures
- [ ] Define Token struct
- [ ] Define TokenType enum
- [ ] Write tests for token creation
- [ ] Implement token equality/comparison

### Task 1.3: Lexer - Basic Tokens
- [ ] TDD: Lex integers (123, 456)
- [ ] TDD: Lex floats (3.14, 2.5)
- [ ] TDD: Lex strings ("hello", "world")
- [ ] TDD: Lex identifiers (foo, bar_baz)
- [ ] TDD: Lex keywords (fn, var, if, else)

### Task 1.4: Lexer - Operators & Symbols
- [ ] TDD: Lex operators (+, -, *, /, ==, !=, etc.)
- [ ] TDD: Lex delimiters ({, }, (, ), [, ])
- [ ] TDD: Lex punctuation (,, ;, :, ::)

### Task 1.5: Lexer - Advanced Features
- [ ] TDD: Handle whitespace and newlines
- [ ] TDD: Handle comments (// and /* */)
- [ ] TDD: Track line/column numbers
- [ ] TDD: Error reporting for invalid tokens

### Task 1.6: Integration & Validation
- [ ] Test lexer on real Wyn files
- [ ] Compare output with C lexer
- [ ] Performance benchmarks
- [ ] Full regression suite

**Deliverable**: `lexer.wyn` that tokenizes any Wyn source file

---

## Epic 2: Parser (Week 2)

**Goal**: Self-hosted parser that builds AST from tokens

### Task 2.1: AST Data Structures
- [ ] Define Expr types (Binary, Unary, Literal, etc.)
- [ ] Define Stmt types (Fn, Var, If, While, etc.)
- [ ] Define Pattern types
- [ ] Write tests for AST node creation

### Task 2.2: Expression Parser
- [ ] TDD: Parse literals
- [ ] TDD: Parse binary expressions
- [ ] TDD: Parse unary expressions
- [ ] TDD: Parse function calls
- [ ] TDD: Parse array/struct literals

### Task 2.3: Statement Parser
- [ ] TDD: Parse function declarations
- [ ] TDD: Parse variable declarations
- [ ] TDD: Parse if/else statements
- [ ] TDD: Parse while/for loops
- [ ] TDD: Parse match expressions

### Task 2.4: Pattern Parser
- [ ] TDD: Parse literal patterns
- [ ] TDD: Parse or-patterns
- [ ] TDD: Parse wildcard patterns
- [ ] TDD: Parse destructuring patterns

### Task 2.5: Integration & Validation
- [ ] Test parser on real Wyn files
- [ ] Compare AST with C parser
- [ ] Error recovery tests
- [ ] Full regression suite

**Deliverable**: `parser.wyn` that builds AST from tokens

---

## Epic 3: Type Checker (Week 3)

**Goal**: Self-hosted type checker with inference

### Task 3.1: Type System
- [ ] Define Type enum (Int, Float, String, etc.)
- [ ] Define Symbol table structure
- [ ] TDD: Type equality/compatibility
- [ ] TDD: Type inference rules

### Task 3.2: Expression Type Checking
- [ ] TDD: Check literal types
- [ ] TDD: Check binary expression types
- [ ] TDD: Check function call types
- [ ] TDD: Check method call types

### Task 3.3: Statement Type Checking
- [ ] TDD: Check function declarations
- [ ] TDD: Check variable declarations
- [ ] TDD: Check control flow types
- [ ] TDD: Check match exhaustiveness

### Task 3.4: Advanced Features
- [ ] TDD: Enum type checking
- [ ] TDD: Struct type checking
- [ ] TDD: Pattern type checking
- [ ] TDD: Generic type checking (if needed)

### Task 3.5: Integration & Validation
- [ ] Test type checker on real Wyn files
- [ ] Compare errors with C checker
- [ ] Error message quality tests
- [ ] Full regression suite

**Deliverable**: `checker.wyn` that validates types

---

## Epic 4: LLVM Code Generator (Week 4)

**Goal**: Self-hosted LLVM IR generator

### Task 4.1: LLVM-C FFI Bindings
- [ ] Define LLVM-C function signatures
- [ ] Create Wyn wrappers for LLVM-C API
- [ ] TDD: Basic LLVM operations
- [ ] TDD: Module/function creation

### Task 4.2: Expression Codegen
- [ ] TDD: Generate literals
- [ ] TDD: Generate binary expressions
- [ ] TDD: Generate function calls
- [ ] TDD: Generate array/struct operations

### Task 4.3: Statement Codegen
- [ ] TDD: Generate function declarations
- [ ] TDD: Generate variable declarations
- [ ] TDD: Generate control flow
- [ ] TDD: Generate match expressions

### Task 4.4: Advanced Features
- [ ] TDD: Generate enum constants
- [ ] TDD: Generate pattern matching
- [ ] TDD: Generate method calls
- [ ] TDD: Optimize IR

### Task 4.5: Integration & Validation
- [ ] Test codegen on real Wyn files
- [ ] Compare IR with C codegen
- [ ] Performance benchmarks
- [ ] Full regression suite

**Deliverable**: `codegen.wyn` that generates LLVM IR

---

## Epic 5: Integration & Bootstrap (Week 5)

**Goal**: Self-hosted compiler compiles itself

### Task 5.1: Compiler Driver
- [ ] TDD: Command line argument parsing
- [ ] TDD: File I/O orchestration
- [ ] TDD: Pipeline (lex → parse → check → codegen)
- [ ] TDD: Error reporting

### Task 5.2: Build System
- [ ] Create Makefile for self-hosted compiler
- [ ] Set up bootstrap process
- [ ] Create test infrastructure
- [ ] Set up CI/CD

### Task 5.3: Bootstrap Validation
- [ ] Compile self-hosted compiler with C compiler
- [ ] Compile self-hosted compiler with itself
- [ ] Verify binary equivalence
- [ ] Performance comparison

### Task 5.4: Final Testing
- [ ] Run all test suites with spawn/await
- [ ] Full regression testing
- [ ] Performance benchmarks
- [ ] Documentation

**Deliverable**: Fully self-hosted Wyn compiler

---

## Testing Strategy

### Unit Tests
- Every function has tests
- TDD: Write test first, then implementation
- Run with spawn/await for parallelism

### Integration Tests
- Test components together
- Compare with C implementation
- Validate on real Wyn files

### Regression Tests
- All existing tests must pass
- Run in parallel with spawn/await
- Automated on every change

### Performance Tests
- Benchmark against C implementation
- Track compilation speed
- Memory usage profiling

---

## Directory Structure

```
self-hosted/
├── src/
│   ├── lexer.wyn          # Lexer implementation
│   ├── parser.wyn         # Parser implementation
│   ├── checker.wyn        # Type checker
│   ├── codegen.wyn        # LLVM codegen
│   ├── ast.wyn            # AST definitions
│   ├── types.wyn          # Type system
│   ├── token.wyn          # Token definitions
│   └── main.wyn           # Compiler driver
├── tests/
│   ├── lexer_test.wyn     # Lexer tests
│   ├── parser_test.wyn    # Parser tests
│   ├── checker_test.wyn   # Type checker tests
│   ├── codegen_test.wyn   # Codegen tests
│   ├── integration_test.wyn
│   └── test_runner.wyn    # Parallel test runner
├── docs/
│   ├── ARCHITECTURE.md    # Architecture docs
│   ├── API.md             # API documentation
│   └── BOOTSTRAP.md       # Bootstrap process
└── Makefile               # Build system
```

---

## Success Metrics

### Epic 1 (Lexer)
- [ ] 100% token coverage
- [ ] All lexer tests pass
- [ ] Matches C lexer output
- [ ] < 10ms for 1000 line file

### Epic 2 (Parser)
- [ ] 100% AST node coverage
- [ ] All parser tests pass
- [ ] Matches C parser output
- [ ] < 50ms for 1000 line file

### Epic 3 (Type Checker)
- [ ] 100% type rule coverage
- [ ] All checker tests pass
- [ ] Same errors as C checker
- [ ] < 100ms for 1000 line file

### Epic 4 (Codegen)
- [ ] 100% IR instruction coverage
- [ ] All codegen tests pass
- [ ] Same IR as C codegen
- [ ] < 200ms for 1000 line file

### Epic 5 (Bootstrap)
- [ ] Self-compilation succeeds
- [ ] Binary equivalence verified
- [ ] All tests pass
- [ ] Performance within 2x of C

---

## Timeline

- **Week 1**: Epic 1 (Lexer) - Foundation
- **Week 2**: Epic 2 (Parser) - AST building
- **Week 3**: Epic 3 (Type Checker) - Validation
- **Week 4**: Epic 4 (Codegen) - IR generation
- **Week 5**: Epic 5 (Bootstrap) - Self-hosting

**Total**: 5 weeks to full self-hosting

---

## Risk Mitigation

### Risk: Performance too slow
**Mitigation**: Profile and optimize hot paths, use efficient data structures

### Risk: LLVM-C FFI complex
**Mitigation**: Start with minimal API surface, expand as needed

### Risk: Bootstrap fails
**Mitigation**: Incremental validation, compare with C at each step

### Risk: Tests take too long
**Mitigation**: Parallel execution with spawn/await, smart test selection

---

## Current Status

- [x] C compiler complete (100%)
- [x] Runtime library complete (core modules)
- [x] All language features working
- [ ] Self-hosted compiler (0%)

**Next**: Start Epic 1, Task 1.1 - Project Structure
