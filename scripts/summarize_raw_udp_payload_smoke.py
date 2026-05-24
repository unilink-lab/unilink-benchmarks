#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


def as_int(row, field):
    try:
        return int(float(row.get(field, "0") or 0))
    except ValueError:
        return 0


def as_float(row, field):
    try:
        return float(row.get(field, "0") or 0)
    except ValueError:
        return 0.0


def build_notes(rows):
    notes = [
        "- raw 4096 succeeds but unilink 4096 fails -> likely unilink UDP path issue.",
        "- raw 4096 fails and unilink 4096 fails -> likely environment / WSL2 / OS behavior.",
        "- raw succeeds and unilink succeeds -> strategy pressure model issue.",
    ]

    for row in rows:
        payload = as_int(row, "payload_size")
        send_success = as_int(row, "send_success")
        server_received = as_int(row, "server_received")
        client_received = as_int(row, "client_received")
        echo_matches = as_int(row, "echo_matches")

        if send_success > 0 and server_received == 0:
            notes.append(
                f"- Raw UDP payload `{payload}` had successful sends but zero server receives; investigate OS, WSL2, "
                "socket buffer, or datagram behavior before attributing this to unilink."
            )
        if server_received > 0 and client_received == 0:
            notes.append(
                f"- Raw UDP payload `{payload}` reached the server but did not echo to the client; investigate echo "
                "routing or receive timeout behavior."
            )
        if client_received > echo_matches:
            notes.append(
                f"- Raw UDP payload `{payload}` produced echo mismatches; investigate truncation or payload integrity."
            )

    return notes


def render(rows):
    lines = [
        "# Raw UDP Payload Smoke Summary",
        "",
        "| payload | send success | server received | client received | echo matches | delivery % | match % | timeouts |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for row in rows:
        lines.append(
            f"| {as_int(row, 'payload_size')} | {as_int(row, 'send_success')} | "
            f"{as_int(row, 'server_received')} | {as_int(row, 'client_received')} | "
            f"{as_int(row, 'echo_matches')} | {as_float(row, 'delivery_percent'):.2f} | "
            f"{as_float(row, 'match_percent'):.2f} | {as_int(row, 'timeouts')} |"
        )

    lines.extend(["", "## Notes", ""])
    lines.extend(build_notes(rows))
    return "\n".join(lines).rstrip() + "\n"


def main():
    parser = argparse.ArgumentParser(description="Summarize raw UDP payload smoke benchmark CSV output.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    if not input_path.exists():
        raise SystemExit(f"input CSV does not exist: {input_path}")

    with input_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render(rows), encoding="utf-8")
    print(output_path)


if __name__ == "__main__":
    main()
