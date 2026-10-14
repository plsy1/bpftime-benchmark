#!/usr/bin/env python3
"""Track official bpftime on Jetson and preserve reproducible uprobe results."""

from __future__ import annotations

import argparse
import datetime as dt
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


HERE = Path(__file__).resolve().parent
DEFAULT_CONFIG = HERE / "config.json"
RESULT_FILES = (
    "benchmark-output.json",
    "benchmark-logs.txt",
    "benchmark-logs-error.txt",
    "results.md",
)
MAP_CASES = (
    "__bench_array_map_lookup",
    "__bench_array_map_update",
    "__bench_hash_map_lookup",
    "__bench_hash_map_update",
    "__bench_hash_map_delete",
    "__bench_per_cpu_array_map_lookup",
    "__bench_per_cpu_array_map_update",
    "__bench_per_cpu_hash_map_lookup",
    "__bench_per_cpu_hash_map_update",
    "__bench_per_cpu_hash_map_delete",
)


class TrackerError(RuntimeError):
    pass


def now() -> str:
    return dt.datetime.now().astimezone().isoformat(timespec="seconds")


def run(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    check: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(argv), flush=True)
    return subprocess.run(
        argv,
        cwd=cwd,
        env=env,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def git(repo: Path, *args: str, check: bool = True) -> str:
    result = run(["git", *args], cwd=repo, check=check, capture=True)
    return result.stdout.strip()


def atomic_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def load_config(path: Path) -> tuple[dict[str, Any], Path]:
    config = json.loads(path.read_text())
    repo = Path(git(path.parent, "rev-parse", "--show-toplevel"))
    replacements = {
        "repo": str(repo),
        "repo_parent": str(repo.parent),
    }
    for key, value in config["paths"].items():
        config["paths"][key] = value.format(**replacements)
    return config, repo


def state_paths(config: dict[str, Any]) -> tuple[Path, Path]:
    state_root = Path(config["paths"]["state_root"])
    results_root = Path(config["paths"]["results_root"])
    state_root.mkdir(parents=True, exist_ok=True)
    results_root.mkdir(parents=True, exist_ok=True)
    return state_root, results_root


def ensure_remote(repo: Path, name: str, url: str) -> None:
    current = git(repo, "remote", "get-url", name, check=False)
    if not current:
        run(["git", "remote", "add", name, url], cwd=repo)
    elif current != url:
        raise TrackerError(f"remote {name!r} is {current!r}, expected {url!r}")


def checked_out_branches(repo: Path) -> set[str]:
    output = git(repo, "worktree", "list", "--porcelain")
    return {
        line.removeprefix("branch ")
        for line in output.splitlines()
        if line.startswith("branch ")
    }


def sync_official(config: dict[str, Any], repo: Path, push: bool) -> str:
    official = config["official"]
    remote = official["remote"]
    branch = official["branch"]
    mirror = official["mirror_branch"]
    ensure_remote(repo, remote, official["url"])
    run(
        [
            "git",
            "fetch",
            "--prune",
            remote,
            f"+refs/heads/{branch}:refs/remotes/{remote}/{branch}",
        ],
        cwd=repo,
    )
    new_sha = git(repo, "rev-parse", f"refs/remotes/{remote}/{branch}")
    old_sha = git(repo, "rev-parse", "--verify", f"refs/heads/{mirror}", check=False)
    if old_sha and old_sha != new_sha:
        ff = run(
            ["git", "merge-base", "--is-ancestor", old_sha, new_sha],
            cwd=repo,
            check=False,
            capture=True,
        )
        if ff.returncode != 0:
            raise TrackerError(
                f"official history is not a fast-forward: {old_sha[:12]} -> {new_sha[:12]}"
            )
    full_ref = f"refs/heads/{mirror}"
    if full_ref in checked_out_branches(repo):
        raise TrackerError(f"{mirror} is checked out in a worktree; refusing to move it")
    update = ["git", "update-ref", full_ref, new_sha]
    if old_sha:
        update.append(old_sha)
    run(update, cwd=repo)
    if push and official.get("push_mirror", False):
        run(
            [
                "git",
                "push",
                official["push_remote"],
                f"{full_ref}:{full_ref}",
            ],
            cwd=repo,
        )
    return new_sha


def worktree_path(state_root: Path, sha: str, role: str) -> Path:
    return state_root / "worktrees" / sha[:12] / role


def ensure_worktree(repo: Path, path: Path, revision: str, branch: str | None = None) -> None:
    if path.exists():
        actual = git(path, "rev-parse", "HEAD", check=False)
        if not actual:
            raise TrackerError(f"existing path is not a worktree: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    argv = ["git", "worktree", "add"]
    if branch:
        existing = git(repo, "rev-parse", "--verify", f"refs/heads/{branch}", check=False)
        argv.extend([path.as_posix(), branch] if existing else ["-b", branch, path.as_posix(), revision])
    else:
        argv.extend(["--detach", path.as_posix(), revision])
    run(argv, cwd=repo)


def write_migration_status(state_root: Path, sha: str, value: dict[str, Any]) -> None:
    value["updated"] = now()
    atomic_json(state_root / "migrations" / f"{sha}.json", value)


def migrate(config: dict[str, Any], repo: Path, official_sha: str) -> Path:
    state_root, _ = state_paths(config)
    prefix = config["candidate"]["branch_prefix"]
    branch = f"{prefix}-{official_sha[:12]}"
    path = worktree_path(state_root, official_sha, "candidate")
    ensure_worktree(repo, path, official_sha, branch)
    completed = set(git(path, "log", "--format=%B").splitlines())
    applied: list[dict[str, str]] = []
    for patch in config["candidate"]["commits"]:
        sha = patch["sha"]
        marker = f"(cherry picked from commit {sha})"
        if marker in completed:
            applied.append(patch)
            continue
        if git(path, "rev-parse", "--verify", "CHERRY_PICK_HEAD", check=False):
            conflicts = git(path, "diff", "--name-only", "--diff-filter=U").splitlines()
            write_migration_status(
                state_root,
                official_sha,
                {
                    "status": "conflict",
                    "official_sha": official_sha,
                    "candidate_branch": branch,
                    "candidate_worktree": str(path),
                    "patch": patch,
                    "conflicts": conflicts,
                    "applied": applied,
                },
            )
            raise TrackerError(f"unfinished cherry-pick in {path}; conflicts: {conflicts}")
        result = run(["git", "cherry-pick", "-x", sha], cwd=path, check=False, capture=True)
        if result.returncode != 0:
            conflicts = git(path, "diff", "--name-only", "--diff-filter=U").splitlines()
            write_migration_status(
                state_root,
                official_sha,
                {
                    "status": "conflict",
                    "official_sha": official_sha,
                    "candidate_branch": branch,
                    "candidate_worktree": str(path),
                    "patch": patch,
                    "conflicts": conflicts,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                    "applied": applied,
                },
            )
            raise TrackerError(
                f"migration stopped at {sha[:12]} ({patch['subject']}); "
                f"worktree preserved at {path}"
            )
        applied.append(patch)
    write_migration_status(
        state_root,
        official_sha,
        {
            "status": "success",
            "official_sha": official_sha,
            "candidate_sha": git(path, "rev-parse", "HEAD"),
            "candidate_branch": branch,
            "candidate_worktree": str(path),
            "applied": applied,
        },
    )
    return path


def initialize_submodules(path: Path, jobs: int) -> None:
    run(["git", "submodule", "sync", "--recursive"], cwd=path)
    run(
        ["git", "submodule", "update", "--init", "--recursive", "--jobs", str(jobs)],
        cwd=path,
    )


def build(config: dict[str, Any], path: Path) -> Path:
    build_cfg = config["build"]
    jobs = int(build_cfg["jobs"])
    initialize_submodules(path, jobs)
    build_dir = path / "build-jetson-daily"
    run(
        ["cmake", "-S", str(path), "-B", str(build_dir), *build_cfg["cmake_args"]],
        cwd=path,
    )
    run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--parallel",
            str(jobs),
            "--target",
            *build_cfg["targets"],
        ],
        cwd=path,
    )
    run(["make", "-C", "benchmark", "test"], cwd=path)
    run(["make", "-C", "benchmark/uprobe", "uprobe"], cwd=path)
    required = (
        build_dir / "runtime/agent/libbpftime-agent.so",
        build_dir / "runtime/syscall-server/libbpftime-syscall-server.so",
        build_dir / "benchmark/simple-benchmark-with-embed-ebpf-calling",
        build_dir / "runtime/unit-test/bpftime_runtime_tests",
        path / "benchmark/test",
        path / "benchmark/uprobe/uprobe",
        path / "benchmark/uprobe/.output/uprobe.bpf.o",
    )
    missing = [str(item) for item in required if not item.exists()]
    if missing:
        raise TrackerError(f"build completed but required artifacts are missing: {missing}")
    return build_dir


