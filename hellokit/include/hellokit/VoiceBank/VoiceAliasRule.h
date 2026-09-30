#ifndef HELLOKIT_VOICEBANK_VOICEALIASRULE_H
#define HELLOKIT_VOICEBANK_VOICEALIASRULE_H

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// A rule that derives a new alias from the alias of an oto entry, for the batch renaming
    /// and the rule-based duplication of the voice bank window. See step 7 in
    /// docs/VoiceBankEditor.md.
    ///
    /// An empty alias denotes the stem of the file name, as in UTAU. The rule applies to that
    /// stem, and the aliases are compared as the stems they denote.
    struct HELLOKIT_VOICEBANK_EXPORT VoiceAliasRule {
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceAliasRule)
    public:
        enum Kind {
            AddPrefix,    ///< prepends \c text
            AddSuffix,    ///< appends \c text
            Replace,      ///< replaces each occurrence of \c text with \c replacement
            RemovePrefix, ///< removes \c text at the start, if present
            RemoveSuffix, ///< removes \c text at the end, if present
        };

        Kind kind = AddSuffix;
        QString text;

        /// The replacement text of \c Replace. Ignored by the other kinds.
        QString replacement;

        /// Returns \a alias with the rule applied. The text is matched case-sensitively. An
        /// empty \c text leaves \a alias unchanged.
        QString apply(const QString &alias) const;

        /// Returns the name that \a alias denotes for the audio file \a fileName: \a alias, or
        /// the stem of \a fileName if \a alias is empty.
        static QString nameOf(const QString &fileName, const QString &alias);

        /// An oto entry of one directory, as far as the rule concerns it.
        struct Entry {
            QString fileName;
            QString alias;
        };

        /// The planned alias of one selected entry.
        struct Change {
            /// The index of the entry in the list passed to plan()
            int index = 0;
            /// The name before the rule, as nameOf() returns it
            QString from;
            /// The new alias
            QString to;
            /// The reason why the change cannot be made, or empty if it can.
            QString problem;
        };

        /// Applies the rule to the entries at \a selected of \a entries, which are all the
        /// entries of one directory, and checks the result. If \a copy is true, the new aliases
        /// are those of copies added beside the entries, else they replace the aliases of the
        /// entries.
        ///
        /// A change has a problem if its new alias is empty, or if its name equals the name of
        /// another entry of the same audio file after the operation, including another change.
        /// A renaming that leaves the name unchanged has no problem and is skipped by the
        /// caller. A copy with an unchanged name has the problem of the equal name.
        QList<Change> plan(const QList<Entry> &entries, const QList<int> &selected,
                           bool copy) const;
    };

}

#endif // HELLOKIT_VOICEBANK_VOICEALIASRULE_H
