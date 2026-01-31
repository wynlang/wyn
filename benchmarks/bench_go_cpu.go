package main

import (
	"fmt"
	"sync"
	"time"
)

func fib(n int) int {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func main() {
	var wg sync.WaitGroup
	start := time.Now()
	
	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			fib(25)
		}()
	}
	
	wg.Wait()
	elapsed := time.Since(start)
	fmt.Printf("%d\n", elapsed.Milliseconds())
}
