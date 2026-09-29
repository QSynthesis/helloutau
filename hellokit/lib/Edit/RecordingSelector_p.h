#ifndef HELLOKIT_EDIT_RECORDINGSELECTOR_P_H
#define HELLOKIT_EDIT_RECORDINGSELECTOR_P_H

#include <filesystem>

#include <QtCore/QList>

#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// Passes each question on, and records the directories for which the user chose an
    /// encoding, to be remembered for recording by the next save.
    class RecordingSelector : public VoiceBankCharsetSelector {
    public:
        explicit RecordingSelector(VoiceBankCharsetSelector *selector) : m_selector(selector) {
        }

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &diagnostics) override {
            auto charset = m_selector->selectCharset(directory, diagnostics);
            if (charset) {
                chosen.push_back(directory.path);
            }
            return charset;
        }

        QList<std::filesystem::path> chosen;

    private:
        VoiceBankCharsetSelector *m_selector;
    };

}

#endif // HELLOKIT_EDIT_RECORDINGSELECTOR_P_H
