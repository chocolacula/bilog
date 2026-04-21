# Instructions for Claude

## Coding style

- **Always brace control-flow bodies.** Every `if`/`else`/`for`/`while`/`do` body must be wrapped in `{ }`, even for single-statement bodies. The repo's clang-tidy surfaces brace-less bodies as warnings.

  Do not write:
  ```cpp
  if (cond) return;
  for (auto x : v) ++x;
  ```

  Always write:
  ```cpp
  if (cond) {
    return;
  }
  for (auto x : v) {
    ++x;
  }
  ```

  Applies to early-returns, short guards, and range-for bodies alike.

- **No non-const references for mutable parameters.** Use pointers for mutable out-parameters; `const T&` stays fine for read-only inputs. Pointer syntax at the call site makes mutation visible.

  Do not write:
  ```cpp
  void format(Buffer<SinkT>& lb, SinkT& sink);
  ```

  Always write:
  ```cpp
  void format(Buffer<SinkT>* lb, SinkT* sink);
  ```

  Exceptions: operator overloads, move references (`T&&`), range-for loop variables, and types imposed by external APIs (e.g., `benchmark::State&` required by Google Benchmark).
