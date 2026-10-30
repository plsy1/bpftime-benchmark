# Deterministic system call mix: dlopen-ed extension modules (ssl), CPU-time
# clocks (vDSO fallback svc), threads (clone3), file reads and writes.
import json, os, ssl, threading, time
for _ in range(200):
    time.process_time()
def work():
    for _ in range(10):
        os.getppid()
threads = [threading.Thread(target=work) for _ in range(4)]
for t in threads:
    t.start()
for t in threads:
    t.join()
for _ in range(100):
    with open('/etc/hostname') as f:
        f.read()
fd = os.open('/dev/null', os.O_WRONLY)
for _ in range(100):
    os.write(fd, b'x')
os.close(fd)
