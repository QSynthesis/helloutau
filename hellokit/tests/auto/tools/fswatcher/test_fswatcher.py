"""Protocol test of hello-fswatcher under typical voice bank editing operations.

    python3 test_fswatcher.py <path to hello-fswatcher>

Tests the program in isolation over its pipes, as hello::kit::FileSystemWatcher communicates with
it. At this layer the messages still distinguish the kind of change, a directory or an entire
tree, a removed root or an unwatchable one, and no message is sent when nothing changed. The
layers above merge these distinctions. See Protocol.h for the messages.

Two voice banks are monitored side by side, and every change occurs in the first, so that any
message about the second, or about neither, is a failure.
"""
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time

# The time limit for an expected message. Messages normally arrive within a fraction of a second,
# and the remainder accommodates a heavily loaded machine.
PATIENCE = 10.0

# The listening period for unexpected messages. An incorrect message arrives as soon as a correct
# one would, so this period suffices to detect one.
QUIET = 1.0


def esc(path):
    return path.replace("%", "%25").replace("\n", "%0A").replace("\r", "%0D")


def write(path, data=b"RIFF"):
    with open(path, "wb") as file:
        file.write(data)


class Program:
    def __init__(self, exe):
        self.process = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.lines = queue.Queue()
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self):
        for line in self.process.stdout:
            self.lines.put(line.decode("utf-8").rstrip("\n"))

    def send(self, text):
        self.process.stdin.write(text.encode("utf-8"))
        self.process.stdin.flush()

    def next(self, timeout):
        try:
            return self.lines.get(timeout=timeout)
        except queue.Empty:
            return None

    def gather(self, wanted=(), quiet=QUIET):
        """Collects output until every line in wanted has arrived, then for quiet more seconds."""
        got = []
        deadline = time.monotonic() + PATIENCE
        while not all(w in got for w in wanted) and time.monotonic() < deadline:
            line = self.next(0.1)
            if line is not None:
                got.append(line)
        settle = time.monotonic() + quiet
        while time.monotonic() < settle:
            line = self.next(0.05)
            if line is not None:
                got.append(line)
        return got

    def close(self):
        try:
            self.process.stdin.close()
            self.process.wait(timeout=5)
        except Exception:
            self.process.kill()
        return self.process.returncode


def check_arguments(exe):
    """--help prints the protocol and exits. Any other argument is rejected on standard error,
    leaving standard output, which the client parses as the protocol, empty."""
    failures = 0
    shown = subprocess.run([exe, "--help"], stdin=subprocess.DEVNULL, capture_output=True,
                           timeout=PATIENCE)
    text = shown.stdout.decode("utf-8")
    words = ("Usage: hello-fswatcher", "hello-fswatcher 1", "roots", "exit", "ok", "dirty",
             "recdirty", "gone", "unwatchable", "unknown", "%25", "%0A", "%0D")
    if shown.returncode != 0 or not all(word in text for word in words):
        print("FAIL --help:", shown.returncode, [w for w in words if w not in text])
        failures += 1

    refused = subprocess.run([exe, "--bogus"], stdin=subprocess.DEVNULL, capture_output=True,
                             timeout=PATIENCE)
    if refused.returncode != 2 or refused.stdout or b"--bogus" not in refused.stderr:
        print("FAIL unknown argument:", refused.returncode, refused.stdout, refused.stderr)
        failures += 1
    return failures


