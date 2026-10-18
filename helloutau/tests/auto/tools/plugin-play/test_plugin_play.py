"""Test of hello-plugin-play, the preview program for classic plugins (docs/PluginPlay.md).

    python3 test_plugin_play.py <path to hello-plugin-play> <path to plugin-play-original-stub>
        <version>

Each test starts a stand-in for the process of the plugin as HelloUtau starts a plugin, and
creates the endpoint of the host named after that process. On Windows the stand-in runs in a job
object, and the endpoint is a named pipe. On other systems the stand-in leads a process group of
its own, and the endpoint is a local socket in the directory of TMPDIR. The program then starts
beside the stand-in, in the same job or process group, with a command line in the form that
PitTrace uses, and the test checks the request at the endpoint and the exit status. The folders
contain non-ASCII characters, so that a conversion of the paths to a narrow code page fails the
test.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time

WINDOWS = sys.platform == "win32"

if WINDOWS:
    import _winapi
    import ctypes
    from ctypes import wintypes
else:
    import signal
    import socket

PROGRAM = os.path.abspath(sys.argv[1])
STUB = os.path.abspath(sys.argv[2])
VERSION = sys.argv[3]

# The extension of the programs
SUFFIX = ".exe" if WINDOWS else ""

# The longest time a run may take
RUN_LIMIT = 30

# The file name of the endpoint of the host without the process ID of the plugin
ENDPOINT_PREFIX = "helloutau-plugin-play-"

if WINDOWS:
    # A Python without a console window, which stands in for the process of the plugin
    PYTHON = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")

    CREATE_SUSPENDED = 0x00000004

    # The byte modes of a pipe, which QLocalServer uses. _winapi does not define them.
    PIPE_TYPE_BYTE = 0x00000000
    PIPE_READMODE_BYTE = 0x00000000

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel32.TerminateJobObject.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    ntdll = ctypes.WinDLL("ntdll")
    ntdll.NtResumeProcess.argtypes = [wintypes.HANDLE]
else:
    PYTHON = sys.executable

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print("FAIL: " + message)


def same_path(first, second):
    # Resolved first, because the temporary directory of macOS is under /var, a link to
    # /private/var, and the program reports the resolved path of its working directory.
    return first is not None and second is not None and \
        os.path.normcase(os.path.realpath(first)) == os.path.normcase(os.path.realpath(second))


if WINDOWS:
    def start_in_job(command, job, **options):
        """Starts command suspended, puts it into job and resumes it, as HelloUtau starts a
        plugin."""
        process = subprocess.Popen(command, creationflags=CREATE_SUSPENDED, **options)
        if not kernel32.AssignProcessToJobObject(job, int(process._handle)):
            raise OSError(ctypes.get_last_error(), "AssignProcessToJobObject failed")
        ntdll.NtResumeProcess(int(process._handle))
        return process


class Host:
    """A stand-in for the process of the plugin, and the endpoint of a host named after it
    unless endpoint is false. reply is the line that the host writes after the request and after
    the delay, or None if the host closes the connection instead."""

    def __init__(self, endpoint=True, reply=b'{"result": "done"}\n', delay=0.0):
        self.reply = reply
        self.delay = delay
        self.request = None
        self.program = None
        self.running_before_reply = None
        self.thread = None
        sleeper = [PYTHON, "-c", "import time; time.sleep(60)"]
        if WINDOWS:
            self.job = kernel32.CreateJobObjectW(None, None)
            self.plugin = start_in_job(sleeper, self.job)
            self.pipe = None
            if endpoint:
                self.pipe = _winapi.CreateNamedPipe(
                    r"\\.\pipe" + "\\" + ENDPOINT_PREFIX + str(self.plugin.pid),
                    _winapi.PIPE_ACCESS_DUPLEX,
                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | _winapi.PIPE_WAIT,
                    1, 65536, 65536, 0, _winapi.NULL)
        else:
            # A short directory, because the path of a local socket is limited to about 100
            # bytes
            self.directory = tempfile.mkdtemp(prefix="pp", dir="/tmp")
            self.plugin = subprocess.Popen(sleeper, preexec_fn=lambda: os.setpgid(0, 0))
            self.server = None
            self.connection = None
            if endpoint:
                self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.server.bind(os.path.join(self.directory,
                                              ENDPOINT_PREFIX + str(self.plugin.pid)))
                self.server.listen(1)
        if endpoint:
            self.thread = threading.Thread(target=self.serve, daemon=True)
            self.thread.start()

    def accept(self):
        """Waits for the program, and returns whether it connected."""
        if WINDOWS:
            try:
                _winapi.ConnectNamedPipe(self.pipe, False)
            except OSError as error:
                # The client may connect before the call.
                if error.winerror != 535:
                    return False
            return True
        try:
            self.connection, _ = self.server.accept()
        except OSError:
            return False
        return True

    def read(self):
        """Returns the next bytes, or an empty bytes object at the end of the connection."""
        try:
            if WINDOWS:
                chunk, _ = _winapi.ReadFile(self.pipe, 4096)
                return chunk
            return self.connection.recv(4096)
        except OSError:
            return b""

    def write(self, data):
        if WINDOWS:
            _winapi.WriteFile(self.pipe, data)
        else:
            self.connection.sendall(data)

    def disconnect(self):
        if WINDOWS:
            if self.pipe is not None:
                _winapi.CloseHandle(self.pipe)
                self.pipe = None
            return
        if self.connection is not None:
            self.connection.close()
            self.connection = None
        if self.server is not None:
            self.server.close()
            self.server = None

    def serve(self):
        if not self.accept():
            return
        data = b""
        while not data.endswith(b"\n"):
            chunk = self.read()
            if not chunk:
                self.disconnect()
                return
            data += chunk
        self.request = json.loads(data.decode("utf-8"))
        time.sleep(self.delay)
        self.running_before_reply = self.program is not None and self.program.poll() is None
        if self.reply is not None:
            try:
                self.write(self.reply)
                # The program reads the reply before the connection closes.
                time.sleep(0.5)
            except OSError:
                # The program has already closed the connection, which running_before_reply
                # reports.
                pass
        self.disconnect()

    def run(self, cwd, written, arguments, program=PROGRAM, env=None):
        """Runs program beside the plugin and returns its exit status. On Windows the command
        line is written, in which {program} stands for program. Other systems pass arguments."""
        if WINDOWS:
            self.program = start_in_job(written.format(program=program), self.job, cwd=cwd,
                                        env=env)
        else:
            group = self.plugin.pid
            self.program = subprocess.Popen(
                [program] + arguments, cwd=cwd,
                env=dict(env or os.environ, TMPDIR=self.directory + "/"),
                preexec_fn=lambda: os.setpgid(0, group))
        try:
            status = self.program.wait(timeout=RUN_LIMIT)
        except subprocess.TimeoutExpired:
            self.program.kill()
            status = None
        if self.thread:
            self.thread.join(timeout=RUN_LIMIT)
        return status

    def close(self):
        self.disconnect()
        if WINDOWS:
            kernel32.TerminateJobObject(self.job, 1)
            kernel32.CloseHandle(self.job)
        else:
            os.killpg(self.plugin.pid, signal.SIGKILL)
        self.plugin.wait()
        if not WINDOWS:
            shutil.rmtree(self.directory, ignore_errors=True)


def folder():
    path = os.path.join(tempfile.mkdtemp(), "音源 plugin")
    os.makedirs(path)
    return path


def test_the_request_reaches_the_host():
    cwd = folder()
    host = Host()
    try:
        status = host.run(cwd, '"{program}"  "Base.ust"  /m:"testplay.txt"',
                          ["Base.ust", "/m:testplay.txt"])
        check(status == 0, "request: exit status %r" % status)
        request = host.request or {}
        check(same_path(request.get("input"), os.path.join(cwd, "Base.ust")),
              "request: input %r" % request.get("input"))
        check(same_path(request.get("modify"), os.path.join(cwd, "testplay.txt")),
              "request: modify %r" % request.get("modify"))
        check(request.get("output", "") is None, "request: output %r" % request.get("output"))
        check(request.get("play") is True, "request: play")
        check(request.get("keepOutput") is False, "request: keepOutput")
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_the_switches():
    cwd = folder()
    host = Host()
    try:
        switches = ["/noplay", "/nodeletewav", "/nodeletecache", "/debug", "/bogus"]
        status = host.run(cwd, '"{program}" Base.ust /o:"out.wav" ' + " ".join(switches),
                          ["Base.ust", "/o:out.wav"] + switches)
        check(status == 0, "switches: exit status %r" % status)
        request = host.request or {}
        check(same_path(request.get("output"), os.path.join(cwd, "out.wav")),
              "switches: output %r" % request.get("output"))
        check(request.get("play") is False, "switches: /noplay")
        check(request.get("keepOutput") is True, "switches: /nodeletewav")
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_the_switches_are_case_sensitive_and_empty_values_absent():
    cwd = folder()
    host = Host()
    try:
        status = host.run(cwd, '"{program}" Base.ust /NOPLAY /m: /M:x.txt',
                          ["Base.ust", "/NOPLAY", "/m:", "/M:x.txt"])
        check(status == 0, "case: exit status %r" % status)
        request = host.request or {}
        check(request.get("play") is True, "case: /NOPLAY took effect")
        check(request.get("modify", "") is None, "case: modify %r" % request.get("modify"))
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_the_program_runs_until_the_reply():
    cwd = folder()
    host = Host(delay=1.0)
    try:
        status = host.run(cwd, '"{program}" Base.ust', ["Base.ust"])
        check(host.running_before_reply is True, "reply: the program ended before the reply")
        check(status == 0, "reply: exit status %r" % status)
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_a_closed_connection_ends_the_program():
    cwd = folder()
    host = Host(reply=None)
    try:
        status = host.run(cwd, '"{program}" Base.ust', ["Base.ust"])
        check(host.request is not None, "closed connection: no request")
        check(status == 0, "closed connection: exit status %r" % status)
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_the_original_runs_without_a_host():
    cwd = folder()
    program = os.path.join(cwd, "playinplugin" + SUFFIX)
    original = os.path.join(cwd, "playinplugin-original" + SUFFIX)
    # The copy keeps the permissions, which include the execute permission on other systems.
    shutil.copy(PROGRAM, program)
    shutil.copy(STUB, original)
    log = os.path.join(cwd, "stub.log")
    env = dict(os.environ, PLUGIN_PLAY_STUB_LOG=log)
    host = Host(endpoint=False)
    try:
        written = '  "Base.ust"  /m:"testplay.txt"'
        arguments = ["Base.ust", "/m:testplay.txt"]
        status = host.run(cwd, '"{program}"' + written, arguments, program=program, env=env)
        check(status == 7, "original: exit status %r" % status)
        lines = open(log, encoding="utf-8").read().splitlines() if os.path.exists(log) else []
        check(len(lines) == 2, "original: the stand-in did not run")
        if len(lines) == 2:
            # The command line as written on Windows, and the arguments separated by tabs
            # elsewhere
            expected = '"%s"%s' % (original, written) if WINDOWS else \
                "\t".join([original] + arguments)
            check(lines[0] == expected, "original: command line %r" % lines[0])
            check(same_path(lines[1], cwd), "original: working directory %r" % lines[1])
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_no_arguments_end_at_once():
    status = subprocess.run([PROGRAM], timeout=RUN_LIMIT).returncode
    check(status == 0, "no arguments: exit status %r" % status)


def test_the_help_and_the_version_as_the_only_argument():
    # The standard output is a pipe, to which the program writes instead of a message box.
    usage = ("Usage: playinplugin%s <input>" % SUFFIX).encode("ascii")
    for argument in ["/?", "-h", "--help"]:
        result = subprocess.run([PROGRAM, argument], stdout=subprocess.PIPE, timeout=RUN_LIMIT)
        check(result.returncode == 0, "help %s: exit status %r" % (argument, result.returncode))
        check(result.stdout.startswith(usage),
              "help %s: output %r" % (argument, result.stdout[:40]))
    result = subprocess.run([PROGRAM, "--version"], stdout=subprocess.PIPE, timeout=RUN_LIMIT)
    check(result.returncode == 0, "version: exit status %r" % result.returncode)
    check(result.stdout == ("hello-plugin-play %s\n" % VERSION).encode("ascii"),
          "version: output %r" % result.stdout)


def test_the_help_with_other_arguments_is_the_input():
    # As in the original, the first argument of a longer command line is the input file.
    cwd = folder()
    host = Host()
    try:
        status = host.run(cwd, '"{program}" --help /noplay', ["--help", "/noplay"])
        check(status == 0, "help as input: exit status %r" % status)
        request = host.request or {}
        check(same_path(request.get("input"), os.path.join(cwd, "--help")),
              "help as input: input %r" % request.get("input"))
        check(request.get("play") is False, "help as input: /noplay")
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        print("RUN: " + test.__name__)
        test()
    print("%d tests, %d failures" % (len(tests), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
