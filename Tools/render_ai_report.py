"""
render_ai_report.py — AI triage prompt generator for render regression reports.

Usage:
  python Tools/render_ai_report.py --report out/CluicheTest/captures/reports/tag_report.json [--prompt-out out/CluicheTest/captures/reports/]

Reads a JSON report and writes <tag>_ai_prompt.txt — a compact prompt for
manual pasting into an AI conversation. No API key required.

Expected report schema (from CaptureReportWriter):
  tag, frame, backend, commit, pass,
  diff.total_pixels, diff.differing_pixels, diff.differing_pct,
  diff.max_delta, diff.threshold, diff.regions
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _format_regions(regions: list, max_regions: int = 5) -> str:
    """Format top differing regions as a compact string."""
    if not regions:
        return "none"
    top = regions[:max_regions]
    parts = []
    for r in top:
        row = r.get("row", "?")
        col = r.get("col", "?")
        pct = r.get("diff_pct", r.get("pct", "?"))
        delta = r.get("max_delta", r.get("delta", "?"))
        parts.append(f"[{row},{col}: {pct}% diff, delta {delta}]")
    return ", ".join(parts)


def generate_prompt(report: dict) -> str:
    """Build the compact AI triage prompt string from the report dict."""
    tag = report.get("tag", "unknown")
    frame = report.get("frame", "?")
    backend = report.get("backend", "?")
    commit = report.get("commit", "?")
    passed = report.get("pass", report.get("ssim_pass", "?"))

    diff = report.get("diff", {})
    # Support both CaptureReportWriter schema and render_diff schema
    total_pixels = diff.get("total_pixels", report.get("total_pixels", "?"))
    differing_pixels = diff.get("differing_pixels", report.get("pixel_diff_count", "?"))
    differing_pct = diff.get("differing_pct", report.get("diff_pct", "?"))
    max_delta = diff.get("max_delta", "?")
    threshold = diff.get("threshold", report.get("threshold", "?"))
    regions = diff.get("regions", [])

    regions_str = _format_regions(regions)

    lines = [
        f"Render regression report for '{tag}' (frame {frame}, backend {backend}, commit {commit}):",
        f"- pass: {passed}",
        f"- total pixels: {total_pixels}, differing: {differing_pixels} ({differing_pct}%), max delta: {max_delta}, threshold: {threshold}",
        f"- Top differing regions: {regions_str}",
        "",
        "Is this regression a genuine visual defect or expected variation?",
        "Provide: verdict (PASS/FAIL/UNCERTAIN), confidence (0-1), and a 1-sentence reason.",
    ]
    return "\n".join(lines)


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "AI triage prompt generator for render regression reports.\n"
            "Reads a JSON report and writes <tag>_ai_prompt.txt.\n"
            "No API key required — paste the output into any AI conversation."
        )
    )
    parser.add_argument(
        "--report", required=True, metavar="PATH",
        help="Path to the JSON report file."
    )
    parser.add_argument(
        "--prompt-out", default=None, metavar="DIR",
        help="Directory to write the prompt file. Default: same directory as --report."
    )
    return parser


if __name__ == "__main__":
    parser = _build_arg_parser()
    args = parser.parse_args()

    report_path = Path(args.report)
    if not report_path.exists():
        print(f"[render_ai_report] ERROR: report not found: {report_path}", file=sys.stderr)
        sys.exit(1)

    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"[render_ai_report] ERROR: invalid JSON in {report_path}: {e}", file=sys.stderr)
        sys.exit(1)

    out_dir = Path(args.prompt_out) if args.prompt_out else report_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    tag = report.get("tag", report_path.stem)
    prompt_text = generate_prompt(report)

    out_path = out_dir / f"{tag}_ai_prompt.txt"
    out_path.write_text(prompt_text, encoding="utf-8")

    print(f"Prompt written to {out_path}")
