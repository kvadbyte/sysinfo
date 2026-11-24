# sysinfo

A small Linux command-line tool that prints a one-shot system report by reading files from `/proc`. It is written in standard C99 and uses only `<stdio.h>`. All string handling is done with small hand-written helper functions, and every buffer is a fixed-size stack buffer.

## What it reports

| Section | Source file | Details |
|---------|-------------|---------|
| Kernel  | `/proc/version` | Full kernel version line |
| Uptime  | `/proc/uptime`  | Formatted as `X days, HH:MM:SS` |
| Load    | `/proc/loadavg` | 1, 5 and 15 minute load averages |
| CPU     | `/proc/cpuinfo` | First `model name` and the count of logical processors |
| Memory  | `/proc/meminfo` | Used / total in MiB, percentage used, ASCII usage bar, total and available |
| Swap    | `/proc/meminfo` | Used / total in MiB |

Used memory is computed as `MemTotal - MemAvailable`. Used swap is `SwapTotal - SwapFree`.

## Requirements

- Linux (a mounted `/proc` filesystem)
- `gcc` or any C99-compliant compiler

## Build

```
gcc -std=c99 -Wall -Wextra -pedantic -o sysinfo sysinfo.c
```

The code is intended to compile with no warnings under these flags.

## Run

```
./sysinfo
```

No arguments or options are accepted. The program prints the report once and exits with status 0.

## Example output

The values below are illustrative; yours will differ.

```
===== System Information =====
Kernel  : Linux version 6.x ...
Uptime  : 2 days, 03:14:55
Load    : 0.52 0.48 0.40
CPU     : Intel(R) Core(TM) ... (8 logical processors)
Memory  : 5120 / 15872 MiB used (32.3%)
          [######--------------] 32.3%
          total 15872 MiB, available 10752 MiB
Swap    : 0 / 2048 MiB used
```

## Design notes

- **Header constraint:** only `<stdio.h>` is included. Helpers such as `my_strlen`, `starts_with` and `after_colon` replace `<string.h>` functions.
- **Safe input:** every line is read with `fgets` and an explicit buffer size. If a line is longer than the buffer (for example the very long `flags` line in `/proc/cpuinfo`), the rest of it is discarded so it is not mistaken for a new line.
- **Safe output:** the program does not use `sprintf`. The only formatted output goes through `printf` and `fprintf`.
- **Error handling:** every `fopen` result is checked. On failure a message is written to `stderr` and the program continues with the remaining sections. Every `sscanf` return value is checked, and every opened file is closed with `fclose`.
- **Structure:** one function per report section (`print_kernel`, `print_uptime`, `print_load`, `print_cpu`, `print_memory`), with `main()` only calling them.

## Limitations

- Linux only. It relies on the `/proc` file formats.
- On architectures whose `/proc/cpuinfo` has no `model name` field (some ARM systems), the CPU model is shown as `unknown`. The logical processor count still works.
- A system without swap prints `Swap    : 0 / 0 MiB used`.
- The report is a single snapshot and does not refresh.