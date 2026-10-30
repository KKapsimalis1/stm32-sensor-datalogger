#!/usr/bin/env python3
"""Record, save and plot the data logger of the STM32 sensor board.

Commands:
  record  Clear the board's log, record for N seconds, download it, save a CSV and plot it.
  dump    Download whatever is currently stored in the board's log, save a CSV and plot it.
  plot    Plot an existing CSV file.

Examples:
  python log_tool.py record --port COM3 --period 500 --duration 60
  python log_tool.py dump --port COM3
  python log_tool.py plot logs/log_20261101_120000.csv

Requires: pip install pyserial matplotlib
Close PuTTY (or any other terminal) first: only one program can use the COM port.
"""

import argparse
import csv
import sys
import time
from datetime import datetime
from pathlib import Path

HEADER = ["t_ms", "temp_c", "press_hpa", "hum_pct",
          "ax_mg", "ay_mg", "az_mg", "gx_dps", "gy_dps", "gz_dps"]
BUFFER_CAPACITY = 1024  # records stored by the firmware (DATALOGGER_CAPACITY)


# --------------------------------------------------------------------------
# Serial communication
# --------------------------------------------------------------------------

def send_command(ser, cmd, char_delay=0.003):
    """Send a command to the board's UART menu.

    The characters are sent one at a time with a short pause. The firmware echoes every
    received byte from inside the UART interrupt, so a fast burst could overrun its
    single-byte receive buffer.
    """
    for ch in cmd + "\r":
        ser.write(ch.encode("ascii"))
        ser.flush()
        time.sleep(char_delay)


def capture_dump(ser, timeout=60.0):
    """Send 'log dump' and return the CSV rows (lists of strings) printed by the board."""
    ser.reset_input_buffer()
    send_command(ser, "log dump")

    rows = []
    in_data = False
    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()

        if line.startswith("t_ms,"):
            in_data = True
            continue
        if "records --" in line:  # footer: "-- N records --"
            return rows
        if in_data:
            fields = line.split(",")
            if len(fields) == len(HEADER):
                try:
                    [float(x) for x in fields]
                except ValueError:
                    continue  # not a data line (echo, prompt, ...)
                rows.append(fields)

    raise TimeoutError("No complete answer from the board (is the firmware running "
                       "and is the right COM port selected?)")


# --------------------------------------------------------------------------
# CSV and plotting
# --------------------------------------------------------------------------

def default_csv_path():
    return Path("logs") / f"log_{datetime.now():%Y%m%d_%H%M%S}.csv"


