#include "NativeMessageBox.h"

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <optional>

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

        struct Result {
            int status;
            std::string output; ///< the standard output of the program
        };

        // Runs the program found in PATH with the arguments, which begin with the program name,
        // and returns its exit status and output, or null if it did not show the box. The box
        // was shown if the program exited with another status, also if the user closed it.
        std::optional<Result> run(const std::vector<std::string> &arguments) {
            std::vector<char *> argv;
            for (const auto &argument : arguments) {
                argv.push_back(const_cast<char *>(argument.c_str()));
            }
            argv.push_back(nullptr);

            int fds[2];
            if (::pipe(fds) != 0) {
                return std::nullopt;
            }
            posix_spawn_file_actions_t actions;
            posix_spawn_file_actions_init(&actions);
            posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
            posix_spawn_file_actions_addclose(&actions, fds[0]);
            posix_spawn_file_actions_addclose(&actions, fds[1]);
            pid_t pid = 0;
            const int spawned =
                posix_spawnp(&pid, argv.front(), &actions, nullptr, argv.data(), environ);
            posix_spawn_file_actions_destroy(&actions);
            ::close(fds[1]);
            if (spawned != 0) {
                ::close(fds[0]);
                return std::nullopt;
            }

            Result result{0, {}};
            char buffer[256];
            for (;;) {
                const auto count = ::read(fds[0], buffer, sizeof(buffer));
                if (count > 0) {
                    result.output.append(buffer, std::size_t(count));
                } else if (count == 0 || errno != EINTR) {
                    break;
                }
            }
            ::close(fds[0]);

            int status = 0;
            while (::waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR) {
                    return std::nullopt;
                }
            }
            if (!WIFEXITED(status) || WEXITSTATUS(status) == notExecuted ||
                WEXITSTATUS(status) == failed) {
                return std::nullopt;
            }
            result.status = WEXITSTATUS(status);
            return result;
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

        bool warns(NativeMessageBox::Icon icon) {
            return icon == NativeMessageBox::Warning || icon == NativeMessageBox::Critical;
        }

        // The buttons in the roles of the dialog programs: the accepting button, the escape
        // button, and the extra button if there are three
        struct Roles {
            NativeMessageBox::StandardButton accept;
            NativeMessageBox::StandardButton escape;
            std::optional<NativeMessageBox::StandardButton> extra;
        };

        Roles rolesOf(const NativeMessageBox::Layout &layout) {
            std::vector<NativeMessageBox::StandardButton> others;
            for (const auto button : layout.buttons) {
                if (button != layout.escapeButton) {
                    others.push_back(button);
                }
            }
            Roles roles{others.front(), layout.escapeButton, std::nullopt};
            if (others.size() > 1) {
                roles.extra = others[1];
            }
            return roles;
        }

    }

    NativeMessageBox::StandardButton
        NativeMessageBox::show(const std::string &title, const std::string &text, Icon icon,
                               int buttons, StandardButton defaultButton,
                               const std::map<StandardButton, std::string> &texts) {
        if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
            return NoButton;
        }
        const auto layout = layoutOf(buttons, defaultButton);
        const auto labelOf = [&texts](StandardButton button) {
            const auto found = texts.find(button);
            return found != texts.cend() ? found->second : nameOf(button);
        };

        if (layout.buttons.size() == 1) {
            const auto button = layout.buttons.front();
            // zenity interprets the text as Pango markup unless --no-markup is given, and a file
            // name with an ampersand would then not be shown.
            std::vector<std::string> zenity{
                "zenity", zenityOptionOf(icon), "--no-markup", "--title", title, "--text", text,
            };
            if (button != Ok || texts.find(Ok) != texts.cend()) {
                zenity.insert(zenity.end(), {"--ok-label", labelOf(button)});
            }
            if (run(zenity)) {
                return button;
            }
            if (run({"kdialog", "--title", title, kdialogOptionOf(icon), text})) {
                return button;
            }
            return NoButton;
        }

        const auto roles = rolesOf(layout);
        std::vector<std::string> zenity{
            "zenity",
            "--question",
            "--no-markup",
            "--title",
            title,
            "--text",
            text,
            "--ok-label",
            labelOf(roles.accept),
            "--cancel-label",
            labelOf(roles.escape),
        };
        if (roles.extra) {
            // zenity writes the label of the extra button, which exits with status 1 as the
            // cancel button does.
            zenity.insert(zenity.end(), {"--extra-button", labelOf(*roles.extra)});
        }
        if (layout.defaultButton == roles.escape) {
            zenity.push_back("--default-cancel");
        }
        if (const auto result = run(zenity)) {
            if (result->status == 0) {
                return roles.accept;
            }
            if (roles.extra && result->output.rfind(labelOf(*roles.extra), 0) == 0) {
                return *roles.extra;
            }
            return roles.escape;
        }

        // kdialog has Yes, No and Cancel, and exits with status 0, 1 and 2 for them.
        std::vector<std::string> kdialog{"kdialog", "--title", title};
        if (roles.extra) {
            kdialog.insert(kdialog.end(),
                           {warns(icon) ? "--warningyesnocancel" : "--yesnocancel", text,
                            "--yes-label", labelOf(roles.accept), "--no-label",
                            labelOf(*roles.extra), "--cancel-label", labelOf(roles.escape)});
        } else {
            kdialog.insert(kdialog.end(),
                           {warns(icon) ? "--warningyesno" : "--yesno", text, "--yes-label",
                            labelOf(roles.accept), "--no-label", labelOf(roles.escape)});
        }
        if (const auto result = run(kdialog)) {
            if (result->status == 0) {
                return roles.accept;
            }
            if (result->status == 1 && roles.extra) {
                return *roles.extra;
            }
            return roles.escape;
        }
        return NoButton;
    }

}
