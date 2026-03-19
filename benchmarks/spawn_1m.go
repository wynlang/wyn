package main

import (
	"fmt"
	"sync"
)

func main() {
	var wg sync.WaitGroup
	for i := 0; i < 1_000_000; i++ {
		wg.Add(1)
		go func(id int) {
			_ = id * id
			wg.Done()
		}(i)
	}
	wg.Wait()
	fmt.Println("spawned 1M goroutines")
}
