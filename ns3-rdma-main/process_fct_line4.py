#!/usr/bin/env python3
import argparse
from pathlib import Path

SCALE = 8 * 8192-48


def format_number(value: float) -> str:
    rounded = round(value)
    if abs(value - rounded) < 1e-12:
        return str(int(rounded))
    return f"{value:.6f}".rstrip("0").rstrip(".")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Process 1_out_fct_line4_by_size.txt from line 2 onward. "
            "For each line, compute (col1..col4) * 8 * 8192 / col5 and write 4 values to a new file."
        )
    )
    parser.add_argument(
        "--input",
        default="mix/output/1/1_out_fct_line4_by_size.txt",
        help="Input file path",
    )
    parser.add_argument(
        "--output",
        default="mix/output/1/1_out_fct_line4_by_size_converted.txt",
        help="Output file path",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    input_path = Path(args.input)
    output_path = Path(args.output)

    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    lines = input_path.read_text(encoding="utf-8").splitlines()
    if len(lines) < 2:
        raise RuntimeError(f"Input file has fewer than 2 lines: {input_path}")

    output_lines = ["v1\tv2\tv3\tv4"]

    for line_no, line in enumerate(lines[1:], start=2):
        text = line.strip()
        if not text:
            continue

        cols = text.split()
        if len(cols) < 5:
            raise RuntimeError(
                f"Line {line_no} has fewer than 5 values: {line}"
            )

        values = [float(cols[i]) for i in range(5)]
        denominator = values[4]
        if denominator == 0:
            raise ZeroDivisionError(f"Line {line_no} has col5 = 0")

        transformed = [(values[i] * SCALE) / denominator for i in range(4)]
        output_lines.append("\t".join(format_number(v) for v in transformed))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(output_lines) + "\n", encoding="utf-8")

    print(f"Done. Wrote {len(output_lines) - 1} rows to: {output_path}")


if __name__ == "__main__":
    main()
