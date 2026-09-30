#ifndef HELLOUTAU_EDITOR_VOICEALIASRULEDIALOG_H
#define HELLOUTAU_EDITOR_VOICEALIASRULEDIALOG_H

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <hellokit/VoiceBank/VoiceAliasRule.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace hello::daw {

    /// Dialog for the batch renaming of aliases and for the rule-based duplication of entries in
    /// the voice bank window. See step 7 in docs/VoiceBankEditor.md.
    ///
    /// The dialog edits a kit::VoiceAliasRule and previews its result for the selected entries:
    /// the current name, the new alias and the problem of each change. OK is disabled while a
    /// change has a problem or no change remains.
    class HELLOUTAU_EDITOR_EXPORT VoiceAliasRuleDialog : public QDialog {
        Q_OBJECT
    public:
        enum Mode {
            Rename,    ///< replaces the aliases of the selected entries
            Duplicate, ///< adds a copy of each selected entry with the new alias
        };

        /// The entries of one directory and the indices of the selected entries among them
        struct Group {
            QList<kit::VoiceAliasRule::Entry> entries;
            QList<int> selected;
        };

        VoiceAliasRuleDialog(Mode mode, const QList<Group> &groups, QWidget *parent = nullptr);
        ~VoiceAliasRuleDialog() override;

        /// Returns the rule of the controls.
        kit::VoiceAliasRule rule() const;

        /// Returns the changes of the rule for each group, as kit::VoiceAliasRule::plan()
        /// returns them.
        QList<QList<kit::VoiceAliasRule::Change>> changes() const;

        QComboBox *kindBox() const;
        QLineEdit *textEdit() const;
        QLineEdit *replacementEdit() const;
        QTreeWidget *preview() const;
        QPushButton *okButton() const;

    private:
        Mode m_mode;
        QList<Group> m_groups;
        QComboBox *m_kind;
        QLineEdit *m_text;
        QLabel *m_replacementLabel;
        QLineEdit *m_replacement;
        QTreeWidget *m_preview;
        QPushButton *m_ok;

        void updatePreview();
    };

}

#endif // HELLOUTAU_EDITOR_VOICEALIASRULEDIALOG_H
