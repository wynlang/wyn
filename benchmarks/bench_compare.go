package main

import (
	"fmt"
	"sync"
	"time"
)

func compute(n int) int {
	return n * n
}

func benchmark(count int) {
	start := time.Now()
	
	results := make([]int, count)
	var wg sync.WaitGroup
	
	for i := 0; i < count; i++ {
		wg.Add(1)
		i := i
		go func() {
			results[i] = compute(i)
			wg.Done()
		}()
	}
	
	wg.Wait()
	elapsed := time.Since(start)
	fmt.Printf("Go %d: %d ms\n", count, elapsed.Milliseconds())
}

func main() {
	benchmark(10000)
	benchmark(100000)
	benchmark(1000000)
	benchmark(10000000)
}
