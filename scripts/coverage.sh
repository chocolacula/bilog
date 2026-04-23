#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="$PWD/build/Debug"
PROF_DIR="$BUILD_DIR/coverage"
MERGED="$PROF_DIR/merged.profdata"
IGNORE_REGEX='(/build/|/tests/|/benches/|/examples/|_test\.cpp)'

mkdir -p "$PROF_DIR"
rm -f "$PROF_DIR"/*.profraw "$MERGED"

cmake --preset Debug \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate"
cmake --build --preset Debug

# Run C++ unit tests.
LLVM_PROFILE_FILE="$PROF_DIR/unit-%m.profraw" \
  ctest --preset Debug --output-on-failure

# Run Python integration tests. The Python harness invokes preproc/postproc
# as subprocesses; LLVM_PROFILE_FILE is inherited via the environment.
LLVM_PROFILE_FILE="$PROF_DIR/integ-%m.profraw" \
  python3 -m unittest discover -s tests/integration -t . -p '*_test.py' -v

xcrun llvm-profdata merge -sparse "$PROF_DIR"/*.profraw -o "$MERGED"

xcrun llvm-cov show \
  -object "$BUILD_DIR/tests/bilog_test" \
  -object "$BUILD_DIR/preproc/preproc" \
  -object "$BUILD_DIR/postproc/postproc" \
  -ignore-filename-regex="$IGNORE_REGEX" \
  -instr-profile="$MERGED" \
  -format=html \
  -output-dir="$PROF_DIR/html"

echo
echo "HTML report: $PROF_DIR/html/index.html"
open $PROF_DIR/html/index.html