def save_csv(rows, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(HEADER)
        writer.writerows(rows)


def load_csv(path):
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        data = {name: [] for name in HEADER}
        for row in reader:
            for name in HEADER:
                data[name].append(float(row[name]))
    return data


def print_period_stats(t_ms):
    """Print how regular the sampling period was (the jitter of the timestamps)."""
    if len(t_ms) < 3:
        return
    dt = [b - a for a, b in zip(t_ms, t_ms[1:])]
    mean = sum(dt) / len(dt)
    std = (sum((x - mean) ** 2 for x in dt) / len(dt)) ** 0.5
    print(f"Sampling period: mean {mean:.1f} ms, std {std:.1f} ms, "
          f"min {min(dt):.0f} ms, max {max(dt):.0f} ms ({len(dt)} intervals)")


def plot_csv(path, show=True):
    import matplotlib
    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    data = load_csv(path)
    if not data["t_ms"]:
        raise ValueError(f"{path} contains no data rows")

    print_period_stats(data["t_ms"])

    t0 = data["t_ms"][0]
    t = [(x - t0) / 1000.0 for x in data["t_ms"]]

    fig, axes = plt.subplots(5, 1, figsize=(10, 12), sharex=True)
    fig.suptitle(f"STM32 data logger - {path.name}")

    axes[0].plot(t, data["temp_c"], color="tab:red")
    axes[0].set_ylabel("Temperature (°C)")

    axes[1].plot(t, data["hum_pct"], color="tab:blue")
    axes[1].set_ylabel("Humidity (%RH)")

    axes[2].plot(t, data["press_hpa"], color="tab:green")
    axes[2].set_ylabel("Pressure (hPa)")
    axes[2].ticklabel_format(axis="y", useOffset=False)

    for key, label in (("ax_mg", "x"), ("ay_mg", "y"), ("az_mg", "z")):
        axes[3].plot(t, data[key], label=label)
    axes[3].set_ylabel("Acceleration (mg)")
    axes[3].legend(loc="best", ncol=3)

    for key, label in (("gx_dps", "x"), ("gy_dps", "y"), ("gz_dps", "z")):
        axes[4].plot(t, data[key], label=label)
    axes[4].set_ylabel("Angular rate (°/s)")
    axes[4].legend(loc="best", ncol=3)
    axes[4].set_xlabel("Time (s)")

    for ax in axes:
        ax.grid(True, alpha=0.3)

    fig.tight_layout()
    png_path = path.with_suffix(".png")
    fig.savefig(png_path, dpi=150)
    print(f"Plot saved to {png_path}")

    if show:
        plt.show()


# --------------------------------------------------------------------------
# Command line
# --------------------------------------------------------------------------

def open_port(args):
    try:
        import serial
    except ImportError:
        sys.exit("pyserial is missing. Install it with: pip install pyserial")
    try:
        return serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as exc:
        sys.exit(f"Cannot open {args.port}: {exc}\n"
                 "Close PuTTY or any other program that uses this port.")


def finish(rows, args):
    if not rows:
        sys.exit("The board's log is empty. Run 'log start' first, or use the 'record' command.")
    out = Path(args.out) if args.out else default_csv_path()
    save_csv(rows, out)
    print(f"{len(rows)} records saved to {out}")
    if args.no_plot:
        print_period_stats([float(r[0]) for r in rows])
    else:
        plot_csv(out, show=not args.no_show)


def cmd_record(args):
    expected = args.duration * 1000.0 / args.period
    if expected > BUFFER_CAPACITY:
        print(f"Warning: {expected:.0f} samples requested but the board keeps only the last "
              f"{BUFFER_CAPACITY}. Use a longer --period or a shorter --duration.")

    with open_port(args) as ser:
        time.sleep(0.5)
        for cmd in ("log stop", "log clear", f"log start {args.period}"):
            send_command(ser, cmd)
            time.sleep(0.5)  # give the firmware time to process each command

        for s in range(args.duration):
            print(f"\rRecording... {s + 1}/{args.duration} s", end="", flush=True)
            time.sleep(1)
        print()

        send_command(ser, "log stop")
        time.sleep(0.5)
        print("Downloading data from the board...")
        rows = capture_dump(ser)
    finish(rows, args)


def cmd_dump(args):
    with open_port(args) as ser:
        time.sleep(0.5)
        print("Downloading data from the board...")
        rows = capture_dump(ser)
    finish(rows, args)


def cmd_plot(args):
    path = Path(args.csv)
    if not path.exists():
        sys.exit(f"File not found: {path}")
    plot_csv(path, show=not args.no_show)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def add_common(sp):
        sp.add_argument("--port", required=True, help="serial port, e.g. COM3 or /dev/ttyACM0")
        sp.add_argument("--baud", type=int, default=115200)
        sp.add_argument("--out", help="output CSV file (default: logs/log_<date_time>.csv)")
        sp.add_argument("--no-plot", action="store_true", help="only save the CSV")
        sp.add_argument("--no-show", action="store_true", help="save the PNG without opening a window")

    rec = sub.add_parser("record", help="record for a while, then download, save and plot")
    add_common(rec)
    rec.add_argument("--period", type=int, default=1000, help="sampling period in ms (default 1000)")
    rec.add_argument("--duration", type=int, default=30, help="recording time in seconds (default 30)")
    rec.set_defaults(func=cmd_record)

    dump = sub.add_parser("dump", help="download the log currently stored on the board")
    add_common(dump)
    dump.set_defaults(func=cmd_dump)

    plot = sub.add_parser("plot", help="plot an existing CSV file")
    plot.add_argument("csv", help="CSV file to plot")
    plot.add_argument("--no-show", action="store_true", help="save the PNG without opening a window")
    plot.set_defaults(func=cmd_plot)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()