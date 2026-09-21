#include "VoiceBank.h"

#include <set>

#include <QtCore/QCoreApplication>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        QString tr(const char *text) {
            return QCoreApplication::translate("hello::kit::VoiceBank", text);
        }

        void complain(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Warning, message});
        }

        QByteArrayView viewOf(const std::string &s) {
            return QByteArrayView(s.data(), qsizetype(s.size()));
        }

        QString displayed(const fs::path &path) {
            return QString::fromStdU16String(path.u16string());
        }

        /// The encoding one directory is to be read in, or nothing where nobody said.
        ///
        /// \note No escapes are undone. Escaping happens on the way out, to a bank this program
        ///       wrote, and nothing here writes one yet. Undoing escapes in a bank from UTAU
        ///       would eat its backslashes.
        std::optional<TextCodec> codecFor(const VoiceBankDirectorySource &directory,
                                          VoiceBankCharsetSelector *selector,
                                          DiagnosticList &diagnostics) {
            QString name;
            if (directory.config) {
                name = directory.config->charset;
            }

            if (name.isEmpty()) {
                if (!selector) {
                    complain(diagnostics,
                             tr("Nothing says what encoding \"%1\" is written in, so it was left "
                                "out.")
                                 .arg(displayed(directory.path)));
                    return std::nullopt;
                }
                const auto chosen = selector->selectCharset(directory, diagnostics);
                if (!chosen) {
                    return std::nullopt;
                }
                name = *chosen;
            }

            TextCodec codec(name);
            if (!codec.isValid()) {
                complain(diagnostics,
                         tr("The encoding \"%1\" is not available, so \"%2\" was left out.")
                             .arg(name, displayed(directory.path)));
                return std::nullopt;
            }
            return codec;
        }

        /// Decodes, or gives back nothing where the bytes are not valid in this encoding.
        QString text(const TextCodec &codec, const std::string &bytes) {
            return codec.decode(viewOf(bytes)).value_or(QString());
        }

        QString stemOf(const fs::path &name) {
            return QString::fromStdU16String(name.stem().u16string());
        }

    }

    std::optional<VoiceBank> VoiceBank::open(const fs::path &root,
                                             VoiceBankCharsetSelector *selector,
                                             DiagnosticList &diagnostics) {
        const auto source = VoiceBankSource::open(root, diagnostics);
        if (!source) {
            return std::nullopt;
        }
        return fromSource(*source, selector, diagnostics);
    }

    std::optional<VoiceBank> VoiceBank::fromSource(const VoiceBankSource &source,
                                                   VoiceBankCharsetSelector *selector,
                                                   DiagnosticList &diagnostics) {
        VoiceBank bank;
        bank.m_root = source.root();
        bank.m_character.name = QString::fromStdU16String(source.root().filename().u16string());

        for (const auto &directory : source.directories()) {
            const auto absolute =
                directory.path.empty() ? source.root() : source.root() / directory.path;

            // A directory nobody can name an encoding for loses only what had to be decoded.
            // Its samples are still reachable by file name, which needed no encoding, and that
            // is how a bank without an oto.ini is sung anyway.
            std::optional<TextCodec> codec;
            if (directory.needsCharset()) {
                codec = codecFor(directory, selector, diagnostics);
            }

            // Only the root's character.txt and readme.txt describe the bank. A subdirectory
            // carrying its own is a bank in its own right, and reading it here would let it
            // rename the one that was opened.
            if (directory.path.empty() && codec) {
                if (directory.character) {
                    const auto &from = *directory.character;
                    if (!from.name.empty()) {
                        bank.m_character.name = text(*codec, from.name);
                    }
                    bank.m_character.image = text(*codec, from.image);
                    bank.m_character.sample = text(*codec, from.sample);
                    bank.m_character.author = text(*codec, from.author);
                    bank.m_character.web = text(*codec, from.web);
                    for (const auto &line : from.extraLines) {
                        bank.m_character.extraLines.push_back(text(*codec, line));
                    }
                }
                if (!directory.readme.isEmpty()) {
                    bank.m_readme = codec->decode(directory.readme).value_or(QString());
                }
                if (directory.prefixMap) {
                    for (const auto &[noteNum, item] : directory.prefixMap->map) {
                        bank.m_prefixMap.insert(noteNum, Affix{text(*codec, item.prefix),
                                                               text(*codec, item.suffix)});
                    }
                }
            }

            // Which audio files an entry already speaks for, so that the rest are added as
            // samples of their own afterwards.
            std::set<std::string> claimed;

            if (directory.oto && codec) {
                for (const auto &[file, entries] : directory.oto->contents) {
                    claimed.insert(file);
                    for (const auto &entry : entries) {
                        VoiceSample sample;
                        sample.path = absolute / fs::path(file);
                        sample.alias = text(*codec, entry.alias);
                        sample.offset = entry.offset;
                        sample.consonant = entry.consonant;
                        sample.cutoff = entry.cutoff;
                        sample.preUtterance = entry.preUtterance;
                        sample.voiceOverlap = entry.voiceOverlap;
                        sample.hasEntry = true;

                        // An entry with no alias of its own is reached by its file name, which
                        // is how UTAU writes the first entry of a sample.
                        if (sample.alias.isEmpty()) {
                            sample.alias = stemOf(fs::path(file));
                        }

                        const int index = int(bank.m_samples.size());
                        bank.m_samples.push_back(sample);
                        if (!bank.m_byAlias.contains(sample.alias)) {
                            bank.m_byAlias.insert(sample.alias, index);
                        }

                        // UTAU reads a sample's file name as an alias as well, which is why
                        // bank authors put a _ in front of a file name they do not want sung by
                        // it. The entry is what carries the timing, so the file name leads here
                        // rather than to a sample with none.
                        const auto stem = stemOf(fs::path(file));
                        if (!bank.m_byStem.contains(stem)) {
                            bank.m_byStem.insert(stem, index);
                        }
                    }
                }
            }

            for (const auto &name : directory.audioFiles) {
                if (claimed.count(name.string()) != 0) {
                    continue;
                }
                VoiceSample sample;
                sample.path = absolute / name;
                sample.alias = stemOf(name);

                const int index = int(bank.m_samples.size());
                bank.m_samples.push_back(sample);
                if (!bank.m_byStem.contains(sample.alias)) {
                    bank.m_byStem.insert(sample.alias, index);
                }
            }
        }

        if (bank.m_samples.isEmpty()) {
            complain(diagnostics, tr("This folder holds nothing that can be sung."));
        }
        return bank;
    }

    QString VoiceBank::prefixedLyric(int noteNum, const QString &lyric) const {
        const auto it = m_prefixMap.find(noteNum);
        if (it == m_prefixMap.end()) {
            return lyric;
        }
        return it->prefix + lyric + it->suffix;
    }

    const VoiceSample *VoiceBank::find(int noteNum, const QString &lyric) const {
        const auto name = prefixedLyric(noteNum, lyric);

        if (const auto it = m_byAlias.find(name); it != m_byAlias.end()) {
            return &m_samples.at(*it);
        }
        if (const auto it = m_byStem.find(name); it != m_byStem.end()) {
            return &m_samples.at(*it);
        }
        return nullptr;
    }

}
