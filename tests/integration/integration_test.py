import json
import shutil
import subprocess
import tempfile
from textwrap import dedent
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DATA_DIR = PROJECT_ROOT / "tests" / "data"


def find_build_dir():
    for candidate in ["build/Debug", "build/Release"]:
        p = PROJECT_ROOT / candidate
        if (p / "preproc" / "preproc").exists():
            return p
    return None


def run(cmd, cwd=None):
    """Run a command and capture its output."""
    result = subprocess.run(
        [str(c) for c in cmd], cwd=cwd, capture_output=True, text=True
    )
    return result


def compile(workspace):
    """
    Build every .cpp under `workspace` into a single `app` binary using CMake.
    """
    build_dir = workspace / "build"
    configure = run(
        [
            "cmake",
            "-S",
            workspace,
            "-B",
            build_dir,
            f"-DBILOG_ROOT={PROJECT_ROOT}",
        ]
    )
    assert configure.returncode == 0, f"cmake configure failed:\n{configure.stderr}"
    build = run(["cmake", "--build", build_dir, "--target", "app"])
    assert build.returncode == 0, f"cmake build failed:\n{build.stderr}"
    return build_dir / "app"


class TestIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build_dir = find_build_dir()
        if cls.build_dir is None:
            raise unittest.SkipTest("build directory not found")

        cls.preproc = cls.build_dir / "preproc" / "preproc"
        if not cls.preproc.exists():
            raise unittest.SkipTest(f"preproc not found at {cls.preproc}")

        cls.postproc = cls.build_dir / "postproc" / "postproc"
        if not cls.postproc.exists():
            raise unittest.SkipTest(f"postproc not found at {cls.postproc}")

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="bilog_")
        self.workspace = Path(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_full_pipeline(self):
        """
        preproc scans a multi-file project recursively, assigns IDs globally,
        and emits a single schema that postproc consumes to reproduce the log.
        """
        source_dir = DATA_DIR / "multi_file"
        expected_file = source_dir / "expected.log"

        workspace = self.workspace / "project"
        shutil.copytree(source_dir, workspace)

        schema_file = workspace / "schema.json"
        bin_file = workspace / "log.bin"
        log_file = workspace / "output.log"

        # Preprocess the whole directory (recurses, produces unified schema).
        run([self.preproc, "-i", workspace, "-o", schema_file])
        self.assertTrue(schema_file.exists(), "Schema not created")

        schema = json.loads(schema_file.read_text())

        # Global tag dedup: "node:" is referenced in every fixture file but
        # appears exactly once in the schema.
        tag_names = set(schema["tags"].values())
        for tag in ("port:", "node:", "percent:", "retries:"):
            self.assertIn(tag, tag_names)
        # One event ID per bilog::log call across all files.
        self.assertEqual(len(schema["events"]), 4)

        # Extension filter: tags that appear only inside a .md file under a
        # nested subdirectory must not leak into the schema.
        for md_only_tag in ("markdown_only_tag:", "markdown_only_node:"):
            self.assertNotIn(md_only_tag, tag_names)

        # Compile all .cpp in the project together into one binary via CMake.
        test_binary = compile(workspace)

        run([test_binary], cwd=workspace)
        self.assertTrue(bin_file.exists(), "Binary log not created")
        self.assertGreater(bin_file.stat().st_size, 0, "Binary log is empty")

        run([self.postproc, "-s", schema_file, "-i", bin_file, "-o", log_file])
        self.assertTrue(log_file.exists(), "Output log not created")

        self.assertEqual(log_file.read_text(), expected_file.read_text())

    def test_preproc_idempotent(self):
        """Running preproc twice produces the same source and schema"""
        source_file = DATA_DIR / "single_file" / "main.cpp"
        test_source = self.workspace / "test.cpp"
        shutil.copy2(source_file, test_source)
        schema_file = self.workspace / "schema.json"

        source0 = test_source.read_text()

        run([self.preproc, "-i", test_source, "-o", schema_file])
        source1 = test_source.read_text()
        schema1 = schema_file.read_text()

        self.assertNotEqual(source0, source1)

        run([self.preproc, "-i", test_source, "-o", schema_file])
        source2 = test_source.read_text()
        schema2 = schema_file.read_text()

        self.assertEqual(source1, source2)
        self.assertEqual(json.loads(schema1), json.loads(schema2))

    def test_preproc_upd_field(self):
        """Adding a field preserves existing tag IDs"""
        source_file = DATA_DIR / "single_file" / "main.cpp"
        test_source = self.workspace / "test.cpp"
        shutil.copy2(source_file, test_source)
        schema_file = self.workspace / "schema.json"

        run([self.preproc, "-i", test_source, "-o", schema_file])
        source = test_source.read_text()

        test_source.write_text(
            source.replace(
                '.cs("node:", "stage1")',
                '.cs("node:", "stage1").i("count:", 1U)',
            )
        )
        run([self.preproc, "-i", test_source, "-o", schema_file])

        schema = json.loads(schema_file.read_text())

        tags = {v: k for k, v in schema["tags"].items()}
        self.assertIn("port:", tags)
        self.assertIn("node:", tags)
        self.assertIn("count:", tags)
        self.assertIn("date:", tags)

        event = list(schema["events"]["0"])
        self.assertEqual(event, ["i", "cs", "i", "s"])

    def test_preproc_missing_write(self):
        """preproc errors on bilog::log() chain without .write()"""
        test_source = self.workspace / "test.cpp"
        # fmt: off
        test_source.write_text(dedent("""\
            #include "bilog/bilog.hpp"
            void f() {
              bilog::log({}).info("oops").i("n:", 1U);
            }
            """)
        )
        # fmt: on
        schema_file = self.workspace / "schema.json"

        result = run([self.preproc, "-i", test_source, "-o", schema_file])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing .write()", result.stderr)

    def _run_app(self):
        """Produce a working (schema, bin) pair by running the empty_id pipeline."""
        workspace = self.workspace / "postproc_fixture"
        shutil.copytree(DATA_DIR / "single_file", workspace)
        schema_file = workspace / "schema.json"
        bin_file = workspace / "log.bin"

        run([self.preproc, "-i", workspace, "-o", schema_file])
        test_binary = compile(workspace)
        run([test_binary], cwd=workspace)
        return schema_file, bin_file

    def test_postproc_stdout_path(self):
        """postproc without -o prints to stdout without errors."""
        schema_file, bin_file = self._run_app()

        result = run([self.postproc, "-s", schema_file, "-i", bin_file])
        # stderr carries the diagnostic message from the stdout branch
        self.assertIn("Formatted 3 log entries to stdout", result.stderr)

    def test_postproc_missing_input(self):
        schema_file, _ = self._run_app()
        missing = self.workspace / "does_not_exist.bin"

        result = run([self.postproc, "-s", schema_file, "-i", missing])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Failed to open", result.stderr)

    def test_postproc_malformed_schema(self):
        _, bin_file = self._run_app()
        bad_schema = self.workspace / "broken.json"
        bad_schema.write_text("this is not json")

        result = run([self.postproc, "-s", bad_schema, "-i", bin_file])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Failed to parse schema", result.stderr)

    def test_postproc_missing_required_arg(self):
        result = run([self.postproc])  # no -s, no -i
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
