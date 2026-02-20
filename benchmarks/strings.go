package main

import (
	"fmt"
	"strconv"
	"strings"
)

func main() {
	count := 0
	for i := 0; i < 10000; i++ {
		s := "hello world " + strconv.Itoa(i)
		if strings.Contains(s, "5000") {
			count++
		}
		u := strings.ToUpper(s)
		r := strings.Replace(u, "HELLO", "HI", -1)
		_ = strings.Split(r, " ")
	}
	fmt.Printf("string ops done, matches = %d\n", count)
}
