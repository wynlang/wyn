# Wyn Memory Model Specification

This document defines the memory management model of the Wyn programming language, including stack vs heap allocation, Automatic Reference Counting (ARC), and string memory management.

For type definitions and layout information, see [Type System Specification](type-system.md).
For the formal grammar, see [Grammar Specification](grammar.md).

## Memory Layout Overview

Wyn uses a hybrid memory management approach combining:
- **Stack allocation** for value types and local variables
- **Heap allocation** for reference types and dynamic data
- **Automatic Reference Counting (ARC)** for memory safety
- **Specialized string management** for efficient string operations

## Stack vs Heap Allocation

### Stack Allocation

#### Value Types on Stack
All primitive types and small structs are allocated on the stack:

```wyn
fn example() {
    let x: int = 42;           // Stack: 8 bytes
    let y: float = 3.14;       // Stack: 8 bytes
    let flag: bool = true;     // Stack: 1 byte
    
    struct Point { x: int, y: int }
    let p = Point { x: 10, y: 20 }; // Stack: 16 bytes
}
// All variables automatically deallocated when function returns
```

#### Function Call Stack
Function parameters and local variables are stack-allocated:

```wyn
fn calculate(a: int, b: int) -> int {  // Parameters on stack
    let result = a + b;                // Local variable on stack
    return result;                     // Return value copied
}
```

#### Stack Frame Layout
```
+------------------+  <- Stack Pointer (SP)
| Local Variables  |
+------------------+
| Function Params  |
+------------------+
| Return Address   |
+------------------+
| Previous Frame   |
+------------------+  <- Frame Pointer (FP)
```

### Heap Allocation

#### Reference Types on Heap
Arrays, strings, and large data structures are heap-allocated:

```wyn
fn heap_examples() {
    let arr = [1, 2, 3, 4, 5];        // Array data on heap
    let text = "Hello, World!";       // String data on heap
    let map = {"key": "value"};       // HashMap on heap
    
    struct LargeStruct {
        data: [int; 1000]  // Large array forces heap allocation
    }
    let large = LargeStruct { data: [0; 1000] }; // Heap allocated
}
```

#### Heap Object Layout
```
Heap Object:
+------------------+
| Reference Count  | (8 bytes)
+------------------+
| Type Info        | (8 bytes)
+------------------+
| Object Data      | (variable size)
+------------------+
```

## Automatic Reference Counting (ARC)

### ARC Fundamentals

Wyn uses ARC to automatically manage heap-allocated memory:

```wyn
fn arc_example() {
    let arr1 = [1, 2, 3];    // RC = 1
    let arr2 = arr1;         // RC = 2 (shared reference)
    let arr3 = arr1;         // RC = 3
    
    // When arr1 goes out of scope: RC = 2
    // When arr2 goes out of scope: RC = 1  
    // When arr3 goes out of scope: RC = 0, memory freed
}
```

### Reference Counting Operations

#### Increment (Retain)
```wyn
// Automatic retain on assignment
let original = [1, 2, 3];  // RC = 1
let copy = original;       // RC = 2 (retain called)
```

#### Decrement (Release)
```wyn
fn scope_example() {
    let data = [1, 2, 3];  // RC = 1
    {
        let temp = data;   // RC = 2
    }                      // RC = 1 (temp released)
}                          // RC = 0 (data released, memory freed)
```

### ARC Implementation Details

#### Reference Count Storage
Each heap object has an 8-byte reference count header:

```c
typedef struct {
    uint64_t ref_count;
    uint64_t type_id;
    uint8_t data[];
} HeapObject;
```

#### Atomic Operations
Reference counting uses atomic operations for thread safety:

```c
// Atomic increment
void arc_retain(HeapObject* obj) {
    __atomic_fetch_add(&obj->ref_count, 1, __ATOMIC_SEQ_CST);
}

// Atomic decrement with deallocation check
void arc_release(HeapObject* obj) {
    if (__atomic_fetch_sub(&obj->ref_count, 1, __ATOMIC_SEQ_CST) == 1) {
        deallocate_object(obj);
    }
}
```

### Cycle Detection and Breaking

#### Weak References
Wyn provides weak references to break reference cycles:

```wyn
struct Node {
    value: int,
    next: Option<Node>,      // Strong reference
    parent: weak Option<Node> // Weak reference (prevents cycles)
}

fn create_list() -> Node {
    let node1 = Node { value: 1, next: None, parent: weak None };
    let node2 = Node { value: 2, next: Some(node1), parent: weak None };
    node1.parent = weak Some(node2); // Weak reference prevents cycle
    return node2;
}
```

#### Cycle Collection
For complex cycles, Wyn uses a mark-and-sweep collector:

```wyn
// Cycle collection is triggered when:
// 1. Memory pressure is high
// 2. Large number of potential cycles detected
// 3. Explicit gc() call (debug builds only)
```

## String Memory Management

### String Representation

#### Small String Optimization (SSO)
Strings ≤ 23 bytes are stored inline to avoid heap allocation:

