#ifndef HELLOKIT_EDIT_RECORDINGSELECTOR_P_H
#define HELLOKIT_EDIT_RECORDINGSELECTOR_P_H

#include <filesystem>

#include <QtCore/QList>

#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// Passes each question on, and records the directories for which the user chose an
    /// encoding or determined the misdecoded file names, whose configuration is to be written.
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

        QList<Answer> selectEncodings(const QList<Question> &questions,
                                      DiagnosticList &diagnostics) override {
            auto answers = m_selector->selectEncodings(questions, diagnostics);
            for (qsizetype i = 0; i < questions.size() && i < answers.size(); ++i) {
                const auto &answer = answers.at(i);
                if ((!questions.at(i).charset && answer.charset) || answer.fileNameEncoding) {
                    chosen.push_back(questions.at(i).directory->path);
                }
            }
            return answers;
        }

        QList<std::filesystem::path> chosen;

    private:
        VoiceBankCharsetSelector *m_selector;
    };

}

#endif // HELLOKIT_EDIT_RECORDINGSELECTOR_P_H
