import json
import shutil
import subprocess
import tempfile
from textwrap import dedent
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DATA_DIR = PROJECT_ROOT / "tests" / "data"

# Each entry: (name, source_file, expected_log, bin_filename)
# bin_filename must match what the source writes via ::open()
PIPELINE_CASES = [
    ("empty_id", "empty_id.cpp", "empty_id.log", "test_output.bin"),
    ("valid_id", "valid_id.cpp", "valid_id.log", "test_output.bin"),
]


def find_build_dir():
    for candidate in ["build/Debug", "build/Release"]:
        p = PROJECT_ROOT / candidate
        if (p / "preproc" / "preproc").exists():
            return p
    return None


def run(cmd, cwd=None):
    result = subprocess.run(
        [str(c) for c in cmd], cwd=cwd, capture_output=True, text=True
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(str(c) for c in cmd)}\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}"
        )
    return result


def compile(source, output):
    run(
        [
            # fmt: off
            "clang++", "-std=c++23", "-stdlib=libc++",
            "-I", PROJECT_ROOT / "bilog" / "include",
            "-o", output,
            source, PROJECT_ROOT / "bilog" / "src" / "sink" / "file.cpp",
            # fmt: on
        ]
    )


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
        """preproc -> compile -> run -> postproc -> compare expected output"""
        for name, source_name, expected_name, bin_name in PIPELINE_CASES:
            with self.subTest(name):
                workspace = self.workspace / name
                workspace.mkdir()

                source_file = DATA_DIR / source_name
                expected_file = DATA_DIR / expected_name

                test_source = workspace / "app.cpp"
                shutil.copy2(source_file, test_source)

                schema_file = workspace / "schema.json"
                bin_file = workspace / bin_name
                log_file = workspace / "output.log"
                test_binary = workspace / "app"

                # Preprocess
                run([self.preproc, "-i", test_source, "-o", schema_file])
                self.assertTrue(schema_file.exists(), "Schema not created")

                # Compile
                compile(test_source, test_binary)

                # Run
                run([test_binary], cwd=workspace)
                self.assertTrue(bin_file.exists(), "Binary log not created")
                self.assertGreater(bin_file.stat().st_size, 0, "Binary log is empty")

                # Postprocess
                run([self.postproc, "-s", schema_file, "-i", bin_file, "-o", log_file])
                self.assertTrue(log_file.exists(), "Output log not created")

                # Compare
                actual = log_file.read_text()
                expected = expected_file.read_text()
                self.assertEqual(actual, expected)

    def test_preproc_idempotent(self):
        """Running preproc twice produces the same source and schema"""
        source_file = DATA_DIR / "empty_id.cpp"
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
        source_file = DATA_DIR / "empty_id.cpp"
        test_source = self.workspace / "test.cpp"
        shutil.copy2(source_file, test_source)
        schema_file = self.workspace / "schema.json"

        run([self.preproc, "-i", test_source, "-o", schema_file])
        source = test_source.read_text()

        test_source.write_text(
            source.replace(
                '.cstr("node:", "stage1")',
                '.cstr("node:", "stage1").num("count:", 1U)',
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
        self.assertEqual(len(event), 6)

    def test_preproc_missing_write(self):
        """preproc errors on bilog::log() chain without .write()"""
        test_source = self.workspace / "test.cpp"
        # fmt: off
        test_source.write_text(dedent("""\
            #include "bilog/bilog.hpp"
            void f() {
              bilog::log({}).info("oops").num("n:", 1U);
            }
            """)
        )
        # fmt: on
        schema_file = self.workspace / "schema.json"

        result = subprocess.run(
            [str(self.preproc), "-i", str(test_source), "-o", str(schema_file)],
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing .write()", result.stderr)


if __name__ == "__main__":
    unittest.main()
