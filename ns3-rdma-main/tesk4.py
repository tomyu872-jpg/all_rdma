#!/usr/bin/env python3
import argparse
import json
import shlex
import subprocess
from pathlib import Path
from typing import List, Optional, Tuple

SIZES = [
    16384,
    40960,
    204800,
    614400,
    1024000,
    2048000,
    5120000,
    12288000,
    25600000,
    40960000,
]


def run_cmd(cmd: List[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, text=True, capture_output=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run experiment 4: set column-4 size in config/1_flow.txt for multiple values, "
            "run network-load-balance in docker container, and aggregate line 4 of 1_out_fct.txt."
        )
    )
    parser.add_argument("--container", default="cw-sim2", help="Docker container name")
    parser.add_argument(
        "--container-workdir",
        default=None,
        help="Repo path inside container. Auto-detected if omitted.",
    )
    parser.add_argument(
        "--flow-file",
        default="config/1_flow.txt",
        help="Flow file path relative to repo root",
    )
    parser.add_argument(
        "--config-file",
        default="mix/output/1/config.txt",
        help="Config file for waf run",
    )
    parser.add_argument(
        "--log-file",
        default="mix/output/1/config.log",
        help="Log file for waf run",
    )
    parser.add_argument(
        "--fct-file",
        default="mix/output/1/1_out_fct.txt",
        help="Per-run FCT output file",
    )
    parser.add_argument(
        "--summary-file",
        default="mix/output/1/1_out_fct_line4_by_size.txt",
        help="Summary output file (size + line 4 of fct)",
    )
    parser.add_argument(
        "--keep-flow-changes",
        action="store_true",
        help="Do not restore flow file at the end",
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
    for mount in mounts:
        src = mount.get("Source")
        dst = mount.get("Destination")
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
            "Cannot map host repo path into container. Please pass --container-workdir explicitly."
        )

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def update_flow_file_column4(flow_file: Path, size_value: int) -> None:
    lines = flow_file.read_text(encoding="utf-8").splitlines()
    if len(lines) < 2:
        raise RuntimeError(f"Flow file has fewer than 2 lines: {flow_file}")

    updated = [lines[0]]
    for idx, line in enumerate(lines[1:], start=2):
        raw = line.strip()
        if not raw:
            updated.append(line)
            continue
        if raw.startswith("#"):
            updated.append(line)
            continue

        cols = raw.split()
        if len(cols) < 4:
            raise RuntimeError(
                f"Line {idx} in {flow_file} has fewer than 4 columns: {line}"
            )
        cols[3] = str(size_value)
        updated.append(" ".join(cols))

    flow_file.write_text("\n".join(updated) + "\n", encoding="utf-8")


def run_simulation(container: str, container_workdir: str, config_file: str, log_file: str) -> None:
    run_expr = (
        f"./waf --run 'scratch/network-load-balance {config_file}'"
        f" > {log_file} 2>&1"
    )
    shell_cmd = f"cd {shlex.quote(container_workdir)} && {run_expr}"
    run_cmd(["docker", "exec", container, "bash", "-lc", shell_cmd])


def get_line4(file_path: Path) -> str:
    lines = file_path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 4:
        return "<MISSING_LINE_4>"
    return lines[3]


def get_line3_col7(file_path: Path) -> str:
    lines = file_path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 3:
        return "<MISSING_LINE_3>"

    columns = lines[2].split()
    if len(columns) < 7:
        return "<MISSING_COL_7>"

    return columns[6]


def main() -> None:
    args = parse_args()

    repo_root = Path(__file__).resolve().parent
    flow_file = (repo_root / args.flow_file).resolve()
    fct_file = (repo_root / args.fct_file).resolve()
    summary_file = (repo_root / args.summary_file).resolve()

    if not flow_file.exists():
        raise FileNotFoundError(f"Flow file not found: {flow_file}")

    # For scripted automation, start container in non-interactive mode.
    run_cmd(["docker", "start", args.container])
    container_workdir = get_container_workdir(args.container, repo_root, args.container_workdir)

    original_flow = flow_file.read_text(encoding="utf-8")
    summary_file.parent.mkdir(parents=True, exist_ok=True)

    try:
        with summary_file.open("w", encoding="utf-8") as out:
            out.write("size\tline4_from_1_out_fct.txt\n")

            for size in SIZES:
                print(f"[RUN] size={size}")
                update_flow_file_column4(flow_file, size)

                run_simulation(
                    args.container,
                    container_workdir,
                    args.config_file,
                    args.log_file,
                )

                if not fct_file.exists():
                    raise FileNotFoundError(f"Expected FCT file not found: {fct_file}")

                line4 = get_line4(fct_file)
                line3_col7 = get_line3_col7(fct_file)
                out.write("\t".join(line4.split()) + "\t" + line3_col7 + "\n")

        print(f"[DONE] Summary file generated: {summary_file}")

    finally:
        if not args.keep_flow_changes:
            flow_file.write_text(original_flow, encoding="utf-8")
            print(f"[RESTORE] Restored flow file: {flow_file}")


if __name__ == "__main__":
    main()
