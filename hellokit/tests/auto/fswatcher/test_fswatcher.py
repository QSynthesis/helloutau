"""What hello-fswatcher says when a voice bank's author does what authors do.

    python3 test_fswatcher.py <path to hello-fswatcher>

The program on its own, spoken to over its pipes, the way hello::kit::FileSystemWatcher speaks to
it. This is the layer where the messages still say which kind of change it was, a directory or a
whole tree, a root gone or one that cannot be followed, and where nothing at all is said when
nothing is to be: the layers above fold those together. See Protocol.h for the messages.

Two banks are followed side by side, and everything happens in the first, so that anything said
about the second, or about neither, is a failure.
"""
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time

# How long to wait for what has to come. It comes within a fraction of a second, and the rest is
# for a machine that is busy.
PATIENCE = 10.0

# How long to listen for what must not come. A wrong message comes as soon as a right one would,
# so this is long enough to hear one and no longer.
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
        """What it says until everything in \\a wanted has come, and then for \\a quiet more."""
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


def main():
    exe = sys.argv[1]
    base = os.path.realpath(tempfile.mkdtemp(prefix="fsw-"))
    program = None
    try:
        parent = os.path.join(base, "voice")
        # A percent sign, and one followed by what reads as an escape, since that is what the
        # protocol escapes and what goes wrong when it is not.
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
        # FSEvents reports what happened just before it started as well: a hint too many, not a
        # failure. What the checks look at starts after it.
        program.gather(quiet=1.5)

        failures = 0

        def expect(name, action, must=(), quiet=False):
            """With quiet, nothing at all may be said. Otherwise everything said is about the
            first bank, and all of must is among it."""
            nonlocal failures
            try:
                action()
                acted = None
            except Exception as error:
                acted = repr(error)
            # Where something has to come, what comes wrong comes with it, and a moment after is
            # enough to hear it. Where nothing may come, the whole of QUIET is listened to.
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
        expect("followed again once back",
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
