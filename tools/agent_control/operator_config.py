"""Private operator configuration; no machine or device identities belong in Git."""

import json
import os
import re
import stat
from pathlib import Path


class OperatorConfigError(RuntimeError):
    """Missing or unsafe private operator configuration."""


def load_operator_config() -> dict:
    path = Path(
        os.environ.get(
            "DOMES_OPERATOR_CONFIG", Path.home() / ".config/domes/operator.json"
        )
    )
    repository = Path(__file__).resolve().parents[2]
    try:
        if not path.is_absolute() or path.resolve().is_relative_to(repository):
            raise OperatorConfigError(
                "operator configuration must be outside the repository"
            )
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
        with os.fdopen(descriptor, "r", encoding="utf-8") as stream:
            metadata = os.fstat(stream.fileno())
            if (
                not stat.S_ISREG(metadata.st_mode)
                or metadata.st_uid != os.getuid()
                or stat.S_IMODE(metadata.st_mode) & 0o077
            ):
                raise OperatorConfigError(
                    "operator configuration must be owner-only and owned by this user"
                )
            value = json.load(stream)
    except (OSError, ValueError) as error:
        raise OperatorConfigError(
            "private operator configuration is unavailable or invalid"
        ) from error
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        raise OperatorConfigError("unsupported operator configuration schema")
    host = value.get("scheduler_host")
    serials = value.get("registered_cp2102n_serials", [])
    if not isinstance(host, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", host):
        raise OperatorConfigError("operator configuration requires one scheduler host")
    if (
        not isinstance(serials, list)
        or len(serials) not in (0, 2)
        or any(
            not isinstance(item, str) or not re.fullmatch(r"[a-z0-9]{8,64}", item)
            for item in serials
        )
        or len(set(serials)) != len(serials)
    ):
        raise OperatorConfigError(
            "operator configuration requires zero or two distinct board identities"
        )
    return value
