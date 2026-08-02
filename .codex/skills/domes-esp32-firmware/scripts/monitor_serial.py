#!/usr/bin/env python3
"""Compatibility entry point for the canonical DOMES serial monitor."""

import os
import sys
from pathlib import Path


canonical = Path(__file__).resolve().parents[4] / "tools" / "firmware" / "monitor_serial.py"
os.execv(sys.executable, [sys.executable, str(canonical), *sys.argv[1:]])
