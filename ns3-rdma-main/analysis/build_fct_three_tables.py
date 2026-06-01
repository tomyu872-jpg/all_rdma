#!/usr/bin/env python3
import argparse
import re
from collections import OrderedDict
from pathlib import Path
from typing import Dict, List, Tuple

SECTION_HEADER_RE = re.compile(
    r"^=====\s*(?:size|flow_count|k)\s*=\s*(\d+)(?:\s*,.*)?\s*=====$"
)


def parse_file(path: Path) -> Tuple[List[int], List[str], Dict[int, Dict[str, Dict[str, str]]]]:
    sizes: List[int] = []
    flow_order: "OrderedDict[str, None]" = OrderedDict()
    data: Dict[int, Dict[str, Dict[str, str]]] = {}

    current_size = None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue

        m = SECTION_HEADER_RE.match(line)
        if m:
            current_size = int(m.group(1))
            if current_size not in data:
                data[current_size] = {}
                sizes.append(current_size)
            continue

        if current_size is None:
            continue

        parts = line.split()
        if len(parts) < 10:
            continue

        src = parts[0]
        dst = parts[1]
        flow_id = f"{src}->{dst}"
        if flow_id not in flow_order:
            flow_order[flow_id] = None

        # 用户指定：第8个值、第7个值、第10个值（1-based）
        tail_latency = parts[7]
        avg_fct = parts[6]
        avg_throughput = parts[9]

        data[current_size][flow_id] = {
            "tail_latency": tail_latency,
            "avg_fct": avg_fct,
            "avg_throughput": avg_throughput,
        }

    return sizes, list(flow_order.keys()), data


def render_table(
    title: str,
    key: str,
    sizes: List[int],
    flows: List[str],
    data: Dict[int, Dict[str, Dict[str, str]]],
) -> str:
    lines: List[str] = []
    lines.append(title)
    lines.append("flow\\section\t" + "\t".join(str(s) for s in sizes))

    for flow in flows:
        row = [flow]
        for size in sizes:
            v = "0"
            if size in data and flow in data[size]:
                v = data[size][flow].get(key, "0")
            row.append(v)
        lines.append("\t".join(row))

    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Parse 1_out_fct_all_sizes.txt and output one fct text file containing "
            "three tables: tail latency(col8), average fct(col7), average throughput(col10)."
        )
    )
    parser.add_argument(
        "--input",
        default="mix/output/1/1_out_fct_all_sizes.txt",
        help="Input aggregated fct file",
    )
    parser.add_argument(
        "--output",
        default="mix/output/1/1_out_fct_three_tables.txt",
        help="Output text file with 3 tables",
    )
    args = parser.parse_args()

    input_path = Path(args.input).resolve()
    output_path = Path(args.output).resolve()

    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    sizes, flows, data = parse_file(input_path)
    if not sizes:
        raise RuntimeError("No sections found in input file")

    sections = [
        render_table("[Table 1] Tail Latency (8th value)", "tail_latency", sizes, flows, data),
        render_table("[Table 2] Average FCT (7th value)", "avg_fct", sizes, flows, data),
        render_table(
            "[Table 3] Average Throughput (10th value)",
            "avg_throughput",
            sizes,
            flows,
            data,
        ),
    ]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n\n".join(sections) + "\n", encoding="utf-8")
    print(f"Output written: {output_path}")


if __name__ == "__main__":
    main()
