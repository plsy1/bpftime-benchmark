// Node: JIT-compiled hot loop plus file and timer I/O on the libuv thread pool.
const fs = require('fs');
let s = 0;
for (let i = 0; i < 2e7; i++) s = (s + i * 7) % 1000003;
let n = 0;
for (let i = 0; i < 2000; i++) { fs.statSync('/proc/self'); n++; }
let pending = 200;
for (let i = 0; i < 200; i++) fs.readFile('/proc/self/stat', (e, b) => { if (!e) n++; if (--pending === 0) console.log('ok', s, n); });
