#!/usr/bin/env python3
import json
import os
import platform
import shutil
import socket
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def run(command, timeout_seconds=5):
    executable = shutil.which(command[0])
    if executable is None:
        return None
    try:
        completed = subprocess.run(
            [executable, *command[1:]],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=timeout_seconds,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    return completed.stdout.strip()


def first_line(value):
    if not value:
        return None
    return value.splitlines()[0].strip()


def read_text_file(path):
    file_path = Path(path)
    if not file_path.exists():
        return None
    value = file_path.read_text(encoding="utf-8", errors="replace").replace("\x00", "").strip()
    return value or None


def read_os_release():
    path = Path("/etc/os-release")
    if not path.exists():
        return {}

    values = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in line or line.startswith("#"):
            continue
        key, raw_value = line.split("=", 1)
        values[key] = raw_value.strip().strip('"')
    return values


def read_meminfo():
    path = Path("/proc/meminfo")
    if not path.exists():
        return {}

    values = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if ":" not in line:
            continue
        key, raw_value = line.split(":", 1)
        value = raw_value.strip().split()[0]
        if value.isdigit():
            values[key] = int(value)
    return values


def parse_lscpu():
    output = run(["lscpu"])
    if not output:
        return {}

    values = {}
    wanted = {
        "Architecture": "architecture",
        "CPU(s)": "logical_cpus",
        "Thread(s) per core": "threads_per_core",
        "Core(s) per socket": "cores_per_socket",
        "Socket(s)": "sockets",
        "Model name": "model_name",
        "CPU max MHz": "cpu_max_mhz",
        "CPU min MHz": "cpu_min_mhz",
        "L1d cache": "l1d_cache",
        "L1i cache": "l1i_cache",
        "L2 cache": "l2_cache",
        "L3 cache": "l3_cache",
        "NUMA node(s)": "numa_nodes",
        "Virtualization": "virtualization",
        "Hypervisor vendor": "hypervisor_vendor",
    }
    for line in output.splitlines():
        if ":" not in line:
            continue
        key, raw_value = line.split(":", 1)
        mapped = wanted.get(key.strip())
        if not mapped:
            continue
        value = raw_value.strip()
        values[mapped] = int(value) if value.isdigit() else value

    logical = values.get("logical_cpus")
    threads = values.get("threads_per_core")
    if isinstance(logical, int) and isinstance(threads, int) and threads > 0:
        values["physical_cores_estimate"] = logical // threads
    return values


def read_thermal_zones():
    zones = []
    for zone in sorted(Path("/sys/class/thermal").glob("thermal_zone*")):
        zone_type = read_text_file(zone / "type")
        temp_raw = read_text_file(zone / "temp")
        temp_millic = int(temp_raw) if temp_raw and temp_raw.lstrip("-").isdigit() else None
        zones.append(
            {
                "name": zone.name,
                "type": zone_type,
                "temp_millicelsius": temp_millic,
            }
        )
    return zones


def collect_jetson_info():
    model = read_text_file("/proc/device-tree/model") or read_text_file("/sys/firmware/devicetree/base/model")
    l4t_release = read_text_file("/etc/nv_tegra_release")
    nvpmodel = run(["nvpmodel", "-q"])
    jetson_clocks = run(["jetson_clocks", "--show"])

    info = {
        "model": model,
        "l4t_release": l4t_release,
        "nvpmodel_query": nvpmodel,
        "jetson_clocks_show": jetson_clocks,
    }
    info["detected"] = any(value for value in info.values())
    return info


def collect_environment():
    os_release = read_os_release()
    meminfo = read_meminfo()
    lscpu = parse_lscpu()

    data = {
        "timestamp_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "hostname": socket.gethostname(),
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "uname": " ".join(platform.uname()),
        },
        "os": {
            "pretty_name": os_release.get("PRETTY_NAME"),
            "id": os_release.get("ID"),
            "version_id": os_release.get("VERSION_ID"),
        },
        "hardware": {
            "cpu": lscpu,
            "memory": {
                "mem_total_kib": meminfo.get("MemTotal"),
                "swap_total_kib": meminfo.get("SwapTotal"),
            },
            "thermal_zones": read_thermal_zones(),
            "jetson": collect_jetson_info(),
        },
        "tools": {
            "cmake": first_line(run(["cmake", "--version"])),
            "cxx": first_line(run([os.environ.get("CXX", "c++"), "--version"])),
            "git": first_line(run(["git", "--version"])),
        },
        "github_actions": {
            "runner_name": os.environ.get("RUNNER_NAME"),
            "runner_os": os.environ.get("RUNNER_OS"),
            "runner_arch": os.environ.get("RUNNER_ARCH"),
            "runner_environment": os.environ.get("RUNNER_ENVIRONMENT"),
            "github_repository": os.environ.get("GITHUB_REPOSITORY"),
            "github_run_id": os.environ.get("GITHUB_RUN_ID"),
            "github_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
            "github_sha": os.environ.get("GITHUB_SHA"),
            "github_ref": os.environ.get("GITHUB_REF"),
        },
    }
    return data


def write_text_report(data, path):
    cpu = data["hardware"]["cpu"]
    memory = data["hardware"]["memory"]
    lines = [
        f"timestamp_utc: {data['timestamp_utc']}",
        f"hostname: {data['hostname']}",
        f"os: {data['os'].get('pretty_name') or data['platform']['system']}",
        f"kernel: {data['platform']['release']}",
        f"architecture: {data['platform']['machine']}",
        f"cpu_model: {cpu.get('model_name')}",
        f"logical_cpus: {cpu.get('logical_cpus')}",
        f"physical_cores_estimate: {cpu.get('physical_cores_estimate')}",
        f"threads_per_core: {cpu.get('threads_per_core')}",
        f"sockets: {cpu.get('sockets')}",
        f"numa_nodes: {cpu.get('numa_nodes')}",
        f"l1d_cache: {cpu.get('l1d_cache')}",
        f"l1i_cache: {cpu.get('l1i_cache')}",
        f"l2_cache: {cpu.get('l2_cache')}",
        f"l3_cache: {cpu.get('l3_cache')}",
        f"mem_total_kib: {memory.get('mem_total_kib')}",
        f"swap_total_kib: {memory.get('swap_total_kib')}",
        f"jetson_model: {data['hardware']['jetson'].get('model')}",
        f"jetson_l4t_release: {data['hardware']['jetson'].get('l4t_release')}",
        f"jetson_nvpmodel: {first_line(data['hardware']['jetson'].get('nvpmodel_query'))}",
        f"jetson_clocks: {first_line(data['hardware']['jetson'].get('jetson_clocks_show'))}",
        f"cmake: {data['tools'].get('cmake')}",
        f"cxx: {data['tools'].get('cxx')}",
        f"git: {data['tools'].get('git')}",
        f"runner_name: {data['github_actions'].get('runner_name')}",
        f"runner_os: {data['github_actions'].get('runner_os')}",
        f"runner_arch: {data['github_actions'].get('runner_arch')}",
        f"github_repository: {data['github_actions'].get('github_repository')}",
        f"github_run_id: {data['github_actions'].get('github_run_id')}",
        f"github_sha: {data['github_actions'].get('github_sha')}",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    output_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build/environment")
    output_dir.mkdir(parents=True, exist_ok=True)

    data = collect_environment()
    (output_dir / "hardware.json").write_text(
        json.dumps(data, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    write_text_report(data, output_dir / "environment.txt")

    print(output_dir / "hardware.json")
    print(output_dir / "environment.txt")


if __name__ == "__main__":
    main()
