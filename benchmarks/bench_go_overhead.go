package main

import (
	"fmt"
	"time"
)

func noop() {}

func main() {
	start := time.Now()
	
	for i := 0; i < 10000; i++ {
		go noop()
	}
	
	elapsed := time.Since(start)
	fmt.Printf("%d\n", elapsed.Microseconds())
}
