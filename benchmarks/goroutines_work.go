package main

import (
	"fmt"
	"time"
)

func fib(n int) int {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func main() {
	start := time.Now()
	
	for i := 0; i < 1000; i++ {
		go fib(20)
	}
	
	elapsed := time.Since(start)
	fmt.Printf("1,000 goroutines (fib20): %dms\n", elapsed.Milliseconds())
}
