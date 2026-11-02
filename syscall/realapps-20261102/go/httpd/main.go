// A Go HTTP server and its clients in one process: many goroutines blocked in
// reads (so preemption signals land while hooked calls are live), GC on, file
// reads per request. Checks every response.
package main

/*
#include <unistd.h>
static int cpid(void) { return getpid(); }
*/
import "C"

import (
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

func main() {
	_ = C.cpid()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		b, err := os.ReadFile("/proc/self/stat")
		if err != nil || len(b) == 0 {
			http.Error(w, "read", 500)
			return
		}
		buf := make([]byte, 4096) // garbage for the GC
		_ = buf
		io.WriteString(w, r.URL.Query().Get("n"))
	})
	go http.Serve(ln, nil)
	url := "http://" + ln.Addr().String() + "/?n="
	const G, N = 64, 300
	var wg sync.WaitGroup
	var bad, good atomic.Int64
	deadline := time.Now().Add(60 * time.Second)
	for g := 0; g < G; g++ {
		wg.Add(1)
		go func(g int) {
			defer wg.Done()
			c := &http.Client{}
			for i := 0; i < N && time.Now().Before(deadline); i++ {
				want := strconv.Itoa(g*N + i)
				resp, err := c.Get(url + want)
				if err != nil {
					bad.Add(1)
					continue
				}
				body, _ := io.ReadAll(resp.Body)
				resp.Body.Close()
				if string(body) != want {
					bad.Add(1)
				} else {
					good.Add(1)
				}
			}
		}(g)
	}
	wg.Wait()
	fmt.Println("good", good.Load(), "bad", bad.Load(), "want", G*N)
	if bad.Load() != 0 || good.Load() != G*N {
		os.Exit(3)
	}
}
