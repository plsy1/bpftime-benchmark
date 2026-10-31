import threading, subprocess, os, sys, multiprocessing, socket, time
def worker(n):
    for i in range(200):
        with open('/proc/self/stat') as f: f.read()
        os.getpid()
ts=[threading.Thread(target=worker,args=(i,)) for i in range(4)]
[t.start() for t in ts]; [t.join() for t in ts]
for i in range(20):
    assert subprocess.run(['/bin/true']).returncode == 0
r = subprocess.run(['/bin/echo','hi'], capture_output=True, text=True); assert r.stdout=='hi\n'
for i in range(10):
    pid=os.fork()
    if pid==0:
        os._exit(0 if os.getpid()!=os.getppid() else 1)
    _,st=os.waitpid(pid,0); assert st==0
def mp(q): q.put(os.getpid())
ctx=multiprocessing.get_context('fork'); q=ctx.Queue(); p=ctx.Process(target=mp,args=(q,)); p.start(); assert q.get()==p.pid; p.join(); assert p.exitcode==0
a,b=socket.socketpair(); a.send(b'x'); assert b.recv(1)==b'x'
print("python OK")
