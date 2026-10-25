#!/usr/bin/env python3
"""Drive an interactive bash through a pty so readline runs: send N `echo audit-line-K` lines,
then exit. Prints the number of lines sent and the elapsed time."""
import os, pty, sys, time

n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
pid, fd = pty.fork()
if pid == 0:
    os.execvp('bash', ['bash', '--norc', '--noprofile', '-i'])
t0 = time.time()
def drain(timeout=0.05):
    import select
    while select.select([fd], [], [], timeout)[0]:
        try:
            if not os.read(fd, 65536):
                return
        except OSError:
            return
drain(0.5)
for k in range(n):
    os.write(fd, f'echo audit-line-{k}\n'.encode())
    drain(0.01)
os.write(fd, b'exit\n')
drain(0.5)
os.waitpid(pid, 0)
print(f'sent {n} lines in {time.time() - t0:.2f}s')
