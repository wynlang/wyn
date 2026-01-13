# Wyn Best Practices Guide

This guide outlines recommended practices for writing clean, efficient, and maintainable Wyn code. Following these conventions will help you write better code and collaborate effectively with other Wyn developers.

## Table of Contents

1. [Code Style and Formatting](#code-style-and-formatting)
2. [Naming Conventions](#naming-conventions)
3. [Function Design](#function-design)
4. [Error Handling](#error-handling)
5. [Memory Management](#memory-management)
6. [Performance Guidelines](#performance-guidelines)
7. [Testing](#testing)
8. [Documentation](#documentation)
9. [Project Structure](#project-structure)
10. [Security Considerations](#security-considerations)

## Code Style and Formatting

### Indentation and Spacing

Use 4 spaces for indentation (not tabs):

```wyn
// Good
fn calculate_area(width: int, height: int) -> int {
    var area = width * height
    return area
}

// Bad
fn calculate_area(width: int, height: int) -> int {
	var area = width * height
	return area
}
```

### Line Length

Keep lines under 100 characters when possible:

```wyn
// Good
fn create_user(name: string, email: string, age: int) -> Result<User, string> {
    if name.is_empty() {
        return Err("Name cannot be empty")
    }
    return Ok(User { name: name, email: email, age: age })
}

// Acceptable for long function signatures
fn process_complex_data(
    input_data: Vec<ComplexType>,
    configuration: ProcessingConfig,
    callback: fn(Result<ProcessedData, Error>) -> void
) -> Result<void, ProcessingError> {
    // Implementation
}
```

### Braces and Brackets

Always use braces for control structures, even single statements:

```wyn
// Good
if condition {
    do_something()
}

// Bad
if condition
    do_something()

// Bad
if condition { do_something() }
```

### Spacing Around Operators

Use spaces around binary operators:

```wyn
// Good
var result = a + b * c
var is_valid = age >= 18 && has_permission

// Bad
var result = a+b*c
var is_valid = age>=18&&has_permission
```

## Naming Conventions

### Variables and Functions

Use `snake_case` for variables and functions:

```wyn
// Good
var user_count = 0
var max_retry_attempts = 3

fn calculate_total_price(items: Vec<Item>) -> float {
    // Implementation
}

// Bad
var userCount = 0
var MaxRetryAttempts = 3

fn calculateTotalPrice(items: Vec<Item>) -> float {
    // Implementation
}
```

### Types and Structs

Use `PascalCase` for types, structs, and enums:

```wyn
// Good
struct UserAccount {
    username: string,
    email: string,
    created_at: DateTime
}

enum OrderStatus {
    Pending,
    Processing,
    Shipped,
    Delivered
}

// Bad
struct user_account {
    username: string,
    email: string,
    created_at: DateTime
}

enum order_status {
    pending,
    processing,
    shipped,
    delivered
}
```

### Constants

Use `SCREAMING_SNAKE_CASE` for constants:

```wyn
// Good
const MAX_CONNECTIONS: int = 100
const DEFAULT_TIMEOUT: float = 30.0
const API_BASE_URL: string = "https://api.example.com"

// Bad
const maxConnections: int = 100
const default_timeout: float = 30.0
```

### Private vs Public

Use clear naming to indicate visibility:

```wyn
// Good - clear public interface
pub struct Database {
    connection_pool: ConnectionPool  // Private by default
}

impl Database {
    pub fn new(config: DatabaseConfig) -> Database {
        // Public constructor
    }
    
    pub fn execute_query(self, query: string) -> Result<QueryResult, DatabaseError> {
        // Public method
    }
    
    fn validate_connection(self) -> bool {
        // Private helper method
    }
}
```

## Function Design

### Keep Functions Small and Focused

Functions should do one thing well:

```wyn
// Good - single responsibility
fn validate_email(email: string) -> bool {
    return email.contains("@") && email.contains(".")
}

fn send_welcome_email(user: User) -> Result<void, EmailError> {
    if !validate_email(user.email) {
        return Err(EmailError::InvalidEmail)
    }
    
    var template = load_email_template("welcome")
    var content = template.render(user)
    return email_service.send(user.email, content)
}

// Bad - doing too many things
fn process_user_registration(user_data: UserData) -> Result<User, string> {
    // Validation
    if user_data.email.is_empty() || !user_data.email.contains("@") {
        return Err("Invalid email")
    }
    
    // Database operations
    var existing_user = database.find_user_by_email(user_data.email)
    if existing_user.is_some() {
        return Err("User already exists")
    }
    
    // Password hashing
    var hashed_password = hash_password(user_data.password)
    
    // User creation
    var user = User {
        email: user_data.email,
        password: hashed_password,
        created_at: DateTime::now()
    }
    
    // Email sending
    send_welcome_email(user)
    
    return Ok(user)
}
```

### Use Descriptive Parameter Names

Make function signatures self-documenting:

```wyn
// Good
fn transfer_money(
    from_account: AccountId,
    to_account: AccountId,
    amount: Money,
    description: string
) -> Result<Transaction, TransferError> {
    // Implementation
}

// Bad
fn transfer(from: int, to: int, amt: float, desc: string) -> Result<Transaction, string> {
    // Implementation
}
```

### Prefer Immutability

Use immutable parameters when possible:

```wyn
// Good
fn calculate_discount(price: Money, discount_rate: float) -> Money {
    return price * (1.0 - discount_rate)
}

// Only use mut when necessary
fn apply_discount(mut order: Order, discount_rate: float) {
    order.total = calculate_discount(order.total, discount_rate)
}
```

## Error Handling

### Use Result Types for Recoverable Errors

Prefer `Result<T, E>` over panicking:

```wyn
// Good
fn divide(a: float, b: float) -> Result<float, string> {
    if b == 0.0 {
        return Err("Division by zero")
    }
    return Ok(a / b)
}

// Bad
fn divide(a: float, b: float) -> float {
    if b == 0.0 {
        panic("Division by zero")  // Should be recoverable
    }
    return a / b
}
```

### Create Custom Error Types

Define specific error types for better error handling:

```wyn
// Good
enum DatabaseError {
    ConnectionFailed(string),
    QueryTimeout,
    InvalidQuery(string),
    RecordNotFound
}

fn find_user(id: UserId) -> Result<User, DatabaseError> {
    // Implementation
}

// Usage
match find_user(user_id) {
    Ok(user) => process_user(user),
    Err(DatabaseError::RecordNotFound) => create_default_user(),
    Err(DatabaseError::ConnectionFailed(msg)) => {
        log_error("Database connection failed: " + msg)
        return Err("Service temporarily unavailable")
    },
    Err(error) => {
        log_error("Database error: " + error)
        return Err("Internal server error")
    }
}
```

### Use the ? Operator for Error Propagation

Simplify error handling with the `?` operator:

```wyn
// Good
fn process_file(path: string) -> Result<ProcessedData, FileError> {
    var content = read_file(path)?
    var parsed = parse_content(content)?
    var processed = transform_data(parsed)?
    return Ok(processed)
}

// Less preferred
fn process_file(path: string) -> Result<ProcessedData, FileError> {
    var content = match read_file(path) {
        Ok(c) => c,
        Err(e) => return Err(e)
    }
    
    var parsed = match parse_content(content) {
        Ok(p) => p,
        Err(e) => return Err(e)
    }
    
    var processed = match transform_data(parsed) {
        Ok(p) => p,
        Err(e) => return Err(e)
    }
    
    return Ok(processed)
}
```

## Memory Management

### Prefer Owned Values

Use owned values when possible for simplicity:

```wyn
// Good - simple and clear
fn process_names(names: Vec<string>) -> Vec<string> {
    var processed = Vec::new()
    for name in names {
        processed.push(name.to_uppercase())
    }
    return processed
}

// Use references only when necessary for performance
fn count_long_names(names: &Vec<string>) -> int {
    var count = 0
    for name in names {
        if name.len() > 10 {
            count += 1
        }
    }
    return count
}
```

### Avoid Circular References

Use weak references to break cycles:

```wyn
// Good
struct Parent {
    children: Vec<Arc<Child>>
}

struct Child {
    parent: Weak<Parent>  // Weak reference prevents cycles
}

// Bad - creates memory leaks
struct Parent {
    children: Vec<Arc<Child>>
}

struct Child {
    parent: Arc<Parent>  // Strong reference creates cycle
}
```

### Use Arc Sparingly

Only use `Arc` when you need shared ownership:

```wyn
// Good - Arc only when needed for sharing
fn share_config(config: Arc<Config>) -> (Worker, Worker) {
    var worker1 = Worker::new(Arc::clone(&config))
    var worker2 = Worker::new(Arc::clone(&config))
    return (worker1, worker2)
}

// Good - owned values when sharing isn't needed
fn process_data(data: Vec<Item>) -> ProcessedData {
    // Process data without sharing
}
```

## Performance Guidelines

### Avoid Unnecessary Allocations

Reuse collections when possible:

```wyn
// Good - reuse buffer
fn process_multiple_files(files: Vec<string>) -> Result<Vec<ProcessedData>, Error> {
    var results = Vec::with_capacity(files.len())  // Pre-allocate
    var buffer = String::new()  // Reuse buffer
    
    for file in files {
        buffer.clear()
        read_file_into_buffer(file, &mut buffer)?
        var processed = process_buffer(&buffer)?
        results.push(processed)
    }
    
    return Ok(results)
}

// Bad - creates new allocations each iteration
fn process_multiple_files(files: Vec<string>) -> Result<Vec<ProcessedData>, Error> {
    var results = Vec::new()
    
    for file in files {
        var content = read_file(file)?  // New allocation each time
        var processed = process_content(content)?
        results.push(processed)
    }
    
    return Ok(results)
}
```

### Use Appropriate Data Structures

Choose the right collection for your use case:

```wyn
// Good - HashMap for key-value lookups
var user_cache: HashMap<UserId, User> = HashMap::new()

// Good - Vec for ordered data
var processing_queue: Vec<Task> = Vec::new()

// Good - HashSet for unique values
var seen_ids: HashSet<ItemId> = HashSet::new()
```

### Profile Before Optimizing

Write clear code first, optimize later:

```wyn
// Good - clear and readable
fn find_user_by_email(users: &Vec<User>, email: string) -> Option<User> {
    for user in users {
        if user.email == email {
            return Some(user.clone())
        }
    }
    return None
}

// Optimize only if profiling shows this is a bottleneck
fn find_user_by_email_optimized(user_index: &HashMap<string, User>, email: string) -> Option<User> {
    return user_index.get(email).cloned()
}
```

## Testing

### Write Unit Tests

Test individual functions in isolation:

```wyn
#[test]
fn test_calculate_discount() {
    var original_price = Money::from_dollars(100)
    var discount_rate = 0.2
    
    var discounted = calculate_discount(original_price, discount_rate)
    
    assert_eq(discounted, Money::from_dollars(80))
}

#[test]
fn test_divide_by_zero() {
    var result = divide(10.0, 0.0)
    
    match result {
        Ok(_) => panic("Expected error for division by zero"),
        Err(msg) => assert_eq(msg, "Division by zero")
    }
}
```

### Use Descriptive Test Names

Make test names explain what they're testing:

```wyn
// Good
#[test]
fn test_user_creation_with_valid_email_succeeds() {
    // Test implementation
}

#[test]
fn test_user_creation_with_invalid_email_returns_error() {
    // Test implementation
}

// Bad
#[test]
fn test_user_creation() {
    // Test implementation
}

#[test]
fn test_user_error() {
    // Test implementation
}
```

### Test Edge Cases

Include tests for boundary conditions:

```wyn
#[test]
fn test_empty_input() {
    var result = process_items(Vec::new())
    assert(result.is_empty())
}

#[test]
fn test_single_item() {
    var items = vec![Item::new("test")]
    var result = process_items(items)
    assert_eq(result.len(), 1)
}

#[test]
fn test_maximum_capacity() {
    var items = create_items(MAX_CAPACITY)
    var result = process_items(items)
    assert_eq(result.len(), MAX_CAPACITY)
}
```

## Documentation

### Write Clear Comments

Explain why, not what:

```wyn
// Good - explains the reasoning
fn retry_with_backoff<F>(mut operation: F, max_attempts: int) -> Result<T, Error>
where F: FnMut() -> Result<T, Error> {
    // Use exponential backoff to avoid overwhelming the server
    // when it's already under stress
    var delay = 1000  // Start with 1 second
    
    for var attempt = 1; attempt <= max_attempts; attempt += 1 {
        match operation() {
            Ok(result) => return Ok(result),
            Err(error) => {
                if attempt == max_attempts {
                    return Err(error)
                }
                
                Thread::sleep(delay)
                delay *= 2  // Double the delay each time
            }
        }
    }
}

// Bad - explains what the code already shows
fn retry_with_backoff<F>(mut operation: F, max_attempts: int) -> Result<T, Error>
where F: FnMut() -> Result<T, Error> {
    // Set delay to 1000
    var delay = 1000
    
    // Loop from 1 to max_attempts
    for var attempt = 1; attempt <= max_attempts; attempt += 1 {
        // Call operation
        match operation() {
            // If ok, return result
            Ok(result) => return Ok(result),
            // If error
            Err(error) => {
                // If last attempt, return error
                if attempt == max_attempts {
                    return Err(error)
                }
                
                // Sleep for delay milliseconds
                Thread::sleep(delay)
                // Multiply delay by 2
                delay *= 2
            }
        }
    }
}
```

### Document Public APIs

Provide clear documentation for public functions:

```wyn
/// Calculates the compound interest for an investment.
///
/// # Arguments
/// * `principal` - The initial investment amount
/// * `rate` - The annual interest rate (as a decimal, e.g., 0.05 for 5%)
/// * `time` - The number of years
/// * `compounds_per_year` - How many times per year interest is compounded
///
/// # Returns
/// The final amount after compound interest is applied
///
/// # Example
/// ```
/// var final_amount = compound_interest(1000.0, 0.05, 10, 12)
/// // Returns approximately 1643.62 for $1000 at 5% for 10 years, compounded monthly
/// ```
pub fn compound_interest(
    principal: float,
    rate: float,
    time: float,
    compounds_per_year: int
) -> float {
    var base = 1.0 + (rate / compounds_per_year)
    var exponent = compounds_per_year * time
    return principal * pow(base, exponent)
}
```

## Project Structure

### Organize Code into Modules

Structure your project logically:

```
src/
├── main.wyn           # Application entry point
├── lib.wyn            # Library root (if creating a library)
├── config/
│   ├── mod.wyn        # Module declaration
│   ├── database.wyn   # Database configuration
│   └── server.wyn     # Server configuration
├── models/
│   ├── mod.wyn
│   ├── user.wyn       # User model
│   └── order.wyn      # Order model
├── services/
│   ├── mod.wyn
│   ├── user_service.wyn
│   └── order_service.wyn
└── utils/
    ├── mod.wyn
    ├── validation.wyn
    └── formatting.wyn
```

### Use Clear Module Boundaries

Keep related functionality together:

```wyn
// models/user.wyn
pub struct User {
    pub id: UserId,
    pub email: string,
    pub created_at: DateTime
}

impl User {
    pub fn new(email: string) -> User {
        // Constructor logic
    }
    
    pub fn is_valid_email(email: string) -> bool {
        // Validation logic
    }
}

// services/user_service.wyn
use crate::models::User

pub struct UserService {
    database: Database
}

impl UserService {
    pub fn create_user(mut self, email: string) -> Result<User, UserError> {
        // Service logic
    }
    
    pub fn find_user(self, id: UserId) -> Result<Option<User>, UserError> {
        // Service logic
    }
}
```

## Security Considerations

### Validate All Inputs

Never trust external input:

```wyn
// Good
fn create_user(user_data: UserRegistration) -> Result<User, ValidationError> {
    // Validate email format
    if !is_valid_email(&user_data.email) {
        return Err(ValidationError::InvalidEmail)
    }
    
    // Validate password strength
    if user_data.password.len() < 8 {
        return Err(ValidationError::WeakPassword)
    }
    
    // Sanitize name input
    var sanitized_name = sanitize_string(&user_data.name)
    
    return Ok(User {
        email: user_data.email,
        name: sanitized_name,
        password_hash: hash_password(&user_data.password)
    })
}

// Bad
fn create_user(user_data: UserRegistration) -> User {
    return User {
        email: user_data.email,  // No validation
        name: user_data.name,    // No sanitization
        password_hash: user_data.password  // Storing plain text!
    }
}
```

### Use Secure Defaults

Choose secure options by default:

```wyn
// Good
struct HttpsClient {
    verify_certificates: bool = true,  // Secure default
    timeout: Duration = Duration::from_seconds(30)
}

// Bad
struct HttpClient {
    use_https: bool = false,  // Insecure default
    verify_certificates: bool = false  // Insecure default
}
```

### Handle Sensitive Data Carefully

```wyn
// Good - clear handling of sensitive data
struct SecureString {
    data: Vec<u8>
}

impl SecureString {
    fn new(data: string) -> SecureString {
        // Implementation that securely handles the string
    }
    
    fn clear(mut self) {
        // Securely overwrite memory
        for i in 0..self.data.len() {
            self.data[i] = 0
        }
    }
}

impl Drop for SecureString {
    fn drop(mut self) {
        self.clear()  // Automatically clear on drop
    }
}
```

Following these best practices will help you write Wyn code that is readable, maintainable, secure, and performant. Remember that consistency is key—establish conventions for your project and stick to them throughout your codebase.