"""Protocol test of hello-fswatcher under typical voice bank editing operations.

    python3 test_fswatcher.py <path to hello-fswatcher>

Tests the program in isolation over its pipes, as hello::kit::FileSystemWatcher communicates with
it. At this layer the messages still distinguish the kind of change, a directory or an entire
tree, a removed root or an unwatchable one, and no message is sent when nothing changed. The
layers above merge these distinctions. See Protocol.h for the messages.

Two voice banks are monitored side by side, and every change occurs in the first, so that any
message about the second, or about neither, is a failure. Without --file-events no entry message
(create, delete, change) may appear. With it, each kind of entry change must be reported on
Windows and Linux, and the option must be refused on macOS.
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

# The messages sent only with --file-events.
ENTRY_WORDS = ("create", "delete", "change")


def esc(path):
    return path.replace("%", "%25").replace("\n", "%0A").replace("\r", "%0D")


def write(path, data=b"RIFF"):
    with open(path, "wb") as file:
        file.write(data)


class Program:
    def __init__(self, exe, arguments=()):
        self.process = subprocess.Popen([exe, *arguments], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE)
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
             "recdirty", "gone", "unwatchable", "unknown", "%25", "%0A", "%0D", "--file-events",
             "create", "delete", "change")
    if shown.returncode != 0 or not all(word in text for word in words):
        print("FAIL --help:", shown.returncode, [w for w in words if w not in text])
        failures += 1

    refused = subprocess.run([exe, "--bogus"], stdin=subprocess.DEVNULL, capture_output=True,
                             timeout=PATIENCE)
    if refused.returncode != 2 or refused.stdout or b"--bogus" not in refused.stderr:
        print("FAIL unknown argument:", refused.returncode, refused.stdout, refused.stderr)
        failures += 1

    # FSEvents reports directories only, so the option is refused rather than accepted without
    # effect. Standard input is closed, so a program that accepted it would exit with 0.
    if sys.platform == "darwin":
        entries = subprocess.run([exe, "--file-events"], stdin=subprocess.DEVNULL,
                                 capture_output=True, timeout=PATIENCE)
        if entries.returncode != 2 or entries.stdout or b"--file-events" not in entries.stderr:
            print("FAIL --file-events on macOS:", entries.returncode, entries.stdout,
                  entries.stderr)
            failures += 1
    return failures


def check_directories(exe):
    """The default mode, as used by hello::kit::FileSystemWatcher."""
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
            unwanted = got if quiet else [
                g for g in got if not about_first_bank(g) or g.split(" ", 1)[0] in ENTRY_WORDS
            ]
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


def check_file_events(exe):
    """With --file-events, each kind of entry change is reported by its entry message, in addition
    to the dirty message of the parent directory. Additional messages about the root are
    permitted, because a single operation may produce several system events."""
    base = os.path.realpath(tempfile.mkdtemp(prefix="fsw-entries-"))
    program = None
    failures = 0
    try:
        root = os.path.join(base, "bank")
        os.makedirs(root)
        R = esc(root)
        S = os.sep

        program = Program(exe, ["--file-events"])
        if program.next(PATIENCE) != "hello-fswatcher 1":
            print("FAIL greeting with --file-events")
            return 1
        program.send("roots\n" + esc(root) + "\n#\n")
        if program.gather(["ok"])[:1] != ["ok"]:
            print("FAIL answer with --file-events")
            return 1
        program.gather(quiet=1.5)

        def expect(name, action, must):
            nonlocal failures
            action()
            got = program.gather(must, quiet=0.3)
            missing = [m for m in must if m not in got]
            unwanted = [g for g in got
                        if not (" " in g and (g.split(" ", 1)[1] == R or
                                              g.split(" ", 1)[1].startswith(R + S)))]
            ok = not missing and not unwanted
            failures += 0 if ok else 1
            print(("PASS " if ok else "FAIL ") + "entries: " + name)
            if not ok:
                for m in missing:
                    print("   missing:", m)
                for g in got:
                    print("   got:", g)

        f, g = os.path.join(root, "f.wav"), os.path.join(root, "g.wav")
        expect("a file created", lambda: write(f),
               ["create " + R + S + "f.wav", "dirty " + R])
        expect("a file rewritten", lambda: write(f, b"RIFF...."),
               ["change " + R + S + "f.wav", "dirty " + R])
        expect("a file renamed", lambda: os.rename(f, g),
               ["delete " + R + S + "f.wav", "create " + R + S + "g.wav", "dirty " + R])
        expect("a file deleted", lambda: os.remove(g),
               ["delete " + R + S + "g.wav", "dirty " + R])
        expect("a directory created", lambda: os.makedirs(os.path.join(root, "d")),
               ["create " + R + S + "d", "recdirty " + R + S + "d", "dirty " + R])
        expect("a file in the new directory", lambda: write(os.path.join(root, "d", "h.wav")),
               ["create " + R + S + "d" + S + "h.wav", "dirty " + R + S + "d"])

        if program.close() != 0:
            print("FAIL exit code with --file-events")
            failures += 1
        program = None
        return failures
    finally:
        if program is not None:
            program.close()
        shutil.rmtree(base, ignore_errors=True)


def main():
    exe = sys.argv[1]
    if check_arguments(exe):
        return 1
    failures = check_directories(exe)
    if sys.platform != "darwin":
        failures += check_file_events(exe)
    print("TOTAL FAILURES", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
