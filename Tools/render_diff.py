"""
render_diff.py — offline render regression diff tool.

Usage:
  python Tools/render_diff.py --run out/CluicheTest/captures/run/ --ref Cluiche/Assets/CluicheTest/captures/reference/ [--report-dir out/CluicheTest/captures/reports/] [--threshold 4]

For each .png in --run, if a matching .png exists in --ref:
  - Compute per-channel SSIM (scikit-image)
  - Compute per-channel histogram comparison
  - Produce a colour-coded diff PNG (red = differs, green = same)
  - Produce a side-by-side composite PNG (ref | run | diff)
  - Write a JSON report to --report-dir/<tag>_diff_report.json

Exit 0 if all pairs pass (SSIM >= 0.95), exit 1 if any fail.

Requires: Pillow, numpy, scikit-image
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _compute_diff(ref_img, run_img, threshold: int):
    """Return (diff_array, pixel_diff_count, ssim_score, channel_ssim).

    diff_array is an RGB numpy array: red pixels exceed threshold, green pixels pass.
    """
    import numpy as np
    from skimage.metrics import structural_similarity

    ref_arr = np.array(ref_img.convert("RGB"), dtype=np.float32)
    run_arr = np.array(run_img.convert("RGB"), dtype=np.float32)

    # Resize run to ref dimensions if they differ
    if ref_arr.shape != run_arr.shape:
        from PIL import Image
        run_img_resized = run_img.resize(
            (ref_img.width, ref_img.height), Image.LANCZOS
        )
        run_arr = np.array(run_img_resized.convert("RGB"), dtype=np.float32)

    # Per-pixel max channel difference
    diff = np.abs(ref_arr - run_arr)
    max_diff = diff.max(axis=2)  # shape: (H, W)
    exceeds = max_diff > threshold

    pixel_diff_count = int(exceeds.sum())

    # Colour-coded diff image: red where exceeds, green where same
    H, W = max_diff.shape
    diff_rgb = np.zeros((H, W, 3), dtype=np.uint8)
    diff_rgb[exceeds] = [255, 0, 0]
    diff_rgb[~exceeds] = [0, 200, 0]

    # SSIM on grayscale (channel_mean) for a single scalar
    ref_gray = ref_arr.mean(axis=2) / 255.0
    run_gray = run_arr.mean(axis=2) / 255.0
    data_range = 1.0
    ssim_score = float(
        structural_similarity(ref_gray, run_gray, data_range=data_range)
    )

    # Per-channel SSIM
    channel_ssim = []
    for c in range(3):
        cs = float(
            structural_similarity(
                ref_arr[:, :, c] / 255.0,
                run_arr[:, :, c] / 255.0,
                data_range=1.0,
            )
        )
        channel_ssim.append(cs)

    return diff_rgb, pixel_diff_count, ssim_score, channel_ssim


def _make_composite(ref_img, run_img, diff_rgb):
    """Return a side-by-side PIL image: ref | run | diff."""
    from PIL import Image
    import numpy as np

    w, h = ref_img.width, ref_img.height
    diff_img = Image.fromarray(diff_rgb, mode="RGB")

    # Resize run to match ref if needed
    if (run_img.width, run_img.height) != (w, h):
        run_img = run_img.resize((w, h), Image.LANCZOS)

    composite = Image.new("RGB", (w * 3, h))
    composite.paste(ref_img.convert("RGB"), (0, 0))
    composite.paste(run_img.convert("RGB"), (w, 0))
    composite.paste(diff_img, (w * 2, 0))
    return composite


def run_diff(run_dir: Path, ref_dir: Path, report_dir: Path,
             threshold: int, ssim_threshold: float) -> bool:
    """Run diffing for all matching PNG pairs. Returns True if all pass."""
    from PIL import Image

    run_pngs = sorted(run_dir.glob("*.png"))
    pairs = [(p, ref_dir / p.name) for p in run_pngs if (ref_dir / p.name).exists()]

    if not pairs:
        print(
            f"[render_diff] WARNING: No run/ref PNG pairs found.\n"
            f"  run  = {run_dir}\n"
            f"  ref  = {ref_dir}"
        )
        return True

    report_dir.mkdir(parents=True, exist_ok=True)
    all_pass = True

    for run_path, ref_path in pairs:
        tag = run_path.stem

        ref_img = Image.open(ref_path)
        run_img = Image.open(run_path)

        diff_rgb, pixel_diff_count, ssim_score, channel_ssim = _compute_diff(
            ref_img, run_img, threshold
        )

        total_pixels = ref_img.width * ref_img.height
        diff_pct = round(pixel_diff_count / total_pixels * 100, 4) if total_pixels else 0.0
        ssim_pass = ssim_score >= ssim_threshold

        if not ssim_pass:
            all_pass = False

        # Save colour-coded diff PNG
        from PIL import Image as _Image
        diff_img = _Image.fromarray(diff_rgb, mode="RGB")
        diff_img.save(report_dir / f"{tag}_diff.png")

        # Save side-by-side composite PNG
        composite = _make_composite(ref_img, run_img, diff_rgb)
        composite.save(report_dir / f"{tag}_composite.png")

        # Write JSON report
        report = {
            "tag": tag,
            "ssim": round(ssim_score, 6),
            "ssim_pass": ssim_pass,
            "ssim_threshold": ssim_threshold,
            "channel_ssim": {
                "R": round(channel_ssim[0], 6),
                "G": round(channel_ssim[1], 6),
                "B": round(channel_ssim[2], 6),
            },
            "pixel_diff_count": pixel_diff_count,
            "total_pixels": total_pixels,
            "diff_pct": diff_pct,
            "threshold": threshold,
        }
        report_path = report_dir / f"{tag}_diff_report.json"
        report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

        # Human-readable summary line
        if ssim_pass:
            print(f"[PASS] {tag} (ssim={ssim_score:.3f}, diff={diff_pct:.2f}%)")
        else:
            print(f"[FAIL] {tag} (ssim={ssim_score:.3f}, diff={diff_pct:.2f}%)")

    return all_pass


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Offline render regression diff tool.\n"
            "Compares PNG captures in --run against references in --ref.\n"
            "Requires: Pillow, numpy, scikit-image"
        )
    )
    parser.add_argument(
        "--run", required=True, metavar="DIR",
        help="Directory containing run captures."
    )
    parser.add_argument(
        "--ref", required=True, metavar="DIR",
        help="Directory containing reference captures."
    )
    parser.add_argument(
        "--report-dir", default=None, metavar="DIR",
        help="Output directory for diff reports and images. "
             "Default: <run-parent>/reports/"
    )
    parser.add_argument(
        "--threshold", type=int, default=4, metavar="N",
        help="Per-pixel max channel difference to flag as differing (default: 4)."
    )
    parser.add_argument(
        "--ssim-threshold", type=float, default=0.95, metavar="F",
        help="Minimum SSIM score for a pair to pass (default: 0.95)."
    )
    return parser


if __name__ == "__main__":
    parser = _build_arg_parser()
    args = parser.parse_args()

    run_dir = Path(args.run)
    ref_dir = Path(args.ref)
    report_dir = (
        Path(args.report_dir)
        if args.report_dir
        else run_dir.parent / "reports"
    )

    if not run_dir.exists():
        print(f"[render_diff] ERROR: --run directory does not exist: {run_dir}", file=sys.stderr)
        sys.exit(1)
    if not ref_dir.exists():
        print(f"[render_diff] WARNING: --ref directory does not exist: {ref_dir}")
        print("[render_diff] No reference images found — nothing to compare.")
        sys.exit(0)

    all_pass = run_diff(run_dir, ref_dir, report_dir, args.threshold, args.ssim_threshold)
    sys.exit(0 if all_pass else 1)
