"""nsys_parse.py SQLITE COPY_BYTES: counts from one nsys export (stdlib only).

Prints JSON: kernels, kernels inside graphs (graphNodeId set), runtime API
calls of the graph family, device-to-host copies of COPY_BYTES (the shim's)
and of other sizes, and how much of the shim's copy time overlaps kernel
execution (union of kernel intervals).
"""
import json
import sqlite3
import sys


def main():
    path, copy_bytes = sys.argv[1], int(sys.argv[2])
    c = sqlite3.connect("file:%s?mode=ro" % path, uri=True)
    tables = {r[0] for r in c.execute("select name from sqlite_master where type='table'")}
    out = {"sqlite": path, "copy_bytes": copy_bytes}
    if "CUPTI_ACTIVITY_KIND_KERNEL" in tables:
        k = c.execute(
            "select count(*), sum(graphNodeId is not null and graphNodeId != 0),"
            " sum(end - start) from CUPTI_ACTIVITY_KIND_KERNEL").fetchone()
        out["kernels"], out["kernels_in_graphs"], out["kernel_ns"] = k[0], k[1] or 0, k[2] or 0
        kiv = c.execute("select start, end from CUPTI_ACTIVITY_KIND_KERNEL order by start").fetchall()
    else:
        out["kernels"] = out["kernels_in_graphs"] = out["kernel_ns"] = 0
        kiv = []
    api = {}
    if "CUPTI_ACTIVITY_KIND_RUNTIME" in tables:
        for name, n in c.execute(
                "select s.value, count(*) from CUPTI_ACTIVITY_KIND_RUNTIME r join StringIds s"
                " on r.nameId = s.id where s.value like 'cudaGraph%' or s.value like"
                " 'cudaStream%Capture%' group by s.value"):
            api[name] = n
    out["runtime_api"] = api
    # union of kernel intervals
    union = []
    for s, e in kiv:
        if union and s <= union[-1][1]:
            if e > union[-1][1]:
                union[-1][1] = e
        else:
            union.append([s, e])
    copies = []
    other = {}
    if "CUPTI_ACTIVITY_KIND_MEMCPY" in tables:
        for s, e, b, kind, stream in c.execute(
                "select start, end, bytes, copyKind, streamId from CUPTI_ACTIVITY_KIND_MEMCPY"):
            if kind == 2 and b == copy_bytes and copy_bytes > 0:
                copies.append((s, e, stream))
            else:
                key = "kind%d" % kind
                other[key] = other.get(key, 0) + 1
    copies.sort()
    ov = 0
    tot = 0
    j = 0
    for s, e, _ in copies:
        tot += e - s
        while j < len(union) and union[j][1] <= s:
            j += 1
        jj = j
        while jj < len(union) and union[jj][0] < e:
            ov += min(e, union[jj][1]) - max(s, union[jj][0])
            jj += 1
    out["shim_copies"] = len(copies)
    out["shim_copy_streams"] = sorted({st for _, _, st in copies})
    out["shim_copy_ns"] = tot
    out["shim_copy_ns_overlapping_kernels"] = ov
    out["shim_copy_overlap_share"] = (ov / tot) if tot else None
    out["other_memcpy"] = other
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
