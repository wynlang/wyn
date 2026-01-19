# Wyn Examples

![Version](https://img.shields.io/badge/version-1.2.2-blue.svg)
**Latest: v1.2.2**

This page provides an overview of all examples available in the [`../examples/`](../examples/) directory, with explanations of what each example demonstrates.

## Running Examples

To run any example:

```bash
cd wyn
./wyn ../examples/<example>.wyn
../examples/<example>.wyn.out
echo $?  # Check exit code
```

## Basic Examples

### [01_hello_world.wyn](../examples/01_hello_world.wyn)
**The classic first program**

```wyn
fn main() -> int {
    print_str("Hello, World!");
    return 0;
}
```

**Demonstrates:**
- Basic function definition
- String output
- Return values

**Expected output:** "Hello, World!" (exit code 0)

### [02_functions.wyn](../examples/02_functions.wyn)
**Function definitions and calls**

```wyn
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() -> int {
    let result = add(10, 15);
    return result;
}
```

**Demonstrates:**
- Function parameters
- Return types
- Function calls
- Variable assignment

**Expected output:** Exit code 25

### [hello.wyn](../examples/hello.wyn)
**Simple greeting program**

```wyn
fn main() -> int {
    print_str("Hello from Wyn!");
    return 42;
}
```

**Demonstrates:**
- Basic program structure
- String literals
- Non-zero exit codes

**Expected output:** "Hello from Wyn!" (exit code 42)

## Data Structures

### [10_structs.wyn](../examples/10_structs.wyn)
**User-defined data types**

```wyn
struct Point {
    x: int,
    y: int
}

fn main() -> int {
    let p = Point { x: 10, y: 20 };
    return p.x + p.y;
}
```

**Demonstrates:**
- Struct definitions
- Struct initialization
- Field access
- Struct usage in expressions

**Expected output:** Exit code 30

### [structs.wyn](../examples/structs.wyn)
**Rectangle area and perimeter calculation**

```wyn
struct Rectangle {
    width: int,
    height: int
}

fn area(r: Rectangle) -> int {
    return r.width * r.height;
}

fn perimeter(r: Rectangle) -> int {
    return 2 * (r.width + r.height);
}

fn main() -> int {
    let rect = Rectangle { width: 10, height: 5 };
    let a = area(rect);
    let p = perimeter(rect);
    return a + p;
}
```

**Demonstrates:**
- Struct definitions
- Functions taking struct parameters
- Multiple function calls
- Arithmetic operations

**Expected output:** Exit code 80 (area 50 + perimeter 30)

### [arrays.wyn](../examples/arrays.wyn)
**Array operations and processing**

```wyn
fn sum_array(arr: [int; 5]) -> int {
    let mut total = 0;
    let mut i = 0;
    while i < 5 {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}

fn max_array(arr: [int; 5]) -> int {
    let mut max_val = arr[0];
    let mut i = 1;
    while i < 5 {
        if arr[i] > max_val {
            max_val = arr[i];
        }
        i = i + 1;
    }
    return max_val;
}

fn main() -> int {
    let numbers = [1, 5, 3, 9, 2];
    let total = sum_array(numbers);
    let maximum = max_array(numbers);
    return total + maximum;
}
```

**Demonstrates:**
- Array literals
- Array indexing
- While loops
- Mutable variables
- Conditional statements
- Array processing algorithms

**Expected output:** Exit code 37 (sum 20 + max 9)

## Advanced Features

### [03_generics.wyn](../examples/03_generics.wyn)
**Generic functions and type parameters**

```wyn
fn max<T>(a: T, b: T) -> T {
    if a > b {
        return a;
    } else {
        return b;
    }
}

fn main() -> int {
    let int_max = max(10, 20);
    let float_max = max(3.14, 2.71);
    return int_max + float_max as int;
}
```

**Demonstrates:**
- Generic function definitions
- Type parameters
- Generic function calls
- Type inference
- Comparison operators

**Expected output:** Exit code varies based on float conversion

### [generics.wyn](../examples/generics.wyn)
**Multiple generic function calls**

```wyn
fn identity<T>(x: T) -> T {
    return x;
}

fn main() -> int {
    let a = identity(42);
    let b = identity(20);
    let c = identity(5);
    return a + b + c;
}
```

**Demonstrates:**
- Generic identity function
- Multiple generic instantiations
- Type inference with generics

**Expected output:** Exit code 67 (42 + 20 + 5)

### [11_methods.wyn](../examples/11_methods.wyn)
**Implementation blocks and methods**

```wyn
struct Point {
    x: int,
    y: int
}

impl Point {
    fn new(x: int, y: int) -> Point {
        return Point { x: x, y: y };
    }
    
    fn distance_from_origin(self) -> int {
        return self.x * self.x + self.y * self.y;
    }
    
    fn translate(self, dx: int, dy: int) -> Point {
        return Point { x: self.x + dx, y: self.y + dy };
    }
}

fn main() -> int {
    let p1 = Point::new(3, 4);
    let distance = p1.distance_from_origin();
    let p2 = p1.translate(1, 1);
    return distance + p2.x + p2.y;
}
```

**Demonstrates:**
- Implementation blocks (`impl`)
- Associated functions (constructors)
- Instance methods
- Method chaining
- `self` parameter

**Expected output:** Exit code varies based on calculation

## Asynchronous Programming

### [04_async_await.wyn](../examples/04_async_await.wyn)
**Asynchronous functions and await**

```wyn
async fn fetch_data() -> int {
    return 42;
}

async fn process_data(data: int) -> int {
    return data * 2;
}

fn main() -> int {
    let future1 = fetch_data();
    let data = await future1;
    
    let future2 = process_data(data);
    let result = await future2;
    
    return result;
}
```

**Demonstrates:**
- Async function definitions
- Future creation
- Await expressions
- Async composition

**Expected output:** Exit code 84 (42 * 2)

## Collections and Data Processing

### [05_file_io.wyn](../examples/05_file_io.wyn)
**File reading and writing**

```wyn
fn main() -> int {
    let content = "Hello from Wyn file I/O!";
    let write_result = file_write("/tmp/wyn_test.txt", content);
    
    if write_result == 1 {
        let read_content = file_read("/tmp/wyn_test.txt");
        let length = str_len(read_content);
        return length;
    } else {
        return -1;
    }
}
```

**Demonstrates:**
- File writing operations
- File reading operations
- Error handling with return codes
- String length calculation

**Expected output:** Exit code equal to content length

### [06_hashmap.wyn](../examples/06_hashmap.wyn)
**Hash map operations**

```wyn
fn main() -> int {
    let map = hashmap_new();
    
    hashmap_insert(map, "apple", 5);
    hashmap_insert(map, "banana", 3);
    hashmap_insert(map, "orange", 8);
    
    let apple_count = hashmap_get(map, "apple");
    let banana_count = hashmap_get(map, "banana");
    
    hashmap_free(map);
    
    return apple_count + banana_count;
}
```

**Demonstrates:**
- Hash map creation
- Key-value insertion
- Value retrieval
- Memory management
- Resource cleanup

**Expected output:** Exit code 8 (5 + 3)

### [08_hashset.wyn](../examples/08_hashset.wyn)
**Hash set operations**

```wyn
fn main() -> int {
    let set = hashset_new();
    
    hashset_add(set, "apple");
    hashset_add(set, "banana");
    hashset_add(set, "apple");  // Duplicate
    
    let has_apple = hashset_contains(set, "apple");
    let has_orange = hashset_contains(set, "orange");
    
    hashset_free(set);
    
    return has_apple + has_orange;
}
```

**Demonstrates:**
- Hash set creation
- Element insertion
- Duplicate handling
- Membership testing
- Memory cleanup

**Expected output:** Exit code 1 (1 + 0)

## JSON and Data Formats

### [09_json.wyn](../examples/09_json.wyn)
**JSON parsing and data extraction**

```wyn
fn main() -> int {
    let json_text = "{\"name\":\"Alice\",\"age\":30,\"score\":95.5}";
    let json = json_parse(json_text);
    
    let name = json_get_string(json, "name");
    let age = json_get_int(json, "age");
    
    json_free(json);
    
    return age + str_len(name);
}
```

**Demonstrates:**
- JSON string parsing
- Data extraction by key
- String and integer retrieval
- Resource management

**Expected output:** Exit code 35 (30 + 5)

## Algorithms and Problem Solving

### [fibonacci.wyn](../examples/fibonacci.wyn)
**Recursive Fibonacci calculation**

```wyn
fn fibonacci(n: int) -> int {
    if n <= 1 {
        return n;
    } else {
        return fibonacci(n - 1) + fibonacci(n - 2);
    }
}

fn main() -> int {
    let fib5 = fibonacci(5);
    let fib7 = fibonacci(7);
    return fib5 + fib7;
}
```

**Demonstrates:**
- Recursive functions
- Base cases and recursive cases
- Multiple function calls
- Mathematical algorithms

**Expected output:** Exit code 18 (fib(5)=5 + fib(7)=13)

### [calculator.wyn](../examples/calculator.wyn)
**Enum-based calculator**

```wyn
enum Operation {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE
}

fn calculate(op: Operation, a: int, b: int) -> int {
    match op {
        Operation::ADD => return a + b,
        Operation::SUBTRACT => return a - b,
        Operation::MULTIPLY => return a * b,
        Operation::DIVIDE => {
            if b != 0 {
                return a / b;
            } else {
                return 0;
            }
        }
    }
}

fn main() -> int {
    let result1 = calculate(Operation::ADD, 10, 15);
    let result2 = calculate(Operation::MULTIPLY, 2, 8);
    return result1 + result2;
}
```

**Demonstrates:**
- Enum definitions
- Pattern matching
- Qualified enum names
- Error handling (division by zero)
- Complex control flow

**Expected output:** Exit code 41 (25 + 16)

### [binary_search.wyn](../examples/binary_search.wyn)
**Binary search algorithm**

```wyn
fn binary_search(arr: [int; 7], target: int) -> int {
    let mut left = 0;
    let mut right = 6;
    
    while left <= right {
        let mid = (left + right) / 2;
        
        if arr[mid] == target {
            return mid;
        } else if arr[mid] < target {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;  // Not found
}

fn main() -> int {
    let sorted_array = [1, 3, 5, 7, 9, 11, 13];
    let index = binary_search(sorted_array, 7);
    return index;
}
```

**Demonstrates:**
- Search algorithms
- Array manipulation
- Loop control
- Conditional logic
- Algorithm implementation

**Expected output:** Exit code 3 (index of value 7)

### [quicksort.wyn](../examples/quicksort.wyn)
**Quicksort sorting algorithm**

```wyn
fn partition(arr: [int; 8], low: int, high: int) -> int {
    let pivot = arr[high];
    let mut i = low - 1;
    
    let mut j = low;
    while j < high {
        if arr[j] <= pivot {
            i = i + 1;
            // Swap arr[i] and arr[j]
            let temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
        j = j + 1;
    }
    
    // Swap arr[i+1] and arr[high]
    let temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    
    return i + 1;
}

fn quicksort(arr: [int; 8], low: int, high: int) -> void {
    if low < high {
        let pi = partition(arr, low, high);
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

fn main() -> int {
    let mut numbers = [64, 34, 25, 12, 22, 11, 90, 5];
    quicksort(numbers, 0, 7);
    
    // Return sum of sorted array
    let mut sum = 0;
    let mut i = 0;
    while i < 8 {
        sum = sum + numbers[i];
        i = i + 1;
    }
    
    return sum;
}
```

**Demonstrates:**
- Complex algorithms
- Recursive functions
- Array manipulation
- In-place sorting
- Multiple function coordination

**Expected output:** Exit code equal to sum of array elements

## Modules and Organization

### [07_modules/](../examples/07_modules/)
**Module system demonstration**

**main.wyn:**
```wyn
import math_user;

fn main() -> int {
    return math_user::calculate_result();
}
```

**math_user.wyn:**
```wyn
export fn calculate_result() -> int {
    return add(10, 20) + multiply(3, 4);
}

fn add(a: int, b: int) -> int {
    return a + b;
}

fn multiply(a: int, b: int) -> int {
    return a * b;
}
```

**Demonstrates:**
- Module imports
- Function exports
- Module organization
- Multi-file projects

**Expected output:** Exit code 42 (30 + 12)

## Game and Simulation Examples

### [game_of_life.wyn](../examples/game_of_life.wyn)
**Conway's Game of Life simulation**

```wyn
struct Grid {
    cells: [[int; 10]; 10],
    width: int,
    height: int
}

fn count_neighbors(grid: Grid, x: int, y: int) -> int {
    let mut count = 0;
    // Implementation details...
    return count;
}

fn next_generation(grid: Grid) -> Grid {
    // Game of Life rules implementation
    return grid;
}

fn main() -> int {
    let mut grid = initialize_grid();
    
    // Run simulation for several generations
    let mut generation = 0;
    while generation < 5 {
        grid = next_generation(grid);
        generation = generation + 1;
    }
    
    return count_live_cells(grid);
}
```

**Demonstrates:**
- 2D arrays
- Complex data structures
- Simulation algorithms
- Nested loops
- State management

**Expected output:** Exit code equal to live cells after 5 generations

### [state_machine.wyn](../examples/state_machine.wyn)
**Finite state machine implementation**

```wyn
enum State {
    IDLE,
    RUNNING,
    PAUSED,
    STOPPED
}

enum Event {
    START,
    PAUSE,
    RESUME,
    STOP
}

fn transition(current: State, event: Event) -> State {
    match current {
        State::IDLE => {
            match event {
                Event::START => return State::RUNNING,
                _ => return current
            }
        },
        State::RUNNING => {
            match event {
                Event::PAUSE => return State::PAUSED,
                Event::STOP => return State::STOPPED,
                _ => return current
            }
        },
        // More states...
    }
}

fn main() -> int {
    let mut state = State::IDLE;
    
    state = transition(state, Event::START);
    state = transition(state, Event::PAUSE);
    state = transition(state, Event::RESUME);
    
    return state as int;
}
```

**Demonstrates:**
- Enums for state representation
- Pattern matching
- State machines
- Event handling
- Complex control flow

**Expected output:** Exit code representing final state

## Performance and Utilities

### [hash_function.wyn](../examples/hash_function.wyn)
**Custom hash function implementation**

```wyn
fn simple_hash(text: string) -> int {
    let mut hash = 0;
    let len = str_len(text);
    let mut i = 0;
    
    while i < len {
        // Simple hash algorithm
        hash = hash * 31 + char_at(text, i);
        i = i + 1;
    }
    
    return hash;
}

fn main() -> int {
    let hash1 = simple_hash("hello");
    let hash2 = simple_hash("world");
    return abs_val(hash1) + abs_val(hash2);
}
```

**Demonstrates:**
- String processing
- Hash algorithms
- Character manipulation
- Mathematical operations

**Expected output:** Exit code equal to sum of hash values

## Archive Examples

The [`../examples/archive/`](../examples/archive/) directory contains 100+ additional examples covering:

- **Advanced algorithms** - Sorting, searching, graph algorithms
- **Data structures** - Trees, linked lists, hash tables
- **Language features** - All syntax variations and edge cases
- **Performance tests** - Benchmarking and optimization examples
- **Experimental code** - Prototype features and tests

## Creating Your Own Examples

### Example Template

```wyn
// Description: What this example demonstrates
// Expected output: What should happen when run

fn main() -> int {
    // Your code here
    return 0;
}
```

### Best Practices

1. **Include comments** explaining what the code demonstrates
2. **Use meaningful names** for functions and variables
3. **Keep examples focused** on one concept
4. **Test your examples** to ensure they work correctly
5. **Document expected output** in comments

### Contributing Examples

To contribute a new example:

1. Create a `.wyn` file in the examples directory
2. Test that it compiles and runs correctly
3. Add documentation explaining what it demonstrates
4. Submit a pull request with your example

## See Also

- [**Getting Started**](getting-started.md) - Installation and basic concepts
- [**Language Guide**](language-guide.md) - Complete syntax reference
- [**Standard Library**](stdlib-reference.md) - Available functions and methods
- [**FAQ**](faq.md) - Common questions and troubleshooting

---

*These examples are for Wyn v1.2.2. All examples are tested and verified to work correctly.*