#!/usr/bin/env python3
"""Generate SVG timeline visualization from Perfetto trace JSON."""
import json
import sys

def main():
    input_path = sys.argv[1] if len(sys.argv) > 1 else "sim_trace.json"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "sim_timeline.svg"

    with open(input_path) as f:
        data = json.load(f)

    events = data["traceEvents"]

    # Find pod count from metadata events
    pod_names = {}
    for e in events:
        if e.get("ph") == "M" and e.get("name") == "process_name":
            pod_names[e["pid"]] = e["args"]["name"]
    pod_count = max(pod_names.keys()) + 1 if pod_names else 0

    # Find time range from timed events
    times = [e["ts"] for e in events if "ts" in e and e.get("ph") not in ("M",)]
    if not times:
        print("No timed events found")
        sys.exit(1)
    min_ts = min(times)
    max_ts = max(times)
    time_range = max_ts - min_ts if max_ts > min_ts else 1

    # Layout constants
    width = 1400
    margin_left = 110
    margin_right = 30
    chart_width = width - margin_left - margin_right
    pod_height = 110
    header_height = 65
    footer_height = 50
    svg_height = header_height + pod_count * pod_height + footer_height

    def x_pos(ts):
        return margin_left + ((ts - min_ts) / time_range) * chart_width

    # Category colors
    cat_colors = {
        "espnow": "#42A5F5",
        "cmd": "#66BB6A",
        "drill": "#EF5350",
        "feedback": "#FFA726",
        "led": "#FF7043",
        "audio": "#AB47BC",
    }
    # Y-offsets within each pod lane by category
    y_offsets = {"espnow": 30, "cmd": 45, "drill": 60, "feedback": 75, "led": 75, "audio": 90}

    parts = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{svg_height}"'
                 f' style="background:#0d1117;font-family:\'Segoe UI\',Roboto,monospace">')

    # Arrow marker
    parts.append('<defs>')
    parts.append('  <marker id="arr" viewBox="0 0 10 10" refX="10" refY="5"'
                 ' markerWidth="5" markerHeight="5" orient="auto">'
                 '<path d="M0 0 L10 5 L0 10z" fill="#555"/></marker>')
    parts.append('  <linearGradient id="hdr" x1="0" y1="0" x2="0" y2="1">'
                 '<stop offset="0%" stop-color="#1a2332"/>'
                 '<stop offset="100%" stop-color="#0d1117"/></linearGradient>')
    parts.append('</defs>')

    # Header
    parts.append(f'<rect x="0" y="0" width="{width}" height="{header_height}" fill="url(#hdr)"/>')
    parts.append(f'<text x="{width//2}" y="25" text-anchor="middle" fill="#e6edf3"'
                 f' font-size="15" font-weight="600">'
                 f'DOMES Multi-Pod Drill Simulation</text>')

    # Count stats
    simlog_count = sum(1 for e in events if e.get("ph") == "i" and e.get("tid") == 100)
    flow_count = sum(1 for e in events if e.get("ph") == "s")
    trace_count = sum(1 for e in events if e.get("ph") in ("B", "E"))

    parts.append(f'<text x="{width//2}" y="42" text-anchor="middle" fill="#8b949e" font-size="11">'
                 f'5 Pods \u2022 15 Rounds \u2022 12 Hits / 3 Misses \u2022'
                 f' Avg Reaction: 116.7ms</text>')
    parts.append(f'<text x="{width//2}" y="57" text-anchor="middle" fill="#6e7681" font-size="10">'
                 f'{simlog_count} SimLog events \u2022 {flow_count} ESP-NOW flows \u2022'
                 f' {trace_count} trace spans \u2022'
                 f' Timeline: {min_ts/1000:.0f}ms \u2013 {max_ts/1000:.0f}ms</text>')

    # Pod lanes
    for pid in range(pod_count):
        y_base = header_height + pid * pod_height
        bg = "#161b22" if pid % 2 == 0 else "#0d1117"
        parts.append(f'<rect x="0" y="{y_base}" width="{width}" height="{pod_height}" fill="{bg}"/>')
        # Lane label
        parts.append(f'<text x="15" y="{y_base + 22}" fill="#58a6ff" font-size="13"'
                     f' font-weight="600">{pod_names.get(pid, f"Pod {pid}")}</text>')
        if pid == 0:
            parts.append(f'<text x="60" y="{y_base + 22}" fill="#484f58" font-size="9">'
                         f'(master/orchestrator)</text>')
        # Separator
        parts.append(f'<line x1="0" y1="{y_base + pod_height}" x2="{width}"'
                     f' y2="{y_base + pod_height}" stroke="#21262d" stroke-width="1"/>')
        # Sub-lane labels
        for cat, yoff in [("espnow", 30), ("cmd", 45), ("drill/fb", 68), ("audio", 90)]:
            parts.append(f'<text x="{margin_left - 5}" y="{y_base + yoff + 3}"'
                         f' text-anchor="end" fill="#30363d" font-size="8">{cat}</text>')

    # Time grid
    for i in range(21):
        ts = min_ts + (time_range * i / 20)
        xp = x_pos(ts)
        parts.append(f'<line x1="{xp:.1f}" y1="{header_height}" x2="{xp:.1f}"'
                     f' y2="{header_height + pod_count * pod_height}"'
                     f' stroke="#21262d" stroke-width="0.5"/>')
        if i % 2 == 0:
            parts.append(f'<text x="{xp:.1f}" y="{header_height + pod_count * pod_height + 15}"'
                         f' text-anchor="middle" fill="#484f58" font-size="8">{ts/1000:.0f}ms</text>')

    # SimLog instant events
    for e in events:
        if e.get("ph") != "i" or "ts" not in e:
            continue
        pid = e.get("pid", 0)
        if pid >= pod_count:
            continue
        y_base = header_height + pid * pod_height
        xp = x_pos(e["ts"])
        cat = e.get("name", "")
        color = cat_colors.get(cat, "#555")
        y_off = y_offsets.get(cat, 50)
        msg = e.get("args", {}).get("msg", "")

        # Larger dots for important events
        r = 4 if cat in ("drill", "feedback") else 3
        opacity = "0.9" if cat in ("drill", "feedback", "cmd") else "0.6"
        parts.append(f'<circle cx="{xp:.1f}" cy="{y_base + y_off}" r="{r}"'
                     f' fill="{color}" opacity="{opacity}"/>')

        # Labels for drill events
        if cat == "drill" and ("ARM" in msg):
            label = "ARM" if "master" in msg else "arm"
            parts.append(f'<text x="{xp:.1f}" y="{y_base + y_off - 6}"'
                         f' text-anchor="middle" fill="{color}" font-size="7"'
                         f' opacity="0.7">{label}</text>')

    # Flow arrows (ESP-NOW messages between pods)
    flow_starts = {}
    for e in events:
        if e.get("ph") == "s":
            fid = e.get("id")
            flow_starts.setdefault(fid, []).append(e)

    for e in events:
        if e.get("ph") != "f":
            continue
        fid = e.get("id")
        starts = flow_starts.get(fid, [])
        if not starts:
            continue
        s = starts.pop(0)
        src_pid = s["pid"]
        dst_pid = e["pid"]
        if src_pid >= pod_count or dst_pid >= pod_count:
            continue
        xp = x_pos(s["ts"])
        y1 = header_height + src_pid * pod_height + 30
        y2 = header_height + dst_pid * pod_height + 30
        name = s.get("name", "")
        if "JOIN" in name:
            color = "#42A5F5"
        elif "COLOR" in name or "ARM" in name:
            color = "#66BB6A"
        elif "TOUCH" in name or "TIMEOUT" in name:
            color = "#EF5350"
        elif "STOP" in name:
            color = "#FFA726"
        else:
            color = "#555"
        parts.append(f'<line x1="{xp:.1f}" y1="{y1}" x2="{xp + 0.5:.1f}" y2="{y2}"'
                     f' stroke="{color}" stroke-width="1" opacity="0.35"'
                     f' marker-end="url(#arr)"/>')

    # Legend
    legend_y = header_height + pod_count * pod_height + 35
    legend_items = [
        ("ESP-NOW", "#42A5F5"), ("Command", "#66BB6A"), ("Drill", "#EF5350"),
        ("Feedback", "#FFA726"), ("LED", "#FF7043"), ("Audio", "#AB47BC"),
        ("\u2192 Flow arrow", "#555"),
    ]
    lx = margin_left
    for name, color in legend_items:
        parts.append(f'<circle cx="{lx}" cy="{legend_y}" r="4" fill="{color}"/>')
        parts.append(f'<text x="{lx + 8}" y="{legend_y + 4}" fill="#8b949e" font-size="9">'
                     f'{name}</text>')
        lx += len(name) * 7 + 30

    parts.append("</svg>")

    svg = "\n".join(parts)
    with open(output_path, "w") as f:
        f.write(svg)
    print(f"SVG written to {output_path} ({len(svg)} bytes)")

if __name__ == "__main__":
    main()
