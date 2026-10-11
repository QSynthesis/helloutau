"""Test of hello-updater, which replaces an installation directory with a new one.

    python3 test_updater.py <path to hello-updater>

Runs the program on directories in a temporary directory and checks the directories, the exit
status and the log afterwards. The names of the directories contain non-ASCII characters, so that
a conversion of the paths to a narrow code page fails the test.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

UPDATER = os.path.abspath(sys.argv[1])
WINDOWS = sys.platform == "win32"

# The longest time a run may take. The rename of a directory in use fails for 10 seconds before
# the program gives up.
RUN_LIMIT = 60

# A Python without a console window on Windows, so that a started program shows no window
if WINDOWS:
    PYTHON = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
else:
    PYTHON = sys.executable

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print("FAIL: " + message)


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as file:
        file.write(text)


def read(path):
    with open(path, encoding="utf-8") as file:
        return file.read()


class Installation:
    """An installation directory with one file, and a new version beside it."""

    def __init__(self):
        self.root = tempfile.mkdtemp()
        base = os.path.join(self.root, "音源 apps")
        self.target = os.path.join(base, "HelloUtau")
        self.source = self.target + ".new"
        self.old = self.target + ".old"
        self.log = os.path.join(self.root, "updater.log")
        write(os.path.join(self.target, "bin", "version.txt"), "1")
        write(os.path.join(self.source, "bin", "version.txt"), "2")

    def version(self):
        return read(os.path.join(self.target, "bin", "version.txt"))

    def run(self, *arguments, cwd=None):
        command = [UPDATER, "--target", self.target, "--source", self.source, "--log", self.log,
                   *arguments]
        return subprocess.run(command, timeout=RUN_LIMIT, cwd=cwd).returncode

    def log_text(self):
        return read(self.log) if os.path.exists(self.log) else ""

    def remove(self):
        shutil.rmtree(self.root, ignore_errors=True)


def wait_for(path, limit=10.0):
    deadline = time.monotonic() + limit
    while time.monotonic() < deadline:
        if os.path.exists(path):
            return True
        time.sleep(0.05)
    return False


def marker_arguments(marker):
    # The arguments contain a space, a quotation mark and trailing backslashes, which the
    # command line of Windows must preserve.
    script = "import sys; open(sys.argv[1], 'w', encoding='utf-8').write(sys.argv[2])"
    return ["--launch", PYTHON, "--", "-c", script, marker, 'a "quoted" text\\\\']


def test_the_directory_is_replaced():
    installation = Installation()
    try:
        check(installation.run() == 0, "replacement: exit status")
        check(installation.version() == "2", "replacement: the new version")
        check(not os.path.exists(installation.source), "replacement: the new directory remains")
        check(not os.path.exists(installation.old), "replacement: the old directory remains")
        check("replaced" in installation.log_text(), "replacement: the log")
    finally:
        installation.remove()


def test_a_working_directory_inside_the_target_is_left():
    # The application starts the program with its own working directory, which may lie inside
    # the installation directory. On Windows that directory cannot be renamed while it is the
    # working directory of a process.
    installation = Installation()
    try:
        status = installation.run(cwd=os.path.join(installation.target, "bin"))
        check(status == 0, "working directory: exit status %d" % status)
        check(installation.version() == "2", "working directory: the new version")
    finally:
        installation.remove()


def test_a_remaining_old_directory_is_removed_first():
    installation = Installation()
    try:
        write(os.path.join(installation.old, "stale.txt"), "stale")
        check(installation.run() == 0, "remaining old directory: exit status")
        check(installation.version() == "2", "remaining old directory: the new version")
        check(not os.path.exists(installation.old), "remaining old directory: not removed")
    finally:
        installation.remove()


def test_a_missing_source_changes_nothing():
    installation = Installation()
    try:
        shutil.rmtree(installation.source)
        check(installation.run() == 3, "missing source: exit status")
        check(installation.version() == "1", "missing source: the old version")
    finally:
        installation.remove()


def test_invalid_arguments_are_rejected():
    for arguments in (["--target"], ["--source", "x"], ["--pid", "12x", "--target", "a",
                                                        "--source", "b"],
                      ["--unknown", "1", "--target", "a", "--source", "b"]):
        status = subprocess.run([UPDATER, *arguments], timeout=RUN_LIMIT,
                                stderr=subprocess.DEVNULL).returncode
        check(status == 1, "invalid arguments %r: exit status %d" % (arguments, status))


def test_the_process_is_awaited():
    installation = Installation()
    sleeper = subprocess.Popen([PYTHON, "-c", "import time; time.sleep(2)"])
    try:
        start = time.monotonic()
        status = installation.run("--pid", str(sleeper.pid))
        elapsed = time.monotonic() - start
        check(status == 0, "wait: exit status")
        check(sleeper.poll() is not None, "wait: the process is still running")
        check(elapsed >= 1.0, "wait: the program did not wait")
        check(installation.version() == "2", "wait: the new version")
    finally:
        sleeper.kill()
        sleeper.wait()
        installation.remove()


def test_a_process_that_does_not_exit_stops_the_update():
    installation = Installation()
    sleeper = subprocess.Popen([PYTHON, "-c", "import time; time.sleep(60)"])
    try:
        status = installation.run("--pid", str(sleeper.pid), "--timeout", "1")
        check(status == 2, "timeout: exit status")
        check(installation.version() == "1", "timeout: the old version")
        check(os.path.exists(installation.source), "timeout: the new directory is removed")
    finally:
        sleeper.kill()
        sleeper.wait()
        installation.remove()


def test_the_program_is_started_after_the_replacement():
    installation = Installation()
    try:
        marker = os.path.join(installation.root, "音 marker.txt")
        check(installation.run(*marker_arguments(marker)) == 0, "start: exit status")
        check(wait_for(marker), "start: the program did not run")
        if os.path.exists(marker):
            time.sleep(0.2)
            check(read(marker) == 'a "quoted" text\\\\', "start: the arguments: %r" % read(marker))
    finally:
        installation.remove()


def test_a_failed_replacement_restores_the_old_directory():
    # Only Windows prevents the rename of a directory in which a file is open.
    if not WINDOWS:
        print("SKIP: the rename of a directory in use succeeds on this system")
        return
    installation = Installation()
    try:
        marker = os.path.join(installation.root, "marker.txt")
        with open(os.path.join(installation.source, "bin", "version.txt"), encoding="utf-8"):
            status = installation.run(*marker_arguments(marker))
        check(status == 3, "restoration: exit status %d" % status)
        check(installation.version() == "1", "restoration: the old version")
        check(not os.path.exists(installation.old), "restoration: the old directory remains")
        check(os.path.exists(installation.source), "restoration: the new directory is removed")
        check("cannot rename" in installation.log_text(), "restoration: the log")
        check(wait_for(marker), "restoration: the program did not run")
    finally:
        installation.remove()


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        print("RUN: " + test.__name__)
        test()
    print("%d tests, %d failures" % (len(tests), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