def test_runtime(path: Path, build_dir: Path) -> None:
    test_binary = build_dir / "runtime/unit-test/bpftime_runtime_tests"
    run([str(test_binary)], cwd=path)


def cpu_frequency(cpu: int) -> dict[str, int | None]:
    root = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq")
    values: dict[str, int | None] = {}
    for key in ("scaling_min_freq", "scaling_max_freq", "scaling_cur_freq"):
        try:
            values[key] = int((root / key).read_text().strip())
        except (FileNotFoundError, ValueError):
            values[key] = None
    return values


def validate_result(path: Path, expected_runs: int) -> dict[str, Any]:
    data = json.loads(path.read_text())
    counts = {
        "kernel": len(data.get("kernel_uprobe", {})),
        "userspace": len(data.get("userspace_uprobe", {})),
        "embed": len(data.get("embed", {})),
    }
    bad: list[str] = []
    for group in ("kernel_uprobe", "userspace_uprobe", "embed"):
        for name, result in data.get(group, {}).items():
            if not isinstance(result, dict) or len(result.get("raw_values", [])) != expected_runs:
                bad.append(f"{group}.{name}")
    if bad:
        raise TrackerError(f"result has incomplete raw samples: {bad}")
    return {"counts": counts, "expected_runs": expected_runs, "incomplete": bad}


