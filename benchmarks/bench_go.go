// Go equivalent benchmark — run with: go run bench_go.go
package main

import (
	"fmt"
	"sync"
	"time"
)

func lightWork(id int) int {
	return id*2 + 1
}

func recurse(n int) int {
	if n <= 0 {
		return 0
	}
	return recurse(n-1) + 1
}

func main() {
	fmt.Println("=== Go Goroutine Benchmark ===")
	fmt.Println()

	// 1. Spawn+await throughput
	fmt.Println("[1] Goroutine+channel throughput (sequential)")
	start := time.Now()
	for i := 0; i < 10000; i++ {
		ch := make(chan int, 1)
		go func(id int) { ch <- lightWork(id) }(i)
		<-ch
	}
	elapsed := time.Since(start)
	fmt.Printf("  10,000 goroutine+recv: %v\n", elapsed)
	fmt.Printf("  Per op: %v\n", elapsed/10000)
	fmt.Println()

	// 2. Concurrent capacity
	fmt.Println("[2] Concurrent goroutine capacity")
	for _, n := range []int{1000, 10000, 50000, 100000} {
		s := time.Now()
		var wg sync.WaitGroup
		for i := 0; i < n; i++ {
			wg.Add(1)
			go func() { defer wg.Done() }()
		}
		wg.Wait()
		fmt.Printf("  %d concurrent: %v\n", n, time.Since(s))
	}
	fmt.Println()

	// 3. Recursion depth
	fmt.Println("[3] Max recursion depth in goroutine")
	for _, d := range []int{1000, 10000, 100000, 500000} {
		ch := make(chan int, 1)
		go func(depth int) { ch <- recurse(depth) }(d)
		r := <-ch
		if r == d {
			fmt.Printf("  depth %d: ✓\n", d)
		} else {
			fmt.Printf("  depth %d: ✗\n", d)
		}
	}

	fmt.Println()
	fmt.Println("=== Benchmark complete ===")
}
