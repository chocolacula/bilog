# Architecture Overview

## Preprocessor

Scans C++ sources for `bilog::log()` calls, assigns event and tag IDs, and generates a schema JSON file.

```bash
preproc -i src/ -o bilog.json
```

Features:

- Separate ID counters for events and tags
- Loads existing schema to preserve stable ID assignment
- Idempotent -- running twice produces the same result
- Removes stale entries when log calls are deleted

Schema format:

```json
{
  "tags": {
    "0": "Server started",
    "1": "port:"
  },
  "events": {
    "0": ["i"]
  }
}
```

`tags` maps tag IDs to their string names. `events` maps event IDs to the ordered list of dynamic field types — one entry per chained field call. Type codes: `i` integer, `f` float/double, `b` bool, `s` string, `cs` constant string.

## Postprocessor

Reads a schema and a binary log file, writes human-readable text. Streams the log via `std::ifstream` — the input file is never fully loaded into RAM.

```bash
# To file
postproc -s bilog.json -i app.bin -o app.log

# To stdout
postproc -s bilog.json -i app.bin
```

## Sinks

### FileSink

```cpp
bilog::FileSink sink("app.bin");
```

- Per-thread 8 KB staging buffer, flushed to the underlying `std::ofstream` on overflow or when the `Buffer` is destroyed
- Internal mutex — safe to share across threads
- Supports move semantics

### StdoutSink

```cpp
bilog::StdoutSink sink;
```

- Detects TTY via `isatty()` -- colors on terminal, plain text on pipes
- Colorizes `[INFO]`, `[ERROR]`, etc. with ANSI escape codes
- Buffers per line, flushes on `\n`

### RingBuffSink

```cpp
bilog::RingBuffSink sink(65536);  // 64 KB ring buffer
```

- Lock-free, multi-producer / single-consumer — records commit atomically via `fetch_add`
- Fixed size, oldest data overwritten when full
- `read()` to drain into a separate buffer
- Header-only, no `.cpp`

## Customizing encoder and sink

Define before including `bilog.hpp`:

```cpp
#define BILOG_ENCODER TextEncoder
#define BILOG_SINK StdoutSink
#include "bilog/bilog.hpp"
```

Or via CMake:

```cmake
target_compile_definitions(my_app PRIVATE BILOG_ENCODER=TextEncoder BILOG_SINK=StdoutSink)
```

Defaults: `BinaryEncoder` + `FileSink`.

## Wire format

Each log record in a binary stream is a sequence of byte pairs:

`[event_id, level] [msg_tag, 0] [field_tag, value] ...`

Each pair has a 1-byte header `[hdr_a:4][hdr_b:4]` where each 4-bit nibble is `[neg:1][width-1:3]`. Payloads are raw little-endian bytes of the declared `width` (1-8).

- Unsigned integers and non-negative signed integers: `neg=0`, stored raw at the narrowest width.
- Negative signed integers: zigzag-encoded, `neg=1`.
- Floats: `width=4` raw IEEE-754. Doubles: `width=8`.
- Strings: the pair header's `width` bytes encode the length; the string payload follows inline.
- Bools: `width=1`, payload `0x00` or `0x01`.

Records have no terminator — the schema's per-event field list tells the decoder how many dynamic pairs follow the fixed header.
