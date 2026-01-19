# Wyn Language Documentation

![Version](https://img.shields.io/badge/version-1.2.2-blue.svg)
**Latest: v1.2.2**

Welcome to the official documentation for the Wyn programming language - a modern systems programming language where everything is an object.

## Documentation Navigation

### Getting Started
- [**Getting Started**](getting-started.md) - Installation, first program, and basic concepts
- [**Examples**](examples.md) - Code examples with explanations

### Language Reference
- [**Language Guide**](language-guide.md) - Complete language reference and syntax
- [**Standard Library Reference**](stdlib-reference.md) - All built-in functions and methods

### Additional Resources
- [**FAQ**](faq.md) - Frequently asked questions and troubleshooting

## Quick Links

- **GitHub Repository:** [wyn-lang/wyn](https://github.com/wyn-lang/wyn)
- **Examples Directory:** [../examples/](../examples/)
- **Issue Tracker:** [GitHub Issues](https://github.com/wyn-lang/wyn/issues)

## What is Wyn?

Wyn is a systems programming language with a unique "everything is an object" philosophy. This means you can call methods on any value:

```wyn
let result = 42.abs() + "hello".len() + 16.0.sqrt()
```

### Key Features

- **Everything is an object** - Methods on all types
- **Type aliases** - `type UserId = int`
- **Extension methods** - Add methods to existing types
- **Pattern matching** - Exhaustive enum matching
- **Generics** - `Option<T>`, `Result<T, E>`
- **Async/await** - Future-based concurrency
- **Memory safety** - Automatic Reference Counting (ARC)

## Version Information

**Current Version:** 1.2.2  
**Status:** Production Ready  
**License:** See [LICENSE](../LICENSE)

---

*This documentation is for Wyn v1.2.2. For older versions, see the git history.*