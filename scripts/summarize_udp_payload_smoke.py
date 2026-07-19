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


def as_bool(row, field):
    return str(row.get(field, "")).strip().lower() in ("1", "true", "yes")


def build_notes(rows):
    notes = []
    for row in rows:
        payload = as_int(row, "payload_size")
        send_success = as_int(row, "send_success")
        server_received = as_int(row, "server_received")
        client_received = as_int(row, "client_received")
        echo_matches = as_int(row, "echo_matches")
        send_attempts = as_int(row, "send_attempts")
        iterations = as_int(row, "iterations")
        stopped_early = as_bool(row, "stopped_early") or (iterations > 0 and send_attempts < iterations)
        stop_reason = row.get("stop_reason", "") or "unknown"

        if payload == 4096 and client_received == 0:
            notes.append(
                "- UDP 4096-byte payload reported zero echo delivery in this run. Treat this as a "
                "benchmark-model, environment, or core UDP behavior issue requiring separate classification."
            )
        if send_success > 0 and server_received == 0:
            notes.append(
                "- Send calls succeeded but the server did not observe datagrams. This suggests endpoint "
                "routing, environment, or datagram delivery behavior should be investigated."
            )
        if server_received > 0 and client_received == 0:
            notes.append(
                "- Server received datagrams but client did not receive echoes. This suggests echo response "
                "routing or client receive behavior should be investigated."
            )
        if client_received > echo_matches:
            notes.append(
                "- Echo payload mismatch detected. This suggests payload corruption, truncation, or benchmark "
                "payload reconstruction behavior should be investigated."
            )
        if stopped_early:
            notes.append(
                f"- One or more rows stopped early after repeated timeouts to keep diagnostic runs bounded "
                f"(`stop_reason={stop_reason}`)."
            )

    if not notes:
        return ["- No UDP payload smoke anomalies detected."]

    unique = []
    seen = set()
    for note in notes:
        if note not in seen:
            unique.append(note)
            seen.add(note)
    return unique


def render(rows):
    lines = [
        "# UDP Payload Smoke Summary",
        "",
        "| strategy | payload | send success | server received | client received | echo matches | delivery % | match % | timeouts |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for row in rows:
        lines.append(
            f"| {row.get('strategy', '')} | {as_int(row, 'payload_size')} | {as_int(row, 'send_success')} | "
            f"{as_int(row, 'server_received')} | {as_int(row, 'client_received')} | "
            f"{as_int(row, 'echo_matches')} | {as_float(row, 'delivery_percent'):.2f} | "
            f"{as_float(row, 'match_percent'):.2f} | {as_int(row, 'timeouts')} |"
        )

    lines.extend(["", "## Notes", ""])
    lines.extend(build_notes(rows))

    lines.extend(
        [
            "",
            "## Classification Guide",
            "",
            "- raw UDP succeeds + Wirestead UDP fails -> investigate the Wirestead UDP path.",
            "- raw UDP fails + Wirestead UDP fails -> investigate environment, OS, WSL2, socket buffer, or datagram behavior.",
            "- raw UDP succeeds + Wirestead UDP succeeds + strategy fails -> investigate the pressure benchmark model.",
        ]
    )

    if rows and "runtime_stats_supported" in rows[0]:
        lines.extend(["", "## RuntimeStats", ""])
        if any(as_bool(row, "runtime_stats_supported") for row in rows):
            lines.extend(
                [
                    "| strategy | payload | client failed sends | client drops | server messages received | server bytes received | server failed sends | server drops |",
                    "|---|---:|---:|---:|---:|---:|---:|---:|",
                ]
            )
            for row in rows:
                lines.append(
                    f"| {row.get('strategy', '')} | {as_int(row, 'payload_size')} | "
                    f"{as_int(row, 'client_failed_sends')} | {as_int(row, 'client_dropped_messages')} | "
                    f"{as_int(row, 'server_messages_received')} | {as_int(row, 'server_bytes_received')} | "
                    f"{as_int(row, 'server_failed_sends')} | {as_int(row, 'server_dropped_messages')} |"
                )
        else:
            lines.append("- RuntimeStats columns are present but unavailable for this selected Wirestead ref.")
    return "\n".join(lines).rstrip() + "\n"


def main():
    parser = argparse.ArgumentParser(description="Summarize UDP payload smoke benchmark CSV output.")
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
