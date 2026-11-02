// Smoke test: goroutines making system calls (getpid, pipe write/read, stat).
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

func main() {
	_ = C.cpid()
	const G, N = 64, 20000
	var wg sync.WaitGroup
	var mu sync.Mutex
	total := 0
	for g := 0; g < G; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			r, w, _ := os.Pipe()
			b := []byte{1}
			n := 0
			for i := 0; i < N; i++ {
				if syscall.Getpid() > 0 {
					n++
				}
				w.Write(b)
				r.Read(b)
				if _, err := os.Stat("/proc/self"); err == nil {
					n++
				}
			}
			r.Close(); w.Close()
			mu.Lock(); total += n; mu.Unlock()
		}()
	}
	wg.Wait()
	fmt.Println("total", total, "want", 2*G*N)
}
