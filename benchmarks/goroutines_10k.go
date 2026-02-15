// Go equivalent benchmark
package main

import (
	"fmt"
	"time"
)

func worker(id int) int {
	return id
}

func main() {
	start := time.Now()
	
	for i := 0; i < 10000; i++ {
		go worker(i)
	}
	
	elapsed := time.Since(start)
	fmt.Printf("10,000 goroutines: %dms\n", elapsed.Milliseconds())
}
