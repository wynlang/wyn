package main

import (
	"fmt"
	"runtime"
	"time"
)

func noop() {}

func benchmark(n int, passes int) {
	var times []int64
	
	for pass := 0; pass < passes; pass++ {
		runtime.GC() // Clean slate
		
		start := time.Now()
		for i := 0; i < n; i++ {
			go noop()
		}
		elapsed := time.Since(start).Nanoseconds()
		times = append(times, elapsed)
	}
	
	// Calculate statistics
	var sum, min, max int64
	min = times[0]
	max = times[0]
	
	for _, t := range times {
		sum += t
		if t < min {
			min = t
		}
		if t > max {
			max = t
		}
	}
	
	avg := sum / int64(len(times))
	
	fmt.Printf("%d spawns (%d passes):\n", n, passes)
	fmt.Printf("  Min: %dns (%.2fms)\n", min, float64(min)/1e6)
	fmt.Printf("  Avg: %dns (%.2fms)\n", avg, float64(avg)/1e6)
	fmt.Printf("  Max: %dns (%.2fms)\n", max, float64(max)/1e6)
	fmt.Printf("  Per-spawn: %dns\n", avg/int64(n))
	fmt.Println()
}

func main() {
	runtime.GOMAXPROCS(runtime.NumCPU())
	fmt.Printf("Go Benchmark (GOMAXPROCS=%d)\n\n", runtime.NumCPU())
	
	benchmark(10000, 10)
	benchmark(100000, 5)
	benchmark(1000000, 3)
}
