#include "NativeProcess.h"

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

extern char **environ;

namespace hello::kit {

    namespace {

        // Starts the process with the arguments argv, the first of which is the name of the
        // program in them, and returns its process ID, or std::nullopt if it does not start
        std::optional<pid_t> startProcess(const std::filesystem::path &program,
                                          std::vector<std::string> argv,
                                          const std::filesystem::path &workingDirectory) {
            std::vector<char *> pointers;
            for (auto &value : argv) {
                pointers.push_back(value.data());
            }
            pointers.push_back(nullptr);
            // The working directory of the child, set by changing that of this process around the
            // call, because posix_spawn_file_actions_addchdir is not available on every system
            // that this project supports. The programs that start a process through the library
            // run one thread.
            std::error_code error;
            const auto previous = std::filesystem::current_path(error);
            if (!workingDirectory.empty()) {
                std::filesystem::current_path(workingDirectory, error);
                if (error) {
                    return std::nullopt;
                }
            }
            pid_t pid = 0;
            const bool started =
                posix_spawn(&pid, program.c_str(), nullptr, nullptr, pointers.data(), environ) == 0;
            if (!workingDirectory.empty()) {
                std::filesystem::current_path(previous, error);
            }
            if (!started) {
                return std::nullopt;
            }
            return pid;
        }

        std::vector<std::string> argvOf(const std::filesystem::path &program,
                                        const std::vector<NativeString> &arguments) {
            std::vector<std::string> argv{program.native()};
            argv.insert(argv.end(), arguments.begin(), arguments.end());
            return argv;
        }

        std::optional<int> waitFor(pid_t pid) {
            int status = 0;
            while (waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR) {
                    return std::nullopt;
                }
            }
            return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }

        // Returns the words of commandLine as a POSIX shell splits them, without expansion, or
        // std::nullopt for an unterminated quotation. See NativeProcess::runCommandLine().
        std::optional<std::vector<std::string>> wordsOf(const std::string &commandLine) {
            std::vector<std::string> words;
            std::string word;
            bool inWord = false;
            for (size_t i = 0; i < commandLine.size(); ++i) {
                const char c = commandLine[i];
                if (c == ' ' || c == '\t' || c == '\n') {
                    if (inWord) {
                        words.push_back(std::move(word));
                        word.clear();
                        inWord = false;
                    }
                    continue;
                }
                inWord = true;
                if (c == '\'') {
                    const auto end = commandLine.find('\'', i + 1);
                    if (end == std::string::npos) {
                        return std::nullopt;
                    }
                    word.append(commandLine, i + 1, end - i - 1);
                    i = end;
                } else if (c == '"') {
                    for (++i;; ++i) {
                        if (i >= commandLine.size()) {
                            return std::nullopt;
                        }
                        const char d = commandLine[i];
                        if (d == '"') {
                            break;
                        }
                        if (d == '\\' && i + 1 < commandLine.size()) {
                            const char e = commandLine[i + 1];
                            if (e == '$' || e == '`' || e == '"' || e == '\\' || e == '\n') {
                                // A backslash and a newline join the lines, as in a shell
                                if (e != '\n') {
                                    word += e;
                                }
                                ++i;
                                continue;
                            }
                        }
                        word += d;
                    }
                } else if (c == '\\') {
                    if (i + 1 < commandLine.size()) {
                        // A backslash and a newline join the lines, as in a shell
                        if (commandLine[i + 1] != '\n') {
                            word += commandLine[i + 1];
                        }
                        ++i;
                    }
                } else {
                    word += c;
                }
            }
            if (inWord) {
                words.push_back(std::move(word));
            }
            return words;
        }

    }

    bool NativeProcess::isElevated() {
        return geteuid() == 0;
    }

    bool NativeProcess::start(const std::filesystem::path &program,
                              const std::vector<NativeString> &arguments,
                              const std::filesystem::path &workingDirectory) {
        return startProcess(program, argvOf(program, arguments), workingDirectory).has_value();
    }

    std::optional<int> NativeProcess::run(const std::filesystem::path &program,
                                          const std::vector<NativeString> &arguments) {
        const auto pid = startProcess(program, argvOf(program, arguments), {});
        if (!pid) {
            return std::nullopt;
        }
        return waitFor(*pid);
    }

    std::optional<int> NativeProcess::runCommandLine(const std::filesystem::path &program,
                                                     NativeString commandLine) {
        auto words = wordsOf(commandLine);
        if (!words) {
            return std::nullopt;
        }
        // A command line without words still names the program in its arguments
        if (words->empty()) {
            words->push_back(program.native());
        }
        const auto pid = startProcess(program, std::move(*words), {});
        if (!pid) {
            return std::nullopt;
        }
        return waitFor(*pid);
    }

}
