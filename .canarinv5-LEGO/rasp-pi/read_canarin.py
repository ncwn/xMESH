import os
import re
import sqlite3
import json
from typing import Iterable, List, Tuple
import logging
from logging.handlers import SysLogHandler

import serial  # pyserial


ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")
LOGGER = logging.getLogger("read_canarin")
LOGGER.propagate = False


def strip_noise(s: str) -> str:
    """Remove NULs, ANSI color codes, and carriage returns from a line."""
    if not s:
        return s
    s = s.replace("\x00", "")
    s = s.replace("\r", "")
    s = ANSI_ESCAPE_RE.sub("", s)
    return s


def parse_pushing_line(line: str) -> Tuple[int, List[Tuple[str, str, str, str]]]:
    """Parse a single 'SENSORMNG: Pushing:' log line.

    Returns:
        (timestamp, entries)
        where entries is a list of tuples: (sensor_name, port, type, value_s)
    """
    # Find payload after marker
    marker = "SENSORMNG: Pushing:"
    if marker not in line:
        raise ValueError("Line does not contain pushing marker")

    payload = line.split(marker, 1)[1].strip()

    # The payload is a comma-separated list of key:value triples, with TIMESTAMP first
    tokens = [tok.strip() for tok in payload.split(",") if tok.strip()]
    if not tokens:
        raise ValueError("No tokens parsed from line")

    ts = None
    entries: List[Tuple[str, str, str, str]] = []

    for tok in tokens:
        parts = tok.split(":")
        if len(parts) < 2:
            # skip malformed token
            continue
        if parts[0] == "TIMESTAMP":
            # Expected: TIMESTAMP:none:n:1759301347
            if len(parts) >= 4:
                try:
                    ts = int(parts[3])
                except ValueError:
                    # leave ts as None; will error later if still None
                    pass
            continue

        # Expected: NAME:port:type:value
        if len(parts) < 4:
            # malformed, skip
            continue
        name = parts[0]
        port = parts[1]
        typ = parts[2]
        # Value itself might contain colons in extremely unlikely cases; join back
        value_s = ":".join(parts[3:])
        entries.append((name, port, typ, value_s))

    if ts is None:
        raise ValueError("Timestamp not found in pushing line")

    return ts, entries


def ensure_table(conn: sqlite3.Connection, table: str) -> None:
    LOGGER.debug(f"Ensuring table exists: {table}")
    conn.execute(
        f"""
        CREATE TABLE IF NOT EXISTS {table} (
            timestamp INTEGER NOT NULL,
            sensor_name TEXT NOT NULL,
            port TEXT NOT NULL,
            type TEXT NOT NULL,
            value_s TEXT NOT NULL
        )
        """
    )


def insert_rows(
    conn: sqlite3.Connection, table: str, ts: int, rows: Iterable[Tuple[str, str, str, str]]
) -> int:
    data = [(ts, name, port, typ, val) for (name, port, typ, val) in rows]
    if not data:
        return 0
    conn.executemany(
        f"INSERT INTO {table} (timestamp, sensor_name, port, type, value_s) VALUES (?, ?, ?, ?, ?)",
        data,
    )
    LOGGER.debug(f"Inserted {len(data)} rows for ts={ts}")
    return len(data)



