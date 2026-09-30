#ifndef HELLOUTAU_EDITOR_VOICEBANKINFOPANEL_H
#define HELLOUTAU_EDITOR_VOICEBANKINFOPANEL_H

#include <filesystem>
#include <memory>

#include <QtWidgets/QWidget>

#include <hellokit/Support/Diagnostic.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace hello::kit {
    class VoiceBankSession;
}

namespace hello::daw {

    /// The information of a voice bank beside its entries, in the right pane of VoiceBankWindow:
    /// contents of \c character.txt, \c readme.txt and \c prefix.map of the root. See step 5
    /// of docs/VoiceBankEditor.md.
    ///
    /// A line is written once its editing finishes, a text once its box loses the focus or
    /// commit() is called, and a cell of the prefix map once it is edited, each as one undo
    /// step. The panel follows the session, except the box that has the focus, whose text is
    /// being typed.
    class HELLOUTAU_EDITOR_EXPORT VoiceBankInfoPanel : public QWidget {
        Q_OBJECT
    public:
        explicit VoiceBankInfoPanel(kit::VoiceBankSession *session, QWidget *parent = nullptr);
        ~VoiceBankInfoPanel();

        /// The folder of the voice bank, in which the image of the character is found.
        void setRoot(const std::filesystem::path &root);

        /// Writes the texts being typed, as before a save.
        void commit();

        /// \name The fields of \c character.txt
        /// @{
        QLineEdit *nameEdit() const;
        QLineEdit *imageEdit() const;
        QLineEdit *sampleEdit() const;
        QLineEdit *authorEdit() const;
        QLineEdit *webEdit() const;

        /// The lines that are no field, as the profile of the character
        QPlainTextEdit *otherLinesEdit() const;

        /// The image, 100 by 100 pixels as UTAU requires, if the file reads
        QLabel *imagePreview() const;
        /// @}

        QPlainTextEdit *readmeEdit() const;

        /// The keys of \c prefix.map from C1 to B7, each a row of the note, the prefix and the
        /// suffix. A key that the file lacks has empty cells; editing one adds the key, and
        /// creates the file if the voice bank has none. The context menu of a row removes its
        /// key.
        QTableWidget *prefixTable() const;

        /// Removes the key of \c prefix.map at \a noteNum, as the context menu does.
        bool removePrefix(int noteNum);

    Q_SIGNALS:
        /// An edit was refused, for the reasons in \a diagnostics.
        void editRejected(const kit::DiagnosticList &diagnostics);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_VOICEBANKINFOPANEL_H
