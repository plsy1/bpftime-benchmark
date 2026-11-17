"""Locate PTX functions/kernels by brace depth. ptxfn.py FILE NAME_SUBSTRING"""
import re, sys
def find(lines, pred):
    """yield (start, end) line indexes (0-based, inclusive) of definitions whose
    header line satisfies pred"""
    i = 0
    n = len(lines)
    while i < n:
        if pred(lines[i]):
            depth = 0; started = False
            for j in range(i, n):
                code = lines[j].split('//', 1)[0]
                depth += code.count('{') - code.count('}')
                if '{' in code: started = True
                if started and depth == 0:
                    yield (i, j); i = j; break
        i += 1
if __name__ == '__main__':
    L = open(sys.argv[1]).read().split('\n')
    for a, b in find(L, lambda l: (l.startswith('.visible .func') or l.startswith('.entry') or l.startswith('.visible .entry') or l.startswith('.func')) and sys.argv[2] in l):
        print(a + 1, b + 1, L[a][:90])
