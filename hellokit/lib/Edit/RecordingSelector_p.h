#ifndef HELLOKIT_EDIT_RECORDINGSELECTOR_P_H
#define HELLOKIT_EDIT_RECORDINGSELECTOR_P_H

#include <filesystem>

#include <QtCore/QList>

#include <hellokit/VoiceBank/VoiceBankSource.h>

namespace hello::kit {

    /// Forwards each question to the wrapped selector, and records the directories whose
    /// configuration is to be written.
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
