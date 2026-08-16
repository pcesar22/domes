"""Broker-owned, frame-filtering PTY relay for untrusted trace clients."""

from __future__ import annotations

import fcntl
import os
import pty
import select
import struct
import termios
import threading
import time
import zlib

FRAME_START = b"\xaa\x55"
MAX_FRAME_BODY = 1025  # One message-type byte plus at most 1024 payload bytes.
TRACE_DUMP_TYPE = 0x12


class SerialTraceProxyError(RuntimeError):
    pass


def _set_raw_115200(descriptor: int) -> None:
    attributes = termios.tcgetattr(descriptor)
    attributes[0] &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
    )
    attributes[1] &= ~termios.OPOST
    attributes[2] &= ~(termios.CSIZE | termios.PARENB)
    attributes[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD
    attributes[2] &= ~termios.HUPCL
    attributes[3] &= ~(
        termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN
    )
    attributes[4] = termios.B115200
    attributes[5] = termios.B115200
    attributes[6][termios.VMIN] = 0
    attributes[6][termios.VTIME] = 1
    termios.tcsetattr(descriptor, termios.TCSANOW, attributes)


def _deassert_physical_reset_lines(descriptor: int) -> None:
    """Mirror the DOMES serial opener's reset-safe RTS-then-DTR sequence."""
    fcntl.ioctl(descriptor, termios.TIOCMBIC, struct.pack("i", termios.TIOCM_RTS))
    fcntl.ioctl(descriptor, termios.TIOCMBIC, struct.pack("i", termios.TIOCM_DTR))


def _decode_candidate_frame(buffer: bytearray) -> bytes | None:
    """Pop one complete, CRC-valid frame or reject candidate-controlled bytes."""
    if len(buffer) < 2:
        return None
    if buffer[:2] != FRAME_START:
        raise SerialTraceProxyError("trace candidate emitted non-frame serial bytes")
    if len(buffer) < 4:
        return None
    body_size = int.from_bytes(buffer[2:4], "little")
    if body_size < 1 or body_size > MAX_FRAME_BODY:
        raise SerialTraceProxyError("trace candidate emitted an invalid frame length")
    frame_size = 4 + body_size + 4
    if len(buffer) < frame_size:
        return None
    frame = bytes(buffer[:frame_size])
    del buffer[:frame_size]
    body = frame[4 : 4 + body_size]
    received_crc = int.from_bytes(frame[-4:], "little")
    if zlib.crc32(body) & 0xFFFF_FFFF != received_crc:
        raise SerialTraceProxyError("trace candidate emitted an invalid frame CRC")
    return frame


class SerialTraceProxy:
    """Expose a PTY while forwarding only one empty TRACE_DUMP request."""

    def __init__(self, port: str) -> None:
        self.port = port
        self.master: int | None = None
        self.slave: int | None = None
        self.device: int | None = None
        self.slave_path = ""
        self.transcript: list[tuple[str, bytes]] = []
        self.stop = threading.Event()
        self.thread: threading.Thread | None = None
        self.error: Exception | None = None
        self._candidate_bytes = bytearray()
        self._dump_requests = 0

    def __enter__(self) -> "SerialTraceProxy":
        try:
            self.master, self.slave = pty.openpty()
            _set_raw_115200(self.slave)
            self.slave_path = os.ttyname(self.slave)
            self.device = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            fcntl.ioctl(self.device, termios.TIOCEXCL)
            _set_raw_115200(self.device)
            _deassert_physical_reset_lines(self.device)
            termios.tcflush(self.device, termios.TCIOFLUSH)
            os.set_blocking(self.master, False)
            os.set_blocking(self.device, False)
            self.thread = threading.Thread(
                target=self._relay, name="domes-trace-serial-relay", daemon=True
            )
            self.thread.start()
            return self
        except Exception:
            if self.device is not None:
                try:
                    _deassert_physical_reset_lines(self.device)
                except OSError:
                    pass
            for descriptor in (self.device, self.master, self.slave):
                if descriptor is not None:
                    try:
                        os.close(descriptor)
                    except OSError:
                        pass
            self.device = self.master = self.slave = None
            raise

    def _write_all(self, descriptor: int, data: bytes) -> None:
        offset = 0
        deadline = time.monotonic() + 5.0
        while offset < len(data) and not self.stop.is_set():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise SerialTraceProxyError("trace serial relay write timed out")
            _, writable, _ = select.select([], [descriptor], [], min(remaining, 0.1))
            if not writable:
                continue
            try:
                written = os.write(descriptor, data[offset:])
            except BlockingIOError:
                continue
            if written <= 0:
                raise SerialTraceProxyError("trace serial relay write failed")
            offset += written

    def _candidate_to_device(self, data: bytes) -> None:
        assert self.device is not None
        self._candidate_bytes.extend(data)
        while True:
            frame = _decode_candidate_frame(self._candidate_bytes)
            if frame is None:
                return
            body_size = int.from_bytes(frame[2:4], "little")
            body = frame[4 : 4 + body_size]
            if body != bytes([TRACE_DUMP_TYPE]) or self._dump_requests != 0:
                raise SerialTraceProxyError(
                    "trace candidate attempted a non-allowlisted serial command"
                )
            self._write_all(self.device, frame)
            self.transcript.append(("tx", frame))
            self._dump_requests += 1

    def _relay(self) -> None:
        assert self.master is not None and self.device is not None
        try:
            while not self.stop.is_set():
                ready, _, _ = select.select([self.master, self.device], [], [], 0.1)
                for source in ready:
                    try:
                        data = os.read(source, 4096)
                    except BlockingIOError:
                        continue
                    if not data:
                        continue
                    if source == self.master:
                        self._candidate_to_device(data)
                    else:
                        self._write_all(self.master, data)
                        self.transcript.append(("rx", data))
        except (OSError, ValueError, SerialTraceProxyError) as error:
            self.error = error
            self.stop.set()

    def __exit__(self, exc_type: object, *_: object) -> None:
        self.stop.set()
        if self.thread:
            self.thread.join(2)
            if self.thread.is_alive() and self.error is None:
                self.error = SerialTraceProxyError("trace serial relay did not stop")
        if self._candidate_bytes and self.error is None:
            self.error = SerialTraceProxyError(
                "trace candidate left an incomplete serial frame"
            )
        if self.device is not None:
            try:
                _deassert_physical_reset_lines(self.device)
            except OSError:
                pass
        for descriptor in (self.device, self.master, self.slave):
            if descriptor is not None:
                try:
                    os.close(descriptor)
                except OSError:
                    pass
        if exc_type is None and self.error is not None:
            raise SerialTraceProxyError(str(self.error)) from self.error
