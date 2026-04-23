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

## Tests

- **Prefer table-driven tests via a `check` lambda.** When N tests share identical logic and differ only by inputs / expected outputs, write them as a table: one `TEST` body with a local `check` lambda whose first parameter is a `const char* name` identifier passed to `SCOPED_TRACE`, followed by a flat list of `check(...)` calls — each line is one row in the table. No trailing `//` comments — put the case description in the `name` argument so it appears in gtest's failure output.

  Do not write:
  ```cpp
  TEST(Suite, PositiveCase) { EXPECT_EQ(fn(1), 2); }
  TEST(Suite, NegativeCase) { EXPECT_EQ(fn(-1), -2); }  // negative branch
  TEST(Suite, ZeroCase)     { EXPECT_EQ(fn(0), 0); }
  ```

  Always write:
  ```cpp
  TEST(Suite, Fn) {
    auto check = [](const char* name, int input, int expected) {
      SCOPED_TRACE(name);
      EXPECT_EQ(fn(input), expected);
    };
    check("positive", 1, 2);
    check("negative", -1, -2);
    check("zero", 0, 0);
  }
  ```

  Use `const char* name` (not `std::string_view`) so clang-tidy's "adjacent similar-type parameters" warning doesn't fire when the next parameter is also a string-like type. Keep the lambda body minimal — a single `EXPECT_EQ` is ideal. When the lambda branches on `expected`, prefer `EXPECT_EQ` against a `std::optional<T>` over `if expected: ASSERT_TRUE else: EXPECT_FALSE` patterns.

  Exception: don't use this pattern when per-case setup/teardown differs meaningfully (e.g. one case needs a temp file, another doesn't).
