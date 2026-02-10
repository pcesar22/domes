#!/usr/bin/env python3
"""Merge multiple pod trace dumps into a single Perfetto-compatible Chrome JSON file.

Usage:
    python trace_merge.py \
        --pod /tmp/trace-pod1.json --pod-name "Pod 1 (EA:52)" \
        --pod /tmp/trace-pod2.json --pod-name "Pod 2 (EB:C2)" \
        --names tools/trace/trace_names.json \
        --align beacon \
        -o /tmp/merged-trace.json

Open the output at https://ui.perfetto.dev
"""

import argparse
import json
import sys
from pathlib import Path

# Category ID -> human name (must match firmware traceEvent.hpp)
CATEGORIES = {
    0: "kernel",
    1: "transport",
    2: "ota",
    3: "wifi",
    4: "led",
    5: "audio",
    6: "touch",
    7: "game",
    8: "user",
    9: "haptic",
    10: "ble",
    11: "nvs",
    12: "espnow",
}

# Category -> thread ID mapping for Perfetto visualization
# Group related categories into separate swim lanes
CATEGORY_TID = {
    "kernel": 1,
    "transport": 2,
    "ota": 3,
    "wifi": 4,
    "led": 5,
    "audio": 6,
    "touch": 7,
    "game": 8,
    "user": 9,
    "haptic": 10,
    "ble": 11,
    "nvs": 12,
    "espnow": 13,
    "unknown": 14,
}


def load_names(path):
    """Load span ID -> human name mappings."""
    if not path:
        return {}
    with open(path) as f:
        return json.load(f)


def resolve_name(raw_name, names_map):
    """Resolve 'span:12345' to human-readable name."""
    if raw_name.startswith("span:"):
        span_id = raw_name.split(":")[1]
        return names_map.get(span_id, raw_name)
    return raw_name


def find_beacon_timestamps(events, names_map):
    """Find SendBeacon begin timestamps for alignment."""
    beacons = []
    for e in events:
        name = resolve_name(e.get("name", ""), names_map)
        if name == "EspNow.SendBeacon" and e.get("ph") == "B":
            beacons.append(e["ts"])
    return beacons


def merge_traces(pod_files, pod_names, names_map, align_mode="zero"):
    """Merge multiple pod trace files into a single Perfetto JSON."""
    merged = []
    pod_data = []

    for i, path in enumerate(pod_files):
        with open(path) as f:
            data = json.load(f)
        events = data if isinstance(data, list) else data.get("traceEvents", [])
        pod_data.append(events)

    # Calculate time offsets for alignment
    offsets = []
    if align_mode == "beacon":
        # Align on first SendBeacon event
        beacon_times = []
        for events in pod_data:
            beacons = find_beacon_timestamps(events, names_map)
            beacon_times.append(beacons[0] if beacons else None)

        if all(t is not None for t in beacon_times):
            # Use first pod's beacon as reference
            ref_time = beacon_times[0]
            for bt in beacon_times:
                offsets.append(ref_time - bt)
            print(f"Aligned on beacon: offsets = {offsets}", file=sys.stderr)
        else:
            # Fall back to zero-align
            print("No beacons found, falling back to zero-align", file=sys.stderr)
            align_mode = "zero"

    if align_mode == "zero":
        # Align all traces to start at t=0
        for events in pod_data:
            if events:
                min_ts = min(e["ts"] for e in events)
                offsets.append(-min_ts)
            else:
                offsets.append(0)

    elif align_mode == "raw":
        offsets = [0] * len(pod_data)

    # Add process name metadata for each pod
    for i, name in enumerate(pod_names):
        merged.append({
            "ph": "M",
            "pid": i,
            "tid": 0,
            "name": "process_name",
            "args": {"name": name},
        })
        # Add thread name metadata for each category
        for cat_name, tid in CATEGORY_TID.items():
            merged.append({
                "ph": "M",
                "pid": i,
                "tid": tid,
                "name": "thread_name",
                "args": {"name": cat_name},
            })

    # Merge events from all pods
    for pod_idx, events in enumerate(pod_data):
        offset = offsets[pod_idx]
        for e in events:
            event = dict(e)
            # Assign pid from pod index
            event["pid"] = pod_idx
            # Resolve category to tid
            cat = event.get("cat", "unknown")
            if cat == "unknown":
                # Try to determine from name
                name = resolve_name(event.get("name", ""), names_map)
                if "EspNow" in name:
                    cat = "espnow"
                    event["cat"] = cat
            event["tid"] = CATEGORY_TID.get(cat, 14)
            # Resolve span name
            event["name"] = resolve_name(event.get("name", ""), names_map)
            # Apply time offset
            event["ts"] = event["ts"] + offset
            merged.append(event)

    # Sort by timestamp for nice viewing
    merged.sort(key=lambda e: (e.get("ts", 0), e.get("pid", 0)))
    return merged


def main():
    parser = argparse.ArgumentParser(description="Merge pod traces for Perfetto")
    parser.add_argument("--pod", action="append", required=True,
                        help="Pod trace JSON file (can specify multiple)")
    parser.add_argument("--pod-name", action="append", default=None,
                        help="Pod display name (one per --pod)")
    parser.add_argument("--names", "-n", default=None,
                        help="trace_names.json for span ID resolution")
    parser.add_argument("--align", choices=["zero", "beacon", "raw"],
                        default="beacon",
                        help="Alignment mode: zero (all start at 0), "
                             "beacon (align on first ESP-NOW beacon), "
                             "raw (use raw timestamps)")
    parser.add_argument("-o", "--output", required=True,
                        help="Output merged JSON file")

    args = parser.parse_args()

    if args.pod_name and len(args.pod_name) != len(args.pod):
        parser.error("Must have same number of --pod-name as --pod")

    pod_names = args.pod_name or [f"Pod {i}" for i in range(len(args.pod))]
    names_map = load_names(args.names)

    merged = merge_traces(args.pod, pod_names, names_map, args.align)

    with open(args.output, "w") as f:
        json.dump({"traceEvents": merged}, f, separators=(",", ":"))

    print(f"Merged {len(args.pod)} pods → {args.output} "
          f"({len(merged)} events)", file=sys.stderr)


if __name__ == "__main__":
    main()
