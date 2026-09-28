#ifndef HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H
#define HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H

#include <filesystem>

#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/VoiceBankSource.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QListWidget;
class QTabWidget;

namespace hello::daw {

    /// Asks the user in which encoding to read a folder of a voice bank that nothing on disk
    /// determines, once per folder.
    ///
    /// The candidates are those of \c TextCodec::ustCandidates(), since a voice bank and a UST
    /// pose the same problem. Each text file of the folder is shown in the selected encoding on a
    /// tab of its own, whose title counts the byte sequences invalid in it. Declining leaves the
    /// folder out, and its samples are then found by file name only. See the section on choosing
    /// the encoding in docs/Editing.md.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankCharsetDialog : public QDialog,
                                                           public kit::VoiceBankCharsetSelector {
        Q_OBJECT
    public:
        explicit VoiceBankCharsetDialog(QWidget *parent = nullptr);
        ~VoiceBankCharsetDialog();

        /// Sets the voice bank whose folders are asked about, which the dialog names.
        void setRoot(const std::filesystem::path &root);

        /// Shows the dialog for \a directory and waits for the answer.
        std::optional<QString> selectCharset(const kit::VoiceBankDirectorySource &directory,
                                             kit::DiagnosticList &diagnostics) override;

        /// Prepares the dialog for \a directory without showing it.
        void setDirectory(const kit::VoiceBankDirectorySource &directory);

        QString selectedCharset() const;
        void setSelectedCharset(const QString &charset);

        /// The titles of the tabs, one per text file.
        QStringList previewTitles() const;

        /// The text shown on tab \a index.
        QString previewText(int index) const;

    private:
        std::filesystem::path m_root;
        const kit::VoiceBankDirectorySource *m_directory = nullptr;
        QLabel *m_message;
        QListWidget *m_charsets;
        QTabWidget *m_previews;

        void updatePreview();
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKCHARSETDIALOG_H
