#include "NativeMessageBox.h"

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cerrno>
#include <cstdlib>

extern char **environ;

// Linux has no message box of the system. The box is shown by a dialog program of the desktop,
// which the library starts and waits for, so that the library links no toolkit.

namespace hello::kit {

    namespace {

        // Exit statuses that indicate that the dialog program did not show the box: 127 if the
        // program cannot be executed, as a shell reports it, and 255 if it rejects its arguments
        // or cannot open the display
        constexpr int notExecuted = 127;
        constexpr int failed = 255;

        // Runs the program found in PATH with the null-terminated arguments, which begin with
        // the program name, and returns whether it showed the box. The box was shown if the
        // program exited with another status, also if the user closed the box without OK.
        bool run(char *const arguments[]) {
            pid_t pid = 0;
            if (posix_spawnp(&pid, arguments[0], nullptr, nullptr, arguments, environ) != 0) {
                return false;
            }
            int status = 0;
            while (waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR) {
                    return false;
                }
            }
            return WIFEXITED(status) && WEXITSTATUS(status) != notExecuted &&
                   WEXITSTATUS(status) != failed;
        }

        const char *zenityOptionOf(NativeMessageBox::Icon icon) {
            switch (icon) {
                case NativeMessageBox::Warning:
                    return "--warning";
                case NativeMessageBox::Critical:
                    return "--error";
                default:
                    break;
            }
            return "--info";
        }

        const char *kdialogOptionOf(NativeMessageBox::Icon icon) {
            switch (icon) {
                case NativeMessageBox::Warning:
                    return "--sorry";
                case NativeMessageBox::Critical:
                    return "--error";
                default:
                    break;
            }
            return "--msgbox";
        }

    }

    bool NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon) {
        if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
            return false;
        }
        // zenity interprets the text as Pango markup unless --no-markup is given, and a file
        // name with an ampersand would then not be shown.
        const char *zenity[] = {
            "zenity", zenityOptionOf(icon), "--no-markup", "--title", title.c_str(),
            "--text", text.c_str(),         nullptr,
        };
        if (run(const_cast<char *const *>(zenity))) {
            return true;
        }
        const char *kdialog[] = {
            "kdialog", "--title", title.c_str(), kdialogOptionOf(icon), text.c_str(), nullptr,
        };
        return run(const_cast<char *const *>(kdialog));
    }

}
