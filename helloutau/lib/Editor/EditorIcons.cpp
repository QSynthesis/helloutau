#include "EditorIcons_p.h"

#include <QtCore/QUrl>

#include <QAKCore/actionregistry.h>

#include <helloutau/Theme/ThemeIcon.h>

namespace hello::daw {

    void addEditorIcons(QAK::ActionRegistry *registry) {
        // The id of each command, which is the id of its icon, and the file of the icon
        static const std::pair<const char *, const char *> icons[] = {
            {"helloutau.edit.undo",            "intellij/undo.svg"          },
            {"helloutau.edit.redo",            "intellij/redo.svg"          },
            {"helloutau.edit.selectTool",      "intellij/inSelection.svg"   },
            {"helloutau.edit.penTool",         "intellij/edit.svg"          },
            {"helloutau.edit.pitchTool",       "intellij/palette.svg"       },
            {"helloutau.edit.mode2",           "intellij/graphLayout.svg"   },
            {"helloutau.view.showPitch",       "intellij/softWrap.svg"      },
            {"helloutau.view.showEnvelopes",   "intellij/coverage.svg"      },
            {"helloutau.view.showParameters",  "intellij/parameter.svg"     },
            {"helloutau.edit.crossfadeP2P3",   "intellij/merge.svg"         },
            {"helloutau.edit.crossfadeP1P4",   "intellij/arrowLeftRight.svg"},
            {"helloutau.edit.find",            "intellij/search.svg"        },
            {"helloutau.playback.play",        "intellij/run.svg"           },
            {"helloutau.playback.pause",       "intellij/pause.svg"         },
            {"helloutau.playback.stop",        "intellij/stop.svg"          },
            {"helloutau.playback.replay",      "intellij/rerun.svg"         },
            {"helloutau.voiceBank.playAudio",  "intellij/run.svg"           },
            {"helloutau.voiceBank.playSpan",   "intellij/runToCursor.svg"   },
            {"helloutau.voiceBank.synthesize", "intellij/lightning.svg"     },
        };
        for (const auto &[id, file] : icons) {
            // The constructor assigns the file to every state. setValue() assigns one state, and
            // the icon engine has no file for the other states.
            ThemeIcon icon;
            icon.files =
                ThemeStates<QString>(QStringLiteral(":/helloutau/icons/") + QLatin1String(file));
            // QActionKit passes a local file to QIcon, which selects the engine of ThemeIcon by
            // the suffix of the encoded name.
            registry->addIcon(QString(), QLatin1String(id),
                              QAK::ActionIcon(QUrl::fromLocalFile(icon.fileName())));
        }
    }

}
