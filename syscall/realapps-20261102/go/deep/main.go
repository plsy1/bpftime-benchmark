// Goroutines on fresh 2 KiB stacks make a system call at every call depth (so
// some calls run with the stack pointer just above the stack guard), while
// sleeping goroutines keep a canary in their outermost frame. Goroutine stacks
// have no guard page: a system call hook that writes below the stack's low end
// lands in a neighbouring stack, often a canary goroutine's top frame.
package main

/*
#include <unistd.h>
static int cpid(void) { return getpid(); }
*/
import "C"

import (
	"fmt"
	"os"
	"sync"
	"syscall"
)

const pattern = 0x5a5a5a5a5a5a5a5a

//go:noinline
func down(d int, pad uint64) uint64 {
	var a [2]uint64
	a[0] = pad
	if d == 0 {
		syscall.Getpid()
		return a[0]
	}
	return down(d-1, pad+1) + a[0]
}

func canary(start, stop *sync.WaitGroup, bad *int64, mu *sync.Mutex) {
	var c [32]uint64
	for i := range c {
		c[i] = pattern
	}
	start.Done()
	stop.Wait()
	for i := range c {
		if c[i] != pattern {
			mu.Lock()
			*bad++
			mu.Unlock()
		}
	}
}

func main() {
	_ = C.cpid()
	var bad int64
	var mu sync.Mutex
	for round := 0; round < 20; round++ {
		var start, stop, done, downs sync.WaitGroup
		stop.Add(1)
		for d := 0; d < 80; d++ {
			start.Add(1)
			done.Add(1)
			go func() { canary(&start, &stop, &bad, &mu); done.Done() }()
			downs.Add(1)
			go func(d int) { down(d, uint64(d)); downs.Done() }(d)
		}
		start.Wait()
		downs.Wait()
		stop.Done()
		done.Wait()
	}
	fmt.Println("corrupted canary words", bad)
	if bad != 0 {
		os.Exit(3)
	}
}
