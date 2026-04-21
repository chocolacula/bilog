<p align="center">
  <img src="readme/logo.jpg" alt="logo" />
</p>
<p align="center">
  <a href="<https://github.com/chocolacula/bilog/actions/workflows/cmake.yml"><img src="https://github.com/chocolacula/bilog/actions/workflows/cmake.yml/badge.svg"alt="Test" /></a>
</p>

High-performance logging library writing a log message in tens of nanoseconds.

Source code needs **preprocessing** to inject id values inline and emit a schema file, the **postprocessor** uses that schema to decode the binary stream back into human-readable log lines.

## Quick start

### Binary encoder

```cpp
#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kTrace, bilog::BinaryEncoder(), bilog::FileSink("app.bin"));

  bilog::log({}) // {} becomes {0, 0, 1} after preprocessing
      .info("Server started,")
      .i("port:", 8080U)
      .write();
}
```

Then use the preprocessor, run the executable, and decode with postprocessor:

```bash
# Assign IDs and generate schema
preproc -i src/ -o bilog.json

# Decode binary log to text
postproc -s bilog.json -i app.bin -o app.log
```

### Text encoder

```cpp
#include "bilog/codec/text.hpp"
#include "bilog/sink/stdout.hpp"

#define BILOG_ENCODER TextEncoder
#define BILOG_SINK StdoutSink
#include "bilog/bilog.hpp"

int main() {
  bilog::init(bilog::Level::kInfo, bilog::TextEncoder(), bilog::StdoutSink());

  bilog::log({})  // no preprocessing needed
      .info("Server started,")
      .i("port:", 8080U)
      .write();
  // Output: [INFO] Server started, port: 8080
}
```

## Logging API

```cpp
bilog::log({<event_id>, <tag_id>, <tag_id>, ...})
    .info("message")         // level + message (trace/debug/info/warn/error/fatal)
    .i("field:", 42)         // integer (signed or unsigned)
    .f("field:", 3.14)       // float or double
    .b("field:", true)       // boolean
    .s("field:", str_var)    // std::string
    .cs("field:", "literal") // constant string (tag-table reference)
    .write();                // finish record
```

The preprocessor owns the IDs in `log({...})` and keeps them in sync with your code — add, remove, or reorder fields freely. **DO NOT** edit that list manually - it's better to keep it empty and let the preprocessor assign them.

More info in [Architecture Overview](readme/architecture.md)

## Benchmarks

Compares bilog against spdlog with the same message payload.

```sh
------------------------------------------------------------
Benchmark                  Time             CPU   Iterations
------------------------------------------------------------
bilog_write_file        54.7 ns         53.3 ns      13879526
bilog_write_ringbuff    51.9 ns         51.8 ns      13552759
spdlog_write_file        391 ns          386 ns      1822556
```

## Build

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset Debug
cmake --build --preset Debug
```

## Testing

```bash
# Unit tests (C++)
./build/Debug/tests/bilog_test

# Integration tests (Python, requires clang to built binaries)
python3 -m unittest discover -s tests/integration -t . -p '*_test.py' -v
```
