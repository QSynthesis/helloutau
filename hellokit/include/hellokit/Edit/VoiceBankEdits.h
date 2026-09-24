#ifndef HELLOKIT_EDIT_VOICEBANKEDITS_H
#define HELLOKIT_EDIT_VOICEBANKEDITS_H

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Support/Diagnostic.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/VoiceBankRefs.h>

namespace hello::kit {

    /// The domain functions of a voice bank, with the conventions of ProjectEdits: each function
    /// performs its modifications in one transaction, joins a transaction in progress, and
    /// returns whether its transaction is committed.
    ///
    /// Each function is Q_INVOKABLE, which lists it in the meta-object of the class, and has a
    /// command.
    ///
    /// \sa VoiceBankCommands::domainFunctions()
    class HELLOKIT_EDIT_EXPORT VoiceBankEdits {
        Q_GADGET
        Q_DECLARE_TR_FUNCTIONS(hello::kit::VoiceBankEdits)
    public:
        /// Replaces every public field of \a entry with those of \a value. The spellings of the
        /// numbers are kept, and a number that no longer reads as its spelling is written anew.
        Q_INVOKABLE static bool setEntry(const OtoEntryRef &entry, const VoiceOtoEntry &value,
                                         DiagnosticList &diagnostics);

        /// Inserts \a entries into \a directory where saving places them: by the bytes of the
        /// file name in the encoding of the \c oto.ini , each after the entries of its audio file.
        /// The order of the tree therefore remains the order of the file.
        Q_INVOKABLE static bool insertEntries(const VoiceDirectoryRef &directory,
                                              const QList<VoiceOtoEntry> &entries,
                                              DiagnosticList &diagnostics);

        /// Inserts an entry for each of \a fileNames, audio files of \a directory without an
        /// entry, with an empty alias, which counts as the stem of the file name, and zero numbers.
        Q_INVOKABLE static bool includeAudio(const VoiceDirectoryRef &directory,
                                             const QStringList &fileNames,
                                             DiagnosticList &diagnostics);

        /// Removes the entries at \a indices of \a directory.
        Q_INVOKABLE static bool removeEntries(const VoiceDirectoryRef &directory,
                                              const QList<int> &indices,
                                              DiagnosticList &diagnostics);

        /// Writes the item of \c prefix.map at \a noteNum , creating \c prefix.map if the voice
        /// bank has none.
        Q_INVOKABLE static bool setPrefix(const VoiceBankRef &bank, int noteNum,
                                          const VoicePrefix &prefix, DiagnosticList &diagnostics);

        /// Removes the item of \c prefix.map at \a noteNum . The file remains.
        ///
        /// \sa VoiceBankDiskState::save()
        Q_INVOKABLE static bool removePrefix(const VoiceBankRef &bank, int noteNum,
                                             DiagnosticList &diagnostics);

        /// Makes \a charset the encoding in which the files of \a directory are saved. The text
        /// is unchanged and the bytes change. See the section on encodings in docs/Editing.md.
        ///
        /// The encoding that the \c oto.ini declares for itself is removed as well, because the
        /// file is then written in \a charset , and declared again if that is UTF-8.
        ///
        /// \sa VoiceBankDirectory::otoCharset
        Q_INVOKABLE static bool convertCharset(const VoiceDirectoryRef &directory,
                                               const QString &charset, DiagnosticList &diagnostics);
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKEDITS_H
