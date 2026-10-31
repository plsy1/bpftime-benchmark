package main
import ("fmt"; "os"; "os/exec"; "sync"; "runtime")
func main() {
	var wg sync.WaitGroup
	for i := 0; i < 32; i++ { wg.Add(1); go func(i int) { defer wg.Done(); for j := 0; j < 100; j++ { f, err := os.Open("/proc/self/stat"); if err != nil { panic(err) }; b := make([]byte, 64); f.Read(b); f.Close(); runtime.Gosched() } }(i) }
	wg.Wait()
	for i := 0; i < 20; i++ { out, err := exec.Command("/bin/echo", "hi").Output(); if err != nil || string(out) != "hi\n" { fmt.Println("exec failed", err, string(out)); os.Exit(1) } }
	fmt.Println("go OK threads", runtime.NumGoroutine())
}