```c
typedef struct {
    union {
        struct {
            char* ptr;      // Heap pointer for long strings
            size_t len;     // String length
            size_t cap;     // Capacity
        } heap;
        struct {
            char data[23];  // Inline storage for short strings
            uint8_t len;    // Length (0-23)
        } inline;
    };
    uint8_t tag;           // 0 = inline, 1 = heap
} WynString;
```

#### String Examples
```wyn
let short = "Hello";           // SSO: stored inline (5 bytes)
let medium = "Hello, World!";  // SSO: stored inline (13 bytes)
let long = "This is a very long string that exceeds the SSO limit"; // Heap allocated
```

### String Interning

#### Compile-time String Interning
String literals are interned at compile time:

```wyn
let s1 = "hello";
let s2 = "hello";
// s1 and s2 point to the same interned string
// Reference count shared between all uses
```

#### Runtime String Interning
Frequently used strings can be interned at runtime:

```wyn
fn intern_example() {
    let s1 = "dynamic".intern();  // Intern at runtime
    let s2 = "dynamic".intern();  // Returns same interned string
    // s1 and s2 share the same memory
}
```

### String Concatenation

#### Copy-on-Write (COW)
String concatenation uses COW semantics:

```wyn
let base = "Hello";
let result = base + ", World!";  // Creates new string, base unchanged

// Efficient in-place concatenation for mutable strings
var mut_str = "Hello".to_mutable();
mut_str += ", World!";  // In-place if capacity allows
```

#### StringBuilder for Multiple Concatenations
```wyn
fn build_string() -> string {
    let builder = StringBuilder::new();
    builder.append("Hello");
    builder.append(", ");
    builder.append("World!");
    return builder.to_string();  // Single allocation
}
```

## Memory Safety Guarantees

### Use-After-Free Prevention
ARC prevents use-after-free by keeping objects alive while referenced:

```wyn
fn safe_reference() -> [int] {
    let data = [1, 2, 3];
    return data;  // Safe: reference count prevents deallocation
}

let result = safe_reference();  // Data still valid
```

### Double-Free Prevention
ARC prevents double-free through reference counting:

```wyn
fn no_double_free() {
    let data = [1, 2, 3];  // RC = 1
    let ref1 = data;       // RC = 2
    let ref2 = data;       // RC = 3
    // Each reference decrements RC when dropped
    // Memory freed only when RC reaches 0
}
```

### Memory Leak Prevention
Weak references and cycle collection prevent most memory leaks:

```wyn
// Potential cycle
struct Node {
    children: [Node],
    parent: weak Option<Node>  // Weak reference breaks cycle
}
```

## Performance Characteristics

### Stack Allocation Performance
- **Allocation**: O(1) - just move stack pointer
- **Deallocation**: O(1) - automatic on scope exit
- **Cache friendly**: Sequential memory layout

### Heap Allocation Performance
- **Allocation**: O(1) amortized with memory pools
- **Deallocation**: O(1) with ARC
- **Reference operations**: O(1) atomic operations

### String Performance
- **Short strings**: No allocation overhead (SSO)
- **Interned strings**: O(1) equality comparison
- **Concatenation**: O(n) but optimized with capacity planning

## Memory Debugging and Profiling

### Debug Mode Features
```wyn
// Compile with --debug-memory for additional checks
fn debug_example() {
    let data = [1, 2, 3];
    // Debug mode tracks:
    // - Allocation stack traces
    // - Reference count history
    // - Potential cycle detection
}
```

### Memory Profiling
```wyn
// Built-in memory profiler
fn profile_memory() {
    memory::start_profiling();
    
    // ... application code ...
    
    let stats = memory::get_stats();
    print("Heap usage: " + stats.heap_bytes.to_string());
    print("Peak usage: " + stats.peak_bytes.to_string());
    print("Allocations: " + stats.allocation_count.to_string());
}
```

## Examples

### Complex Data Structure with ARC
```wyn
struct Graph {
    nodes: [Node]
}

struct Node {
    id: int,
    value: string,
    edges: [weak Node]  // Weak references prevent cycles
}

fn create_graph() -> Graph {
    let node1 = Node { id: 1, value: "A", edges: [] };
    let node2 = Node { id: 2, value: "B", edges: [] };
    
    // Create bidirectional edges with weak references
    node1.edges.push(weak node2);
    node2.edges.push(weak node1);
    
    return Graph { nodes: [node1, node2] };
}
```

### Efficient String Processing
```wyn
fn process_large_text(input: string) -> string {
    let builder = StringBuilder::with_capacity(input.len() * 2);
    
    for line in input.lines() {
        if !line.is_empty() {
            builder.append(line.trim());
            builder.append("\n");
        }
    }
    
    return builder.to_string();
}
```

### Memory-Efficient Array Operations
```wyn
fn filter_and_map(data: [int]) -> [string] {
    // Use iterator to avoid intermediate allocations
    return data
        .iter()
        .filter(|x| x % 2 == 0)
        .map(|x| x.to_string())
        .collect();
}
```