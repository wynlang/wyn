package main

import "fmt"

func fib(n int) int {
	if n <= 1 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func main() {
	result := fib(35)
	fmt.Printf("fib(35) = %d\n", result)
}