def main():
    exe = sys.argv[1]
    if check_arguments(exe):
        return 1
    base = os.path.realpath(tempfile.mkdtemp(prefix="fsw-"))
    program = None
    try:
        parent = os.path.join(base, "voice")
        # A percent sign, and one followed by text resembling an escape sequence, because the
        # protocol escapes percent signs and missing escaping would corrupt such names.
        root = os.path.join(parent, "bank %25 100%")
        other = os.path.join(base, "other bank")
        for directory in ("a/b", "gone-soon", "renamed-soon", "leaving"):
            os.makedirs(os.path.join(root, directory))
        os.makedirs(other)

        R = esc(root)
        S = os.sep

        def about_first_bank(line):
            path = line.split(" ", 1)[1] if " " in line else ""
            return path == R or path.startswith(R + S)

        program = Program(exe)
        greeting = program.next(PATIENCE)
        if greeting != "hello-fswatcher 1":
            print("FAIL greeting:", greeting)
            return 1

        program.send("roots\n" + esc(root) + "\n" + esc(other) + "\n#\n")
        answer = program.gather(["ok"])
        if not answer or answer[0] != "ok":
            print("FAIL answer:", answer)
            return 1
        # FSEvents also reports events from shortly before the stream started, which is a
        # superfluous hint rather than a failure. The checks consider only later messages.
        program.gather(quiet=1.5)

        failures = 0

        def expect(name, action, must=(), quiet=False):
            """With quiet, no message may arrive. Otherwise every message must concern the first
            voice bank, and all lines listed in must are required."""
            nonlocal failures
            try:
                action()
                acted = None
            except Exception as error:
                acted = repr(error)
            # If a message is expected, an incorrect message arrives with it, and a short period
            # afterward suffices to detect it. If no message may arrive, the full QUIET period is
            # observed.
            got = program.gather(must, quiet=QUIET if quiet else 0.3)
            missing = [m for m in must if m not in got]
            unwanted = got if quiet else [g for g in got if not about_first_bank(g)]
            ok = acted is None and not missing and not unwanted
            failures += 0 if ok else 1
            print(("PASS " if ok else "FAIL ") + name)
            if not ok:
                if acted:
                    print("   action failed:", acted)
                for m in missing:
                    print("   missing:", m)
                for g in got:
                    print("   got:", g)

        expect("a new file deep down",
               lambda: write(os.path.join(root, "a", "b", "x.wav")),
               must=["dirty " + R + S + "a" + S + "b"])
        expect("an oto.ini edited in place",
               lambda: write(os.path.join(root, "oto.ini"), b"a.wav=a\r\n"),
               must=["dirty " + R])
        expect("a subdirectory removed",
               lambda: shutil.rmtree(os.path.join(root, "gone-soon")),
               must=["dirty " + R])
        expect("a subdirectory renamed",
               lambda: os.rename(os.path.join(root, "renamed-soon"), os.path.join(root, "renamed")),
               must=["dirty " + R, "recdirty " + R + S + "renamed"])
        expect("a file in the renamed directory",
               lambda: write(os.path.join(root, "renamed", "y.wav")),
               must=["dirty " + R + S + "renamed"])

        outside = os.path.join(base, "moved-in")
        os.makedirs(os.path.join(outside, "deep"))
        expect("a tree moved in",
               lambda: os.rename(outside, os.path.join(root, "moved-in")),
               must=["recdirty " + R + S + "moved-in"])
        expect("a file deep in the tree moved in",
               lambda: write(os.path.join(root, "moved-in", "deep", "z.wav")),
               must=["dirty " + R + S + "moved-in" + S + "deep"])

        def made_and_filled():
            fresh = os.path.join(root, "fresh")
            os.makedirs(os.path.join(fresh, "inner"))
            write(os.path.join(fresh, "inner", "w.wav"))

        expect("a directory made and filled at once", made_and_filled,
               must=["recdirty " + R + S + "fresh"])

        expect("a sibling outside both banks",
               lambda: write(os.path.join(parent, "other.txt"), b"x"),
               quiet=True)
        expect("a directory moved out of the bank",
               lambda: os.rename(os.path.join(root, "leaving"), os.path.join(base, "left")),
               must=["dirty " + R])
        expect("a file in the directory moved out",
               lambda: write(os.path.join(base, "left", "late.wav")),
               quiet=True)

        expect("the root renamed",
               lambda: os.rename(root, root + " old"),
               must=["gone " + R])
        expect("the root renamed back",
               lambda: os.rename(root + " old", root),
               must=["recdirty " + R])
        expect("monitoring resumed after the rename back",
               lambda: write(os.path.join(root, "a", "again.wav")),
               must=["dirty " + R + S + "a"])
        expect("the directory holding the root renamed",
               lambda: os.rename(parent, parent + "2"),
               must=["gone " + R])

        code = program.close()
        program = None
        if code != 0:
            print("FAIL exit code", code, "at the end of input")
            failures += 1

        print("FAILURES", failures)
        return 1 if failures else 0
    finally:
        if program is not None:
            program.close()
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
