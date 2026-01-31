package main

import (
	"fmt"
	"time"
)

func compute(n int) int {
	return n * n
}

func main() {
	start := time.Now()
	
	results := make([]chan int, 10000)
	for i := 0; i < 10000; i++ {
		results[i] = make(chan int, 1)
		go func(n int, ch chan int) {
			ch <- compute(n)
		}(i, results[i])
	}
	
	errors := 0
	for i := 0; i < 10000; i++ {
		r := <-results[i]
		if r != i*i {
			errors++
		}
	}
	
	elapsed := time.Since(start)
	
	fmt.Printf("Completed: %d errors\n", errors)
	fmt.Printf("Time: %.2f ms\n", float64(elapsed.Microseconds())/1000.0)
	fmt.Printf("Per operation: %.2f μs\n", float64(elapsed.Nanoseconds())/10000.0/1000.0)
	
	if errors == 0 {
		fmt.Println("✅ PASS")
	} else {
		fmt.Println("❌ FAIL")
	}
}
