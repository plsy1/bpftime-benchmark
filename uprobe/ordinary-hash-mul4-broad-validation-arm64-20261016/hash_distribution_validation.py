#!/usr/bin/env python3

"""Compare upstream and multiplicative uint32 hash behavior.

This deterministic model mirrors bpftime_hash_map's prime bucket count,
linear probing, and EMPTY/FILLED/DELETED states. It covers ordinary, random,
and adversarial key distributions at several capacities and load factors. The
reported unit is bucket probes rather than wall-clock time.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import random
import statistics
import sys
from dataclasses import asdict, dataclass
from typing import Callable, Iterable


HashFunction = Callable[[int], int]

EMPTY = 0
FILLED = 1
DELETED = 2
MASK32 = 0xFFFFFFFF
GOLDEN_RATIO_32 = 0x9E3779B1


def is_prime(value: int) -> bool:
    if value <= 1:
        return False
    if value <= 3:
        return True
    if value % 2 == 0 or value % 3 == 0:
        return False
    divisor = 5
    while divisor * divisor <= value:
        if value % divisor == 0 or value % (divisor + 2) == 0:
            return False
        divisor += 6
    return True


def next_prime(value: int) -> int:
    while not is_prime(value):
        value += 1
    return value


def upstream_hash(value: int) -> int:
    result = 0
    for shift in range(0, 32, 8):
        result = result * 31 + ((value >> shift) & 0xFF)
    return result


def candidate_hash(value: int) -> int:
    return (value * GOLDEN_RATIO_32) & MASK32


def mix32(value: int) -> int:
    value = (value ^ (value >> 16)) & MASK32
    value = (value * 0x7FEB352D) & MASK32
    value = (value ^ (value >> 15)) & MASK32
    value = (value * 0x846CA68B) & MASK32
    return (value ^ (value >> 16)) & MASK32


def make_keys(pattern: str, count: int) -> list[int]:
    if pattern == "sequential":
        keys = list(range(count))
    elif pattern == "sequential-offset":
        keys = [0x80000000 + index for index in range(count)]
    elif pattern == "reverse":
        keys = list(reversed(range(count)))
    elif pattern.startswith("stride-"):
        stride = int(pattern.removeprefix("stride-"))
        keys = [((index * stride) + 7) & MASK32 for index in range(count)]
    elif pattern == "fixed-low-16":
        keys = [((index << 16) | 0x55AA) & MASK32 for index in range(count)]
    elif pattern == "fixed-high-16":
        keys = [0x55AA0000 | index for index in range(count)]
    elif pattern == "lcg":
        keys = [
            (index * 1664525 + 1013904223) & MASK32
            for index in range(count)
        ]
    elif pattern == "mixed":
        keys = [mix32(index) for index in range(count)]
    elif pattern.startswith("random-"):
        seed = int(pattern.removeprefix("random-"))
        keys = random.Random(seed).sample(range(1 << 32), count)
    else:
        raise ValueError(f"unknown key pattern: {pattern}")

    if len(set(keys)) != len(keys):
        raise RuntimeError(f"{pattern} produced duplicate keys")
    return keys


PATTERNS = [
    "sequential",
    "sequential-offset",
    "reverse",
    "stride-2",
    "stride-16",
    "stride-256",
    "stride-4096",
    "stride-65536",
    "fixed-low-16",
    "fixed-high-16",
    "lcg",
    "mixed",
    "random-1",
    "random-7",
    "random-42",
]


def replacement_keys(existing: Iterable[int], count: int, salt: int) -> list[int]:
    occupied = set(existing)
    result: list[int] = []
    index = 0
    while len(result) < count:
        key = mix32(index + salt)
        index += 1
        if key not in occupied:
            occupied.add(key)
            result.append(key)
    return result


class LinearProbeMap:
    def __init__(self, capacity: int, hash_function: HashFunction):
        self.bucket_count = next_prime(capacity)
        self.states = [EMPTY] * self.bucket_count
        self.keys = [0] * self.bucket_count
        self.hash_function = hash_function

    def update(self, key: int) -> int:
        bucket = self.hash_function(key) % self.bucket_count
        start = bucket
        first_deleted: int | None = None
        probes = 0
        while True:
            probes += 1
            state = self.states[bucket]
            if state == EMPTY:
                target = first_deleted if first_deleted is not None else bucket
                self.states[target] = FILLED
                self.keys[target] = key
                return probes
            if state == DELETED and first_deleted is None:
                first_deleted = bucket
            elif state == FILLED and self.keys[bucket] == key:
                return probes
            bucket = (bucket + 1) % self.bucket_count
            if bucket == start:
                if first_deleted is None:
                    raise RuntimeError("hash table has no insertion slot")
                self.states[first_deleted] = FILLED
                self.keys[first_deleted] = key
                return probes

    def lookup(self, key: int) -> int:
        bucket = self.hash_function(key) % self.bucket_count
        start = bucket
        probes = 0
        while True:
            probes += 1
            state = self.states[bucket]
            if state == EMPTY:
                return probes
            if state == FILLED and self.keys[bucket] == key:
                return probes
            bucket = (bucket + 1) % self.bucket_count
            if bucket == start:
                return probes

    def delete(self, key: int) -> int:
        bucket = self.hash_function(key) % self.bucket_count
        start = bucket
        probes = 0
        while True:
            probes += 1
            state = self.states[bucket]
            if state == EMPTY:
                raise RuntimeError(f"delete missed key {key}")
            if state == FILLED and self.keys[bucket] == key:
                self.states[bucket] = DELETED
                return probes
            bucket = (bucket + 1) % self.bucket_count
            if bucket == start:
                raise RuntimeError(f"delete missed key {key}")


@dataclass(frozen=True)
class ProbeStats:
    average: float
    p50: int
    p95: int
    p99: int
    maximum: int


@dataclass(frozen=True)
class Result:
    capacity: int
    bucket_count: int
    load: float
    entries: int
    pattern: str
    phase: str
    delete_fraction: float
    upstream_average: float
    candidate_average: float
    average_ratio: float
    upstream_p95: int
    candidate_p95: int
    p95_ratio: float
    upstream_p99: int
    candidate_p99: int
    upstream_maximum: int
    candidate_maximum: int


def percentile(values: list[int], quantile: float) -> int:
    ordered = sorted(values)
    index = max(0, math.ceil(len(ordered) * quantile) - 1)
    return ordered[index]


def probe_stats(values: list[int]) -> ProbeStats:
    return ProbeStats(
        average=statistics.fmean(values),
        p50=percentile(values, 0.50),
        p95=percentile(values, 0.95),
        p99=percentile(values, 0.99),
        maximum=max(values),
    )


def make_result(
    capacity: int,
    load: float,
    entries: int,
    pattern: str,
    phase: str,
    delete_fraction: float,
    upstream: list[int],
    candidate: list[int],
) -> Result:
    old = probe_stats(upstream)
    new = probe_stats(candidate)
    return Result(
        capacity=capacity,
        bucket_count=next_prime(capacity),
        load=load,
        entries=entries,
        pattern=pattern,
        phase=phase,
        delete_fraction=delete_fraction,
        upstream_average=old.average,
        candidate_average=new.average,
        average_ratio=new.average / old.average,
        upstream_p95=old.p95,
        candidate_p95=new.p95,
        p95_ratio=new.p95 / old.p95,
        upstream_p99=old.p99,
        candidate_p99=new.p99,
        upstream_maximum=old.maximum,
        candidate_maximum=new.maximum,
    )


def run_once(
    keys: list[int],
    capacity: int,
    hash_function: HashFunction,
    delete_fraction: float,
    churn_cycles: int = 3,
) -> dict[str, list[int]]:
    table = LinearProbeMap(capacity, hash_function)
    measurements: dict[str, list[int]] = {}
    measurements["insert"] = [table.update(key) for key in keys]
    measurements["lookup-hit"] = [table.lookup(key) for key in keys]

    missing = replacement_keys(keys, len(keys), 0x13579BDF)
    measurements["lookup-miss"] = [table.lookup(key) for key in missing]

    delete_count = max(1, round(len(keys) * delete_fraction))
    active = list(keys)
    used = set(keys)
    measurements["delete"] = []
    measurements["reinsert"] = []
    for cycle in range(churn_cycles):
        step = max(1, len(active) // delete_count)
        deleted = active[cycle % step :: step][:delete_count]
        if len(deleted) < delete_count:
            deleted_set = set(deleted)
            deleted.extend(
                key for key in active if key not in deleted_set
            )
            deleted = deleted[:delete_count]
        deleted_set = set(deleted)
        active = [key for key in active if key not in deleted_set]
        measurements["delete"].extend(table.delete(key) for key in deleted)

        replacements = replacement_keys(
            used, len(deleted), 0x2468ACE0 + cycle * 0x10000
        )
        used.update(replacements)
        measurements["reinsert"].extend(
            table.update(key) for key in replacements
        )
        active.extend(replacements)
    measurements["post-churn-lookup"] = [table.lookup(key) for key in active]
    return measurements


def validate(
    capacities: list[int],
    loads: list[float],
    delete_fractions: list[float],
) -> list[Result]:
    results: list[Result] = []
    for capacity in capacities:
        for load in loads:
            entries = max(1, int(capacity * load))
            for pattern in PATTERNS:
                keys = make_keys(pattern, entries)
                for delete_fraction in delete_fractions:
                    upstream = run_once(
                        keys, capacity, upstream_hash, delete_fraction
                    )
                    candidate = run_once(
                        keys, capacity, candidate_hash, delete_fraction
                    )
                    for phase in upstream:
                        results.append(
                            make_result(
                                capacity,
                                load,
                                entries,
                                pattern,
                                phase,
                                delete_fraction,
                                upstream[phase],
                                candidate[phase],
                            )
                        )
    return results


def geometric_mean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def print_summary(results: list[Result]) -> None:
    print(
        "phase,cases,better,same,worse,median_ratio,geomean_ratio,p95_ratio,worst_ratio",
        file=sys.stderr,
    )
    for phase in sorted({result.phase for result in results}):
        ratios = [
            result.average_ratio for result in results if result.phase == phase
        ]
        better = sum(ratio < 0.98 for ratio in ratios)
        same = sum(0.98 <= ratio <= 1.02 for ratio in ratios)
        worse = sum(ratio > 1.02 for ratio in ratios)
        ratio_p95 = percentile(
            [round(ratio * 10000) for ratio in ratios], 0.95
        ) / 10000
        print(
            f"{phase},{len(ratios)},{better},{same},{worse},"
            f"{statistics.median(ratios):.3f},{geometric_mean(ratios):.3f},"
            f"{ratio_p95:.3f},{max(ratios):.3f}",
            file=sys.stderr,
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--format", choices=("csv", "json"), default="csv")
    parser.add_argument(
        "--quick", action="store_true", help="run a smaller smoke-test matrix"
    )
    parser.add_argument(
        "--max-regression",
        type=float,
        default=None,
        help="fail if any average-probe ratio exceeds this value",
    )
    args = parser.parse_args()

    if args.quick:
        capacities = [64, 1024]
        loads = [0.50, 0.97]
        delete_fractions = [0.10, 1.00]
    else:
        capacities = [64, 256, 1024, 4096]
        loads = [0.25, 0.50, 0.75, 0.90, 0.97]
        delete_fractions = [0.10, 0.50, 1.00]

    results = validate(capacities, loads, delete_fractions)
    rows = [asdict(result) for result in results]
    if args.format == "json":
        json.dump(rows, sys.stdout, indent=2)
        print()
    else:
        writer = csv.DictWriter(sys.stdout, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    print_summary(results)
    worst = max(results, key=lambda result: result.average_ratio)
    print(
        "worst average-probe regression: "
        f"{worst.average_ratio:.2f}x ({worst.phase}, {worst.pattern}, "
        f"capacity={worst.capacity}, load={worst.load:.2f}, "
        f"deleted={worst.delete_fraction:.0%})",
        file=sys.stderr,
    )
    if (
        args.max_regression is not None
        and worst.average_ratio > args.max_regression
    ):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
