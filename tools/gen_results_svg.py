#!/usr/bin/env python3
"""Generate SVG drill results visualization."""
import sys

def main():
    output_path = sys.argv[1] if len(sys.argv) > 1 else "sim_results.svg"

    # Drill results from the trace generator output
    rounds = [
        (1, True, 200.0, 0, "green"),
        (2, True, 180.0, 0, "green"),
        (3, True, 250.0, 1, "green"),
        (4, True, 150.0, 0, "green"),
        (0, True, 120.0, 0, "green"),
        (2, True, 100.0, 0, "yellow"),
        (4, False, 0, 0, "yellow"),
        (1, True, 80.0, 2, "yellow"),
        (3, False, 0, 0, "yellow"),
        (0, True, 90.0, 0, "yellow"),
        (1, True, 60.0, 0, "red"),
        (2, True, 55.0, 1, "red"),
        (3, False, 0, 0, "red"),
        (4, True, 45.0, 0, "red"),
        (0, True, 70.0, 3, "red"),
    ]

    width = 900
    height = 520
    margin_left = 60
    margin_right = 30
    margin_top = 80
    bar_height = 22
    bar_gap = 4
    chart_width = width - margin_left - margin_right
    max_reaction = 300  # ms scale

    parts = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}"'
                 f' style="background:#0d1117;font-family:\'Segoe UI\',Roboto,monospace">')

    # Header
    parts.append(f'<text x="{width//2}" y="28" text-anchor="middle" fill="#e6edf3"'
                 f' font-size="15" font-weight="600">Drill Results — Reaction Times</text>')

    # Stats row
    hits = sum(1 for r in rounds if r[1])
    misses = len(rounds) - hits
    avg = sum(r[2] for r in rounds if r[1]) / hits
    parts.append(f'<text x="{width//2}" y="48" text-anchor="middle" fill="#8b949e" font-size="11">'
                 f'15 Rounds \u2022 {hits} Hits \u2022 {misses} Misses \u2022'
                 f' Avg: {avg:.1f}ms \u2022 Best: 45.0ms \u2022 Worst: 250.0ms</text>')

    # Phase labels
    phases = [("Warm-up", 0, 5, "#66BB6A"), ("Speed", 5, 5, "#FFA726"), ("Sprint", 10, 5, "#EF5350")]
    for label, start, count, color in phases:
        y1 = margin_top + start * (bar_height + bar_gap)
        y2 = y1 + count * (bar_height + bar_gap) - bar_gap
        parts.append(f'<rect x="2" y="{y1}" width="4" height="{y2 - y1}" rx="2" fill="{color}" opacity="0.6"/>')
        mid_y = (y1 + y2) / 2
        parts.append(f'<text x="10" y="{mid_y + 4}" fill="{color}" font-size="8"'
                     f' transform="rotate(-90 10 {mid_y})" text-anchor="middle">{label}</text>')

    # Grid lines
    for ms in range(0, 301, 50):
        xp = margin_left + (ms / max_reaction) * chart_width
        parts.append(f'<line x1="{xp:.1f}" y1="{margin_top - 10}" x2="{xp:.1f}"'
                     f' y2="{margin_top + len(rounds) * (bar_height + bar_gap)}"'
                     f' stroke="#21262d" stroke-width="0.5"/>')
        parts.append(f'<text x="{xp:.1f}" y="{margin_top - 15}" text-anchor="middle"'
                     f' fill="#484f58" font-size="8">{ms}ms</text>')

    # Bars
    phase_colors = {"green": "#66BB6A", "yellow": "#FFA726", "red": "#EF5350"}
    for i, (pod, hit, react_ms, pad, phase) in enumerate(rounds):
        y = margin_top + i * (bar_height + bar_gap)
        color = phase_colors[phase]

        # Round label
        parts.append(f'<text x="{margin_left - 8}" y="{y + bar_height // 2 + 4}"'
                     f' text-anchor="end" fill="#8b949e" font-size="10">R{i}</text>')

        if hit:
            bar_w = (react_ms / max_reaction) * chart_width
            # Gradient bar
            parts.append(f'<rect x="{margin_left}" y="{y}" width="{bar_w:.1f}"'
                         f' height="{bar_height}" rx="3" fill="{color}" opacity="0.75"/>')
            # Pod label inside bar
            if bar_w > 80:
                parts.append(f'<text x="{margin_left + 8}" y="{y + bar_height // 2 + 4}"'
                             f' fill="#0d1117" font-size="10" font-weight="600">'
                             f'Pod {pod}</text>')
                parts.append(f'<text x="{margin_left + bar_w - 8}" y="{y + bar_height // 2 + 4}"'
                             f' text-anchor="end" fill="#0d1117" font-size="10">'
                             f'{react_ms:.0f}ms</text>')
            else:
                parts.append(f'<text x="{margin_left + bar_w + 5}" y="{y + bar_height // 2 + 4}"'
                             f' fill="{color}" font-size="10">'
                             f'Pod {pod} \u2022 {react_ms:.0f}ms</text>')
        else:
            # Miss indicator
            parts.append(f'<rect x="{margin_left}" y="{y}" width="{chart_width}"'
                         f' height="{bar_height}" rx="3" fill="#21262d" opacity="0.5"/>')
            parts.append(f'<text x="{margin_left + 8}" y="{y + bar_height // 2 + 4}"'
                         f' fill="#f85149" font-size="10" font-weight="600">'
                         f'Pod {pod} \u2014 MISS (timeout)</text>')
            # X marks
            parts.append(f'<text x="{margin_left + chart_width - 10}"'
                         f' y="{y + bar_height // 2 + 4}" text-anchor="end"'
                         f' fill="#f85149" font-size="12">\u2717</text>')

    # Pod distribution summary at bottom
    summary_y = margin_top + len(rounds) * (bar_height + bar_gap) + 25
    parts.append(f'<text x="{width//2}" y="{summary_y}" text-anchor="middle"'
                 f' fill="#8b949e" font-size="11">Per-Pod Hit Rate:</text>')
    pod_stats = {}
    for pod, hit, react, pad, phase in rounds:
        if pod not in pod_stats:
            pod_stats[pod] = {"hits": 0, "total": 0, "times": []}
        pod_stats[pod]["total"] += 1
        if hit:
            pod_stats[pod]["hits"] += 1
            pod_stats[pod]["times"].append(react)

    sx = width // 2 - 200
    for pod in sorted(pod_stats.keys()):
        s = pod_stats[pod]
        pct = s["hits"] / s["total"] * 100
        avg_t = sum(s["times"]) / len(s["times"]) if s["times"] else 0
        color = "#66BB6A" if pct == 100 else "#FFA726" if pct >= 50 else "#EF5350"
        parts.append(f'<text x="{sx}" y="{summary_y + 20}" fill="{color}" font-size="10">'
                     f'Pod {pod}: {s["hits"]}/{s["total"]} ({pct:.0f}%)'
                     f' avg={avg_t:.0f}ms</text>')
        sx += 160

    parts.append("</svg>")
    svg = "\n".join(parts)
    with open(output_path, "w") as f:
        f.write(svg)
    print(f"SVG written to {output_path} ({len(svg)} bytes)")

if __name__ == "__main__":
    main()
