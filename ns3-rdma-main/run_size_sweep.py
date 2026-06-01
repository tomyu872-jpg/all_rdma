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
            "Sweep flow count in config/1_flow.txt (line 1), run simulation in "
            "docker container cw-sim2, and aggregate 1_out_fct.txt outputs."
        )
    )
    parser.add_argument(
        "--container",
        default="cw-sim2",
        help="Docker container name (default: cw-sim2)",
    )
    parser.add_argument(
        "--flow-file",
        default="config/1_flow.txt",
        help="Flow config file path relative to repo root",
    )
    parser.add_argument(
        "--config-file",
        default="mix/output/1/config.txt",
        help="Config file used by waf --run",
    )
    parser.add_argument(
        "--fct-file",
        default="mix/output/1/1_out_fct.txt",
        help="FCT output file generated each run",
    )
    parser.add_argument(
        "--aggregate-file",
        default="mix/output/1/1_out_fct_all_sizes.txt",
        help="Aggregated output file",
    )
    parser.add_argument(
        "--container-workdir",
        default=None,
        help=(
            "Repo path inside container. If omitted, script auto-detects from docker mount info."
        ),
    )
    parser.add_argument(
        "--keep-flow-changes",
        action="store_true",
        help="Do not restore config/1_flow.txt to its original content after completion",
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
    host_repo_str = str(host_repo.resolve())

    candidates: List[Tuple[int, str]] = []
    for m in mounts:
        src = m.get("Source")
        dst = m.get("Destination")
        if not src or not dst:
            continue
        src_path = Path(src).resolve()
        try:
            rel = Path(host_repo_str).resolve().relative_to(src_path)
        except ValueError:
            continue
        candidates.append((len(str(src_path)), str(Path(dst) / rel)))

    if not candidates:
        raise RuntimeError(
            "Cannot map host repo path into container. Use --container-workdir explicitly."
        )

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def update_flow_count(flow_file: Path, flow_count: int) -> None:
    lines = flow_file.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise RuntimeError(f"Flow file is empty: {flow_file}")

    lines[0] = str(flow_count)
    flow_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_one(container: str, container_workdir: str, config_file: str) -> None:
    run_expr = f"./waf --run 'scratch/network-load-balance {config_file}' > mix/output/1/config.log 2>&1"
    shell_cmd = f"cd {shlex.quote(container_workdir)} && {run_expr}"
    run_cmd(["docker", "exec", container, "bash", "-lc", shell_cmd])


def main() -> None:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent
    flow_file = (repo_root / args.flow_file).resolve()
    fct_file = (repo_root / args.fct_file).resolve()
    aggregate_file = (repo_root / args.aggregate_file).resolve()

    if not flow_file.exists():
        raise FileNotFoundError(f"Flow file not found: {flow_file}")

    # Start container (or keep it running if already started).
    run_cmd(["docker", "start", args.container])

    container_workdir = get_container_workdir(args.container, repo_root, args.container_workdir)

    original_flow = flow_file.read_text(encoding="utf-8")
    aggregate_file.parent.mkdir(parents=True, exist_ok=True)

    try:
        with aggregate_file.open("w", encoding="utf-8") as agg:
            agg.write("# Aggregated FCT outputs by flow count\n")
            agg.write(f"# container={args.container}\n")
            agg.write(f"# container_workdir={container_workdir}\n\n")

            for flow_count in FLOW_COUNTS:
                print(f"[RUN] flow_count={flow_count}")
                update_flow_count(flow_file, flow_count)
                run_one(args.container, container_workdir, args.config_file)

                if not fct_file.exists():
                    raise FileNotFoundError(f"Expected FCT output file not found: {fct_file}")

                content = fct_file.read_text(encoding="utf-8")
                per_count_file = fct_file.with_name(f"1_out_fct_flows_{flow_count}.txt")
                per_count_file.write_text(content, encoding="utf-8")

                agg.write(f"===== flow_count={flow_count} =====\n")
                agg.write(content.rstrip("\n"))
                agg.write("\n\n")

        print(f"[DONE] Aggregated file: {aggregate_file}")

    finally:
        if not args.keep_flow_changes:
            flow_file.write_text(original_flow, encoding="utf-8")
            print(f"[RESTORE] Restored flow file: {flow_file}")


if __name__ == "__main__":
    main()