def result_summary(data: dict[str, Any], official_sha: str) -> dict[str, Any]:
    kernel = data["kernel_uprobe"]
    userspace = data["userspace_uprobe"]
    operations: dict[str, Any] = {}
    for case in MAP_CASES:
        if case in kernel and case in userspace:
            k = kernel[case]["avg"]
            u = userspace[case]["avg"]
            operations[case] = {"kernel": k, "userspace": u, "ratio": u / k}
    return {
        "timestamp": now(),
        "official_sha": official_sha,
        "uprobe": {
            "kernel": kernel.get("__bench_uprobe", {}).get("avg"),
            "userspace": userspace.get("__bench_uprobe", {}).get("avg"),
        },
        "operations": operations,
    }


def benchmark(config: dict[str, Any], path: Path, build_dir: Path, official_sha: str) -> Path:
    _, results_root = state_paths(config)
    bench_cfg = config["benchmark"]
    cpu = int(bench_cfg["cpu"])
    runs = int(bench_cfg["runs"])
    run(["sudo", "-n", "true"], cwd=path)
    run(["sudo", "-n", "jetson_clocks"], cwd=path)
    before = cpu_frequency(cpu)
    output_dir = results_root / f"{dt.datetime.now().astimezone():%Y%m%d-%H%M%S}-{official_sha[:12]}"
    output_dir.mkdir(parents=True, exist_ok=False)
    benchmark_dir = path / "benchmark/uprobe"
    for name in RESULT_FILES:
        generated = benchmark_dir / name
        if generated.exists():
            generated.unlink()
    environment = os.environ.copy()
    environment["BPFTIME_BUILD_DIR"] = str(build_dir)
    environment["BPFTIME_LOG_OUTPUT"] = "console"
    command = [
        "taskset",
        "-c",
        str(cpu),
        sys.executable,
        "benchmark/uprobe/benchmark.py",
        "--iter",
        str(runs),
    ]
    log_path = output_dir / "runner.log"
    print("+", " ".join(command), flush=True)
    with log_path.open("w") as log:
        result = subprocess.run(
            command,
            cwd=path,
            env=environment,
            text=True,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
    for name in RESULT_FILES:
        generated = benchmark_dir / name
        if generated.exists():
            shutil.copy2(generated, output_dir / name)
    if result.returncode != 0:
        atomic_json(
            output_dir / "metadata.json",
            {
                "status": "benchmark-failed",
                "official_sha": official_sha,
                "returncode": result.returncode,
                "frequency_before": before,
                "frequency_after": cpu_frequency(cpu),
                "finished": now(),
            },
        )
        raise TrackerError(f"benchmark failed; see {log_path}")
    result_json = output_dir / "benchmark-output.json"
    validation = validate_result(result_json, runs)
    data = json.loads(result_json.read_text())
    summary = result_summary(data, official_sha)
    after = cpu_frequency(cpu)
    metadata = {
        "status": "success",
        "official_sha": official_sha,
        "worktree": str(path),
        "build_dir": str(build_dir),
        "cpu": cpu,
        "frequency_before": before,
        "frequency_after": after,
        "validation": validation,
        "finished": now(),
    }
    atomic_json(output_dir / "metadata.json", metadata)
    atomic_json(output_dir / "summary.json", summary)
    atomic_json(results_root / "latest.json", {"result_dir": str(output_dir), **summary})
    with (results_root / "history.jsonl").open("a") as history:
        history.write(json.dumps({"result_dir": str(output_dir), **summary}, sort_keys=True) + "\n")
    return output_dir


def last_success_sha(results_root: Path) -> str | None:
    latest = results_root / "latest.json"
    if not latest.exists():
        return None
    return json.loads(latest.read_text()).get("official_sha")


def daily(config: dict[str, Any], repo: Path, *, push: bool, force: bool) -> dict[str, Any]:
    state_root, results_root = state_paths(config)
    official_sha = sync_official(config, repo, push)
    status: dict[str, Any] = {"official_sha": official_sha, "started": now()}
    if (
        config["benchmark"].get("skip_unchanged_sha", True)
        and not force
        and last_success_sha(results_root) == official_sha
    ):
        status.update({"status": "unchanged", "finished": now()})
        atomic_json(state_root / "last-run.json", status)
        return status
    official_path = worktree_path(state_root, official_sha, "official")
    ensure_worktree(repo, official_path, official_sha)
    build_dir = build(config, official_path)
    test_runtime(official_path, build_dir)
    result_dir = benchmark(config, official_path, build_dir, official_sha)
    status["official_result"] = str(result_dir)
    try:
        candidate_path = migrate(config, repo, official_sha)
        status["candidate_worktree"] = str(candidate_path)
        status["migration"] = "success"
    except TrackerError as error:
        status["migration"] = "needs-attention"
        status["migration_error"] = str(error)
    status.update({"status": "success", "finished": now()})
    atomic_json(state_root / "last-run.json", status)
    return status


def print_status(config: dict[str, Any], repo: Path) -> None:
    state_root, results_root = state_paths(config)
    official = config["official"]
    remote_ref = f"refs/remotes/{official['remote']}/{official['branch']}"
    mirror_ref = f"refs/heads/{official['mirror_branch']}"
    value = {
        "repo": str(repo),
        "official_remote": git(repo, "rev-parse", "--verify", remote_ref, check=False) or None,
        "official_mirror": git(repo, "rev-parse", "--verify", mirror_ref, check=False) or None,
        "last_run": json.loads((state_root / "last-run.json").read_text())
        if (state_root / "last-run.json").exists()
        else None,
        "latest_result": json.loads((results_root / "latest.json").read_text())
        if (results_root / "latest.json").exists()
        else None,
    }
    print(json.dumps(value, indent=2, sort_keys=True))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    subparsers = parser.add_subparsers(dest="command", required=True)
    sync_parser = subparsers.add_parser("sync", help="update the exact official mirror branch")
    sync_parser.add_argument("--push", action="store_true")
    subparsers.add_parser("status", help="show mirror, migration, and result status")
    migrate_parser = subparsers.add_parser("migrate", help="apply the configured patch stack")
    migrate_parser.add_argument("--official-sha")
    build_parser = subparsers.add_parser("build", help="build an existing worktree")
    build_parser.add_argument("worktree", type=Path)
    daily_parser = subparsers.add_parser("daily", help="sync, build, test, benchmark, then migrate")
    daily_parser.add_argument("--push", action="store_true")
    daily_parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    config, repo = load_config(args.config.resolve())
    state_root, _ = state_paths(config)
    lock_path = state_root / "tracker.lock"
    with lock_path.open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise TrackerError("another tracker process is already running") from error
        if args.command == "sync":
            print(sync_official(config, repo, args.push))
        elif args.command == "status":
            print_status(config, repo)
        elif args.command == "migrate":
            official_sha = args.official_sha or git(
                repo, "rev-parse", f"refs/heads/{config['official']['mirror_branch']}"
            )
            print(migrate(config, repo, official_sha))
        elif args.command == "build":
            print(build(config, args.worktree.resolve()))
        elif args.command == "daily":
            print(json.dumps(daily(config, repo, push=args.push, force=args.force), indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (TrackerError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