def process_serial_forever(dev_path: str, baudrate: int, db_path: str, table: str) -> None:
    """Continuously read from a serial device using pyserial and insert rows.

    Uses a 1-second read timeout for responsiveness. Retries with backoff on errors.
    """
    # Single-threaded use of SQLite
    conn = sqlite3.connect(db_path, timeout=30)
    ensure_table(conn, table)

    backoff_s = 1.0
    while True:
        ser = None
        try:
            LOGGER.info(f"Opening serial: {dev_path} @ {baudrate} baud (timeout=1)")
            ser = serial.Serial(dev_path, baudrate=baudrate, timeout=1)
            backoff_s = 1.0  # reset after successful open
            while True:
                try:
                    line_bytes = ser.readline()
                except Exception:
                    LOGGER.error("Serial read error; will reopen", exc_info=False)
                    break
                if not line_bytes:
                    # Timed out without data; continue polling
                    continue
                try:
                    text = line_bytes.decode("utf-8", errors="ignore")
                except Exception:
                    LOGGER.debug("Byte decode error; skipping line", exc_info=False)
                    continue
                line = strip_noise(text)
                if "SENSORMNG: Pushing:" not in line:
                    continue
                try:
                    ts, entries = parse_pushing_line(line)
                except Exception:
                    LOGGER.debug("Failed to parse serial pushing line", exc_info=False)
                    continue
                try:
                    count = insert_rows(conn, table, ts, entries)
                    if count:
                        conn.commit()
                        LOGGER.debug(f"Committed {count} rows")
                except Exception:
                    try:
                        conn.rollback()
                    except Exception:
                        pass
                    LOGGER.warning("Database write failed; continuing", exc_info=False)
        except KeyboardInterrupt:
            LOGGER.info("Interrupted; exiting.")
            break
        except Exception:
            LOGGER.error("Serial/database loop error; will retry", exc_info=False)
        finally:
            try:
                if ser is not None and ser.is_open:
                    ser.close()
            except Exception:
                pass
        # Backoff before trying to reopen
        try:
            import time as _time
            _time.sleep(backoff_s)
        except Exception:
            pass
        backoff_s = min(backoff_s * 2, 30.0)


def load_or_init_config(cfg_path: str) -> dict:
    base_dir = os.path.dirname(cfg_path)
    defaults = {
        # Serial device path: '/dev/ttyACM0' or '/dev/ttyUSB0'
        "device": "/dev/ttyACM0",
        "baudrate": 115200,
        "db_path": "canarin.sqlite",  # relative to rasp-pi/
        "table": "sensor_readings",
        # Logging level: DEBUG, INFO, WARNING, ERROR, CRITICAL
        "log_level": "INFO",
    }

    cfg = None
    if os.path.isfile(cfg_path) and os.path.getsize(cfg_path) > 0:
        try:
            with open(cfg_path, "r", encoding="utf-8") as f:
                cfg = json.load(f)
        except Exception:
            cfg = None

    if not isinstance(cfg, dict):
        cfg = defaults.copy()
        with open(cfg_path, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)
        return cfg

    # Merge any missing defaults
    updated = False
    for k, v in defaults.items():
        if k not in cfg:
            cfg[k] = v
            updated = True
    if updated:
        with open(cfg_path, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)
    return cfg


def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    cfg_path = os.path.join(base_dir, "config.json")
    cfg = load_or_init_config(cfg_path)

    # Configure simpler syslog logging
    level_name = str(cfg.get("log_level", "INFO")).upper()
    level = getattr(logging, level_name, logging.INFO)
    LOGGER.handlers[:] = []
    LOGGER.setLevel(level)
    formatter = logging.Formatter("read_canarin[%(process)d]: %(levelname)s: %(message)s")
    try:
        handler = SysLogHandler(address="/dev/log", facility=SysLogHandler.LOG_USER)
        # Hint rsyslog about program tag; not all Python versions support this
        try:
            setattr(handler, "ident", "read_canarin: ")
        except Exception:
            pass
        handler.setLevel(level)
        handler.setFormatter(formatter)
        LOGGER.addHandler(handler)
    except Exception:
        # Fallback to stderr if syslog socket not available
        handler = logging.StreamHandler()
        handler.setLevel(level)
        handler.setFormatter(formatter)
        LOGGER.addHandler(handler)

    def resolve(p: str) -> str:
        return p if os.path.isabs(p) else os.path.join(base_dir, p)

    # Support both new 'device' and legacy 'log_path' keys
    device_path = cfg.get("device") or cfg.get("log_path") or "/dev/ttyACM0"
    db_path = resolve(cfg.get("db_path", "canarin.sqlite"))
    table = cfg.get("table", "sensor_readings")
    baudrate = int(cfg.get("baudrate", 115200))

    os.makedirs(os.path.dirname(db_path), exist_ok=True)

    # Serial-only mode
    if not isinstance(device_path, str) or not device_path.startswith("/dev/tty"):
        raise SystemExit(
            "Serial-only mode: set 'device' (or legacy 'log_path') in config.json to a tty device like /dev/ttyACM0 or /dev/ttyUSB0"
        )

    process_serial_forever(device_path, baudrate, db_path, table)


if __name__ == "__main__":
    main()
