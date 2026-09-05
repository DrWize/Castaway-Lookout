"""Execute the firmware login handler with host-side NVS/HTTP fault injection."""

import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "johnny-esp32/components/jcnet/jcnet.c"
HARNESS = pathlib.Path(__file__).with_name("login_session_harness.c")


class LoginSessionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get("CC") or shutil.which("gcc")
        if not compiler and pathlib.Path("C:/msys64/mingw64/bin/gcc.exe").exists():
            compiler = "C:/msys64/mingw64/bin/gcc.exe"
        if not compiler:
            raise RuntimeError("A host C compiler is required; set CC or add gcc to PATH")
        cls.temp = tempfile.TemporaryDirectory(prefix="johnny-login-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = pathlib.Path(cls.temp.name)
        source = SOURCE.read_text(encoding="utf-8")
        start = source.index("static esp_err_t session_storage_error(")
        end = source.index("static esp_err_t logout_handler(", start)
        (directory / "session_under_test.c").write_text(source[start:end], encoding="utf-8")
        cls.executable = directory / ("login.exe" if os.name == "nt" else "login")
        environment = os.environ.copy()
        environment["PATH"] = str(pathlib.Path(compiler).parent) + os.pathsep + environment["PATH"]
        result = subprocess.run(
            [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(directory),
             str(HARNESS), "-o", str(cls.executable)],
            capture_output=True, text=True, env=environment,
        )
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout or "Host C compilation failed")

    def test_login_storage_paths(self):
        for scenario in ("wrong", "missing", "success", "open", "write", "commit", "stats"):
            with self.subTest(scenario=scenario):
                subprocess.run([str(self.executable), scenario], check=True,
                               capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
