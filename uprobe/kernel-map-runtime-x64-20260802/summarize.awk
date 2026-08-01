#!/usr/bin/awk -f

BEGIN {
    FS = ","
    OFS = ","
    operations[1] = "array_lookup"
    operations[2] = "array_update"
    operations[3] = "hash_lookup"
    operations[4] = "hash_update"
    arm64["array_lookup"] = 1.384079
    arm64["array_update"] = 10.951741
    arm64["hash_lookup"] = 26.768945
    arm64["hash_update"] = 90.822847
}

NR == 1 {
    expected = "operation,implementation,round,run_cnt,runtime_ns,avg_ns_per_invocation"
    if ($0 != expected) {
        print "unexpected CSV header: " $0 > "/dev/stderr"
        exit 1
    }
    next
}

{
    operation = $1
    implementation = $2
    round = $3 + 0
    key = operation SUBSEP round

    if (!(operation in arm64) || round < 1 || round > 5) {
        print "unexpected operation or round at line " NR > "/dev/stderr"
        exit 1
    }
    if ($4 + 0 != 20000) {
        print "unexpected run_cnt at line " NR ": " $4 > "/dev/stderr"
        exit 1
    }
    if (++seen[key, implementation] != 1) {
        print "duplicate sample at line " NR > "/dev/stderr"
        exit 1
    }

    if (implementation == "control") {
        control_runtime[key] = $5 + 0
        control_count[key] = $4 + 0
    } else if (implementation == "real") {
        real_runtime[key] = $5 + 0
        real_count[key] = $4 + 0
    } else if (implementation == "real-minus-control") {
        reported[key] = $6 + 0
    } else {
        print "unexpected implementation at line " NR ": " implementation > "/dev/stderr"
        exit 1
    }
}

END {
    if (NR != 61) {
        print "unexpected row count: expected 61 including header, got " NR > "/dev/stderr"
        exit 1
    }

    print "operation", "samples", "mean_ns_per_helper", "sample_stddev_ns", "cv_percent", "min_ns", "max_ns", "arm64_ns_per_helper", "x64_over_arm64"
    for (i = 1; i <= 4; ++i) {
        operation = operations[i]
        sum = 0
        sumsq = 0
        min = ""
        max = ""
        for (round = 1; round <= 5; ++round) {
            key = operation SUBSEP round
            if (!(key in control_runtime) || !(key in real_runtime) ||
                !(key in reported)) {
                print "missing sample for " operation " round " round > "/dev/stderr"
                exit 1
            }
            recomputed = (real_runtime[key] / real_count[key] - control_runtime[key] / control_count[key]) / 1000.0
            difference = recomputed - reported[key]
            if (difference < 0)
                difference = -difference
            if (difference > 0.000002) {
                print "delta mismatch for " operation " round " round > "/dev/stderr"
                exit 1
            }
            value = reported[key]
            sum += value
            sumsq += value * value
            if (min == "" || value < min)
                min = value
            if (max == "" || value > max)
                max = value
        }
        mean = sum / 5.0
        variance = (sumsq - 5.0 * mean * mean) / 4.0
        if (variance < 0 && variance > -1e-12)
            variance = 0
        stddev = sqrt(variance)
        cv = stddev / mean * 100.0
        ratio = mean / arm64[operation]
        printf "%s,5,%.6f,%.6f,%.3f,%.6f,%.6f,%.6f,%.6f\n", operation, mean, stddev, cv, min, max, arm64[operation], ratio
    }
}
