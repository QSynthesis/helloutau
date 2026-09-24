#include "VoiceBankEdits.h"

#include <algorithm>

#include <QtCore/QByteArray>
#include <QtCore/QSet>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace {

        bool fail(DiagnosticList &diagnostics, const QString &message) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message = message;
            diagnostics.push_back(diagnostic);
            return false;
        }

        // The file name as saving compares it: the bytes in the encoding of the oto.ini, which
        // is the declared encoding if available and the encoding of the directory otherwise. A
        // directory without an encoding has no text yet, and UTF-8 stands in for it.
        QByteArray orderOf(const QString &fileName, const VoiceDirectoryRef &directory) {
            for (const auto &name : {directory.otoCharset(), directory.charset()}) {
                if (!name.isEmpty()) {
                    const TextCodec codec(name);
                    if (codec.isValid()) {
                        return codec.encode(fileName);
                    }
                }
            }
            return fileName.toUtf8();
        }

    }

    bool VoiceBankEdits::setEntry(const OtoEntryRef &entry, const VoiceOtoEntry &value,
                                  DiagnosticList &diagnostics) {
        auto transaction = entry.session()->transaction(tr("Change Oto Entry"));
        entry.setFileName(value.fileName);
        entry.setAlias(value.alias);
        entry.setOffset(value.offset);
        entry.setConsonant(value.consonant);
        entry.setCutoff(value.cutoff);
        entry.setPreUtterance(value.preUtterance);
        entry.setVoiceOverlap(value.voiceOverlap);
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::insertEntries(const VoiceDirectoryRef &directory,
                                       const QList<VoiceOtoEntry> &entries,
                                       DiagnosticList &diagnostics) {
        if (entries.isEmpty()) {
            return true;
        }
        auto transaction = directory.session()->transaction(tr("Insert Oto Entries"));
        const auto list = directory.otoEntries();
        for (const auto &entry : entries) {
            // After every entry that does not sort after it, which places it after the entries
            // of its audio file.
            const auto key = orderOf(entry.fileName, directory);
            int index = list.size();
            while (index > 0 && orderOf(list.at(index - 1).fileName(), directory) > key) {
                --index;
            }
            list.insert(index, {entry});
        }
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::includeAudio(const VoiceDirectoryRef &directory,
                                      const QStringList &fileNames, DiagnosticList &diagnostics) {
        const auto audio = directory.session()->audioFiles(directory.path());
        QSet<QString> named;
        const auto list = directory.otoEntries();
        for (int i = 0; i < list.size(); ++i) {
            named.insert(list.at(i).fileName());
        }
        QList<VoiceOtoEntry> entries;
        for (const auto &fileName : fileNames) {
            if (!audio.contains(fileName)) {
                return fail(diagnostics,
                            tr("\"%1\" is not an audio file of this folder.").arg(fileName));
            }
            if (named.contains(fileName)) {
                return fail(diagnostics, tr("\"%1\" already has an oto entry.").arg(fileName));
            }
            named.insert(fileName);
            VoiceOtoEntry entry;
            entry.fileName = fileName;
            entries.push_back(entry);
        }
        auto transaction = directory.session()->transaction(tr("Include Audio Files"));
        if (!insertEntries(directory, entries, diagnostics)) {
            return false;
        }
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::removeEntries(const VoiceDirectoryRef &directory,
                                       const QList<int> &indices, DiagnosticList &diagnostics) {
        const auto list = directory.otoEntries();
        auto sorted = indices;
        std::sort(sorted.begin(), sorted.end());
        for (qsizetype i = 0; i < sorted.size(); ++i) {
            if (sorted[i] < 0 || sorted[i] >= list.size() ||
                (i > 0 && sorted[i] == sorted[i - 1])) {
                return fail(diagnostics, tr("The folder has %1 oto entries, not an entry %2.")
                                             .arg(list.size())
                                             .arg(sorted[i]));
            }
        }
        auto transaction = directory.session()->transaction(tr("Remove Oto Entries"));
        // From the last, so that the remaining indices stay valid.
        for (auto it = sorted.crbegin(); it != sorted.crend(); ++it) {
            list.remove(*it, 1);
        }
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::setPrefix(const VoiceBankRef &bank, int noteNum, const VoicePrefix &prefix,
                                   DiagnosticList &diagnostics) {
        auto transaction = bank.session()->transaction(tr("Set Prefix"));
        const auto map = bank.prefixMap();
        if (map.isValid()) {
            map.setValue(noteNum, prefix);
        } else {
            bank.setPrefixMap(QMap<int, VoicePrefix>{
                {noteNum, prefix}
            });
        }
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::removePrefix(const VoiceBankRef &bank, int noteNum,
                                      DiagnosticList &diagnostics) {
        const auto map = bank.prefixMap();
        if (!map.contains(noteNum)) {
            return fail(diagnostics, tr("The prefix map has no key %1.").arg(noteNum));
        }
        auto transaction = bank.session()->transaction(tr("Remove Prefix"));
        map.remove(noteNum);
        return transaction.commit(diagnostics);
    }

    bool VoiceBankEdits::convertCharset(const VoiceDirectoryRef &directory, const QString &charset,
                                        DiagnosticList &diagnostics) {
        const TextCodec codec(charset);
        if (charset.isEmpty() || !codec.isValid()) {
            return fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(charset));
        }
        // The canonical name, so that another spelling of the encoding in effect is no change.
        auto transaction = directory.session()->transaction(tr("Convert Encoding"));
        directory.setCharsets(codec.name(), QString());
        return transaction.commit(diagnostics);
    }

}
