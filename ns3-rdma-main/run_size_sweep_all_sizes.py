#!/usr/bin/env python3
import argparse
import json
import shlex
import subprocess
from pathlib import Path
from typing import List, Optional, Tuple

FLOW_COUNTS = list(range(1, 9))


def run_cmd(cmd: List[str], check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=check, text=True, capture_output=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Sweep k=1..8 for config/1_flow.txt and config/1_topology.txt, run "
            "network-load-balance in docker container, and aggregate 1_out_fct.txt."
        )
    )
    parser.add_argument(
        "--container",
        default="cw-sim2",
        help="Docker container name (default: cw-sim2)",
    )
    parser.add_argument(
        "--container-workdir",
        default=None,
        help=(
            "Repo path inside container. If omitted, auto-detect via docker mount info."
        ),
    )
    parser.add_argument(
        "--flow-file",
        default="config/1_flow.txt",
        help="Flow config file path relative to repo root",
    )
    parser.add_argument(
        "--topology-file",
        default="config/1_topology.txt",
        help="Topology config file path relative to repo root",
    )
    parser.add_argument(
        "--config-file",
        default="mix/output/1/config.txt",
        help="Config file passed to scratch/network-load-balance",
    )
    parser.add_argument(
        "--log-file",
        default="mix/output/1/config.log",
        help="Per-run simulator log file",
    )
    parser.add_argument(
        "--fct-file",
        default="mix/output/1/1_out_fct.txt",
        help="Per-run FCT output file",
    )
    parser.add_argument(
        "--aggregate-file",
        default="mix/output/1/1_out_fct_all_sizes.txt",
        help="Aggregated FCT file",
    )
    parser.add_argument(
        "--keep-config-changes",
        action="store_true",
        help="Do not restore flow/topology files after sweep",
    )
    return parser.parse_args()


def get_container_workdir(container: str, host_repo: Path, user_workdir: Optional[str]) -> str:
    if user_workdir:
        return user_workdir

    inspect = run_cmd(["docker", "inspect", container])
    info = json.loads(inspect.stdout)
    if not info:
        raise RuntimeError(f"Cannot inspect container: {container}")

    mounts = info[0].get("Mounts", [])
    host_repo_resolved = host_repo.resolve()

    candidates: List[Tuple[int, str]] = []
    for m in mounts:
        src = m.get("Source")
        dst = m.get("Destination")
        if not src or not dst:
            continue
        src_path = Path(src).resolve()
        try:
            rel = host_repo_resolved.relative_to(src_path)
        except ValueError:
            continue
        candidates.append((len(str(src_path)), str(Path(dst) / rel)))

    if not candidates:
        raise RuntimeError(
            "Cannot map host repo path into container. Use --container-workdir explicitly."
        )

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def ensure_container_started(container: str) -> None:
    # Non-interactive equivalent for scripted runs.
    run_cmd(["docker", "start", container], check=True)


def set_flow_count(flow_file: Path, k: int) -> None:
    lines = flow_file.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise RuntimeError(f"Flow file is empty: {flow_file}")
    lines[0] = str(k)
    flow_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _format_rate_gbps(k: int) -> str:
    rate = 100.0 / float(k)
    text = f"{rate:.6f}".rstrip("0").rstrip(".")
    return f"{text}Gbps"


def set_topology_rate(topology_file: Path, k: int) -> None:
    lines = topology_file.read_text(encoding="utf-8").splitlines()
    start = 2  # line 3 in 1-based indexing
    end = start + k
    if len(lines) < end:
        raise RuntimeError(
            f"Topology file has only {len(lines)} lines, cannot update lines 3..{2 + k}"
        )

    rate_str = _format_rate_gbps(k)
    for i in range(start, end):
        parts = lines[i].split()
        if len(parts) < 3:
            raise RuntimeError(f"Topology line {i + 1} has fewer than 3 columns: {lines[i]}")
        parts[2] = rate_str
        lines[i] = " ".join(parts)

    topology_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_experiment(container: str, container_workdir: str, config_file: str, log_file: str) -> None:
    run_expr = (
        f"./waf --run 'scratch/network-load-balance {config_file}'"
        f" > {log_file} 2>&1"
    )
    shell_cmd = f"cd {shlex.quote(container_workdir)} && {run_expr}"
    run_cmd(["docker", "exec", container, "bash", "-lc", shell_cmd], check=True)


def main() -> None:
    args = parse_args()

    repo_root = Path(__file__).resolve().parent
    flow_file = (repo_root / args.flow_file).resolve()
    topology_file = (repo_root / args.topology_file).resolve()
    fct_file = (repo_root / args.fct_file).resolve()
    aggregate_file = (repo_root / args.aggregate_file).resolve()

    if not flow_file.exists():
        raise FileNotFoundError(f"Flow file not found: {flow_file}")
    if not topology_file.exists():
        raise FileNotFoundError(f"Topology file not found: {topology_file}")

    ensure_container_started(args.container)
    container_workdir = get_container_workdir(args.container, repo_root, args.container_workdir)

    original_flow = flow_file.read_text(encoding="utf-8")
    original_topology = topology_file.read_text(encoding="utf-8")

    aggregate_file.parent.mkdir(parents=True, exist_ok=True)

    try:
        with aggregate_file.open("w", encoding="utf-8") as agg:
            agg.write("# Aggregated FCT outputs by flow count and topology rate\n")
            agg.write(f"# container={args.container}\n")
            agg.write(f"# container_workdir={container_workdir}\n")
            agg.write(f"# command=./waf --run 'scratch/network-load-balance {args.config_file}'\n\n")

            for k in FLOW_COUNTS:
                rate_str = _format_rate_gbps(k)
                print(f"[RUN] k={k}, first {k} topology links rate={rate_str}")

                set_flow_count(flow_file, k)
                set_topology_rate(topology_file, k)

                run_experiment(
                    args.container,
                    container_workdir,
                    args.config_file,
                    args.log_file,
                )

                if not fct_file.exists():
                    raise FileNotFoundError(f"Expected FCT file not found: {fct_file}")

                fct_content = fct_file.read_text(encoding="utf-8")
                per_k_file = fct_file.with_name(f"1_out_fct_k{k}.txt")
                per_k_file.write_text(fct_content, encoding="utf-8")

                agg.write(f"===== k={k}, rate={rate_str} =====\n")
                agg.write(fct_content.rstrip("\n"))
                agg.write("\n\n")

        print(f"[DONE] Aggregate output: {aggregate_file}")

    finally:
        if not args.keep_config_changes:
            flow_file.write_text(original_flow, encoding="utf-8")
            topology_file.write_text(original_topology, encoding="utf-8")
            print(f"[RESTORE] Restored: {flow_file}")
            print(f"[RESTORE] Restored: {topology_file}")


if __name__ == "__main__":
    main()
