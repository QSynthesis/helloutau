#include "EditorIcons_p.h"

#include <QtCore/QUrl>

#include <QAKCore/actionregistry.h>

#include <helloutau/Theme/ThemeIcon.h>

namespace hello::daw {

    void addEditorIcons(QAK::ActionRegistry *registry) {
        // The id of each command, which is the id of its icon, and the file of the icon
        static const std::pair<const char *, const char *> icons[] = {
            {"helloutau.edit.undo",           "intellij/undo.svg"  },
            {"helloutau.edit.redo",           "intellij/redo.svg"  },
            {"helloutau.edit.penTool",        "intellij/edit.svg"  },
            {"helloutau.edit.find",           "intellij/search.svg"},
            {"helloutau.playback.play",       "intellij/run.svg"   },
            {"helloutau.playback.pause",      "intellij/pause.svg" },
            {"helloutau.playback.stop",       "intellij/stop.svg"  },
            {"helloutau.playback.replay",     "intellij/rerun.svg" },
            {"helloutau.voiceBank.playAudio", "intellij/run.svg"   },
        };
        for (const auto &[id, file] : icons) {
            ThemeIcon icon;
            icon.files.setValue(ThemeButtonState::Up,
                                QStringLiteral(":/helloutau/icons/") + QLatin1String(file));
            // QActionKit passes a local file to QIcon, which selects the engine of ThemeIcon by
            // the suffix of the encoded name.
            registry->addIcon(QString(), QLatin1String(id),
                              QAK::ActionIcon(QUrl::fromLocalFile(icon.fileName())));
        }
    }

}
