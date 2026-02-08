package main

import (
	"fmt"
	"time"
)

func compute(n int) int {
	return n * n
}

func benchmark(count int) {
	start := time.Now()
	
	results := make([]chan int, count)
	for i := 0; i < count; i++ {
		results[i] = make(chan int, 1)
		go func(n int, ch chan int) {
			ch <- compute(n)
		}(i, results[i])
	}
	
	for i := 0; i < count; i++ {
		<-results[i]
	}
	
	elapsed := time.Since(start)
	fmt.Printf("%d,%d\n", count, elapsed.Milliseconds())
}

func main() {
	benchmark(10000)
	benchmark(100000)
	benchmark(1000000)
	benchmark(10000000)
}
