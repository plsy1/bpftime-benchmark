// The hook stack after os/exec (review 2026-11-05, M1): Go's exec makes a
// clone(CLONE_VFORK|CLONE_VM) child that execs on its parent thread's TLS and
// hook stack. With the frame it abandons still marked live, every later hooked
// call of that thread ran on the caller's stack: with a program attached, an
// 18 KB frame on a goroutine stack (2 KiB to start, no guard page). Each round
// execs /bin/true from several goroutines, then runs deep's workload:
// goroutines making a system call at every call depth next to sleeping
// goroutines whose canaries a stray write would hit. CPU-bound goroutines keep
// asynchronous preemption (SIGURG, handled on the signal stack) going.
package main

/*
#include <unistd.h>
static int cpid(void) { return getpid(); }
*/
import "C"

import (
	"fmt"
	"os"
	"os/exec"
	"sync"
	"sync/atomic"
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
	var spin atomic.Bool
	spin.Store(true)
	var spinners sync.WaitGroup
	for i := 0; i < 2; i++ {
		spinners.Add(1)
		go func() {
			defer spinners.Done()
			x := uint64(1)
			for spin.Load() {
				x = x*6364136223846793005 + 1
			}
			_ = x
		}()
	}
	execs := 0
	for round := 0; round < 20; round++ {
		var ex sync.WaitGroup
		var failed atomic.Int32
		for i := 0; i < 8; i++ {
			ex.Add(1)
			go func() {
				defer ex.Done()
				if err := exec.Command("/bin/true").Run(); err != nil {
					failed.Add(1)
				}
			}()
		}
		ex.Wait()
		if failed.Load() != 0 {
			fmt.Println("exec failed", failed.Load())
			os.Exit(4)
		}
		execs += 8
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
	spin.Store(false)
	spinners.Wait()
	fmt.Println("execs", execs, "corrupted canary words", bad)
	if bad != 0 {
		os.Exit(3)
	}
}
