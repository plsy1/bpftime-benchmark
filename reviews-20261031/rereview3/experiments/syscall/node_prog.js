const { Worker, isMainThread, parentPort } = require('worker_threads'); const cp = require('child_process'); const fs = require('fs');
if (!isMainThread) { let s = 0; for (let i = 0; i < 200; i++) s += fs.readFileSync('/proc/self/stat').length; parentPort.postMessage(s); } else {
  let done = 0; for (let i = 0; i < 4; i++) { const w = new Worker(__filename); w.on('message', () => { if (++done == 4) { for (let j = 0; j < 10; j++) { if (cp.execFileSync('/bin/echo', ['hi']).toString() !== 'hi\n') process.exit(1); } console.log('node OK'); } }); } }
