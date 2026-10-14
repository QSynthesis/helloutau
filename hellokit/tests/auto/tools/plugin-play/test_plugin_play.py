"""Test of hello-plugin-play, the preview program for classic plugins (docs/PluginPlay.md).

    python3 test_plugin_play.py <path to hello-plugin-play> <path to plugin-play-original-stub>

Each test creates a job object as HelloUtau does for a plugin run, starts a stand-in for the
process of the plugin in it, and creates the pipe of the host named after that process. The
program then starts in the same job, with a command line in the form that PitTrace uses, and the
test checks the request in the pipe and the exit status. The folders contain non-ASCII characters,
so that a conversion of the paths to a narrow code page fails the test.
"""
import _winapi
import ctypes
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from ctypes import wintypes

PROGRAM = os.path.abspath(sys.argv[1])
STUB = os.path.abspath(sys.argv[2])

# The longest time a run may take
RUN_LIMIT = 30

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

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print("FAIL: " + message)


def same_path(first, second):
    return first is not None and second is not None and \
        os.path.normcase(first) == os.path.normcase(second)


def start_in_job(command, job, **options):
    """Starts command suspended, puts it into job and resumes it, as HelloUtau starts a plugin."""
    process = subprocess.Popen(command, creationflags=CREATE_SUSPENDED, **options)
    if not kernel32.AssignProcessToJobObject(job, int(process._handle)):
        raise OSError(ctypes.get_last_error(), "AssignProcessToJobObject failed")
    ntdll.NtResumeProcess(int(process._handle))
    return process


class Host:
    """A job with a stand-in for the process of the plugin, and the pipe of a host named after it
    unless pipe is false. reply is the line that the host writes after the request and after the
    delay, or None if the host closes the pipe instead."""

    def __init__(self, pipe=True, reply=b'{"result": "done"}\n', delay=0.0):
        self.job = kernel32.CreateJobObjectW(None, None)
        self.plugin = start_in_job([PYTHON, "-c", "import time; time.sleep(60)"], self.job)
        self.reply = reply
        self.delay = delay
        self.request = None
        self.program = None
        self.running_before_reply = None
        self.pipe = None
        self.thread = None
        if pipe:
            name = r"\\.\pipe\helloutau-plugin-play-%d" % self.plugin.pid
            self.pipe = _winapi.CreateNamedPipe(
                name, _winapi.PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | _winapi.PIPE_WAIT,
                1, 65536, 65536, 0, _winapi.NULL)
            self.thread = threading.Thread(target=self.serve, daemon=True)
            self.thread.start()

    def serve(self):
        try:
            _winapi.ConnectNamedPipe(self.pipe, False)
        except OSError as error:
            # The client may connect before the call.
            if error.winerror != 535:
                return
        data = b""
        while not data.endswith(b"\n"):
            try:
                chunk, _ = _winapi.ReadFile(self.pipe, 4096)
            except OSError:
                return
            if not chunk:
                return
            data += chunk
        self.request = json.loads(data.decode("utf-8"))
        time.sleep(self.delay)
        self.running_before_reply = self.program is not None and self.program.poll() is None
        if self.reply is not None:
            try:
                _winapi.WriteFile(self.pipe, self.reply)
                # The program reads the reply before the pipe closes.
                time.sleep(0.5)
            except OSError:
                # The program has already closed the pipe, which running_before_reply reports.
                pass
        _winapi.CloseHandle(self.pipe)
        self.pipe = None

    def run(self, command, cwd, env=None):
        """Runs the program in the job and returns its exit status."""
        self.program = start_in_job(command, self.job, cwd=cwd, env=env)
        try:
            status = self.program.wait(timeout=RUN_LIMIT)
        except subprocess.TimeoutExpired:
            self.program.kill()
            status = None
        if self.thread:
            self.thread.join(timeout=RUN_LIMIT)
        return status

    def close(self):
        if self.pipe is not None:
            _winapi.CloseHandle(self.pipe)
        kernel32.TerminateJobObject(self.job, 1)
        kernel32.CloseHandle(self.job)
        self.plugin.wait()


def folder():
    path = os.path.join(tempfile.mkdtemp(), "音源 plugin")
    os.makedirs(path)
    return path


def test_the_request_reaches_the_host():
    cwd = folder()
    host = Host()
    try:
        status = host.run('"%s"  "Base.ust"  /m:"testplay.txt"' % PROGRAM, cwd)
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
        status = host.run('"%s" Base.ust /o:"out.wav" /noplay /nodeletewav /nodeletecache /debug '
                          '/bogus' % PROGRAM, cwd)
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
        status = host.run('"%s" Base.ust /NOPLAY /m: /M:x.txt' % PROGRAM, cwd)
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
        status = host.run('"%s" Base.ust' % PROGRAM, cwd)
        check(host.running_before_reply is True, "reply: the program ended before the reply")
        check(status == 0, "reply: exit status %r" % status)
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_a_closed_pipe_ends_the_program():
    cwd = folder()
    host = Host(reply=None)
    try:
        status = host.run('"%s" Base.ust' % PROGRAM, cwd)
        check(host.request is not None, "closed pipe: no request")
        check(status == 0, "closed pipe: exit status %r" % status)
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_the_original_runs_without_a_host():
    cwd = folder()
    program = os.path.join(cwd, "playinplugin.exe")
    original = os.path.join(cwd, "playinplugin-original.exe")
    shutil.copyfile(PROGRAM, program)
    shutil.copyfile(STUB, original)
    log = os.path.join(cwd, "stub.log")
    env = dict(os.environ, PLUGIN_PLAY_STUB_LOG=log)
    host = Host(pipe=False)
    try:
        arguments = '  "Base.ust"  /m:"testplay.txt"'
        status = host.run('"%s"%s' % (program, arguments), cwd, env=env)
        check(status == 7, "original: exit status %r" % status)
        lines = open(log, encoding="utf-8").read().splitlines() if os.path.exists(log) else []
        check(len(lines) == 2, "original: the stand-in did not run")
        if len(lines) == 2:
            check(lines[0] == '"%s"%s' % (original, arguments),
                  "original: command line %r" % lines[0])
            check(same_path(lines[1], cwd), "original: working directory %r" % lines[1])
    finally:
        host.close()
        shutil.rmtree(os.path.dirname(cwd), ignore_errors=True)


def test_no_arguments_end_at_once():
    status = subprocess.run([PROGRAM], timeout=RUN_LIMIT).returncode
    check(status == 0, "no arguments: exit status %r" % status)


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        print("RUN: " + test.__name__)
        test()
    print("%d tests, %d failures" % (len(tests), len(failures)))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
