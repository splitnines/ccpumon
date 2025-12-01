# ccpumon

Monitor the CPU of a Cisco IOS‑XE device via an SSH session in real time.

## Overview

`ccpumon` is a lightweight command‑line tool written in C that connects via SSH to a Cisco IOS‑XE device and monitors CPU usage in real time. It’s designed for network engineers and system administrators who want a simple, portable way to keep tabs on switch/router CPU load without relying on SNMP, web UIs, or heavy monitoring frameworks.

## Features

- Real‑time CPU utilization reporting of a Cisco IOS‑XE device over SSH.
- Minimal dependencies — built with plain C and a Makefile.
- Works with standard SSH credentials — no need to install additional agents.

## Requirements

- A Cisco IOS‑XE device reachable via SSH.
- A system with an SSH client and a C toolchain (e.g., `gcc` + `make`).
- Access credentials (username/password or key) with privileges to view CPU statistics on the IOS‑XE device.

## Building & Installation

From the top‑level directory:

```sh
make && make install
```

## Usage

Typical usage is:

```sh
ccpumon <host name or IP addewss> <username>
```

Example:

```sh
ccpumon 192.168.1.1 admin
```

## Use Cases

- Quickly check real‑time CPU usage on IOS‑XE devices with no SNMP configured.
- Useful for lab networks during testing or production to monitor a suspect device.
## License

This project is released under the MIT License.
