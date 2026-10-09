#include "EditorIcons_p.h"

#include <QtCore/QUrl>
#include <QtGui/QColor>

#include <QAKCore/actionregistry.h>

#include <helloutau/Theme/ThemeIcon.h>

namespace hello::daw {

    void addEditorIcons(QAK::ActionRegistry *registry) {
        // The id of each command, which is the id of its icon, and the file of the icon
        static const std::pair<const char *, const char *> icons[] = {
            {"helloutau.edit.undo",                "intellij/undo.svg"              },
            {"helloutau.edit.redo",                "intellij/redo.svg"              },
            {"helloutau.select.selectTool",        "helloutau/selectTool.svg"       },
            {"helloutau.select.penTool",           "intellij/edit.svg"              },
            {"helloutau.select.pitchTool",         "helloutau/pitchTool.svg"        },
            {"helloutau.edit.mode2",               "helloutau/mode2.svg"            },
            {"helloutau.edit.convertPitchToMode1", "intellij/freeze.svg"            },
            {"helloutau.view.showPitch",           "helloutau/showPitch.svg"        },
            {"helloutau.view.showRenderedPitch",   "helloutau/showRenderedPitch.svg"},
            {"helloutau.view.showEnvelopes",       "helloutau/showEnvelopes.svg"    },
            {"helloutau.view.showParameters",      "helloutau/showParameters.svg"   },
            {"helloutau.edit.crossfadeP2P3",       "helloutau/crossfadeP2P3.svg"    },
            {"helloutau.edit.crossfadeP1P4",       "helloutau/crossfadeP1P4.svg"    },
            {"helloutau.edit.resetEnvelopes",      "helloutau/resetEnvelope.svg"     },
            {"helloutau.edit.find",                "intellij/search.svg"            },
            {"helloutau.playback.play",            "intellij/run.svg"               },
            {"helloutau.playback.stop",            "intellij/stop.svg"              },
            {"helloutau.playback.replay",          "intellij/rerun.svg"             },
            {"helloutau.voiceBank.playAudio",      "intellij/run.svg"               },
            {"helloutau.voiceBank.playSpan",       "intellij/runToCursor.svg"       },
            {"helloutau.voiceBank.synthesize",     "intellij/lightning.svg"         },
        };
        for (const auto &[id, file] : icons) {
            // The constructor assigns the file to every state. setValue() assigns one state, and
            // the icon engine has no file for the other states.
            ThemeIcon icon;
            icon.files =
                ThemeStates<QString>(QStringLiteral(":/helloutau/icons/") + QLatin1String(file));
            // TODO: Move per-icon colors to the shared style sheet or theme configuration.
            if (QLatin1String(id) == QLatin1String("helloutau.edit.convertPitchToMode1")) {
                const auto color = QColor(QStringLiteral("#3574F0"));
                icon.colors.setValue(ThemeButtonState::Up, color);
                icon.colors.setValue(ThemeButtonState::Over, color);
            }
            // QActionKit passes a local file to QIcon, which selects the engine of ThemeIcon by
            // the suffix of the encoded name.
            registry->addIcon(QString(), QLatin1String(id),
                              QAK::ActionIcon(QUrl::fromLocalFile(icon.fileName())));
        }
    }

}
