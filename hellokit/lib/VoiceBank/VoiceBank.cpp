#include "VoiceBank.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>

#include <hellokit/Support/TextCodec.h>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        void fail(DiagnosticList &diagnostics, const QString &message) {
            diagnostics.push_back({DiagnosticSeverity::Error, message});
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

        QByteArray digestOf(const QByteArray &bytes) {
            return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
        }

        /// The codec for reading one directory, or \c std::nullopt if no encoding is
        /// specified.
        ///
        /// \note Escape sequences are neither decoded nor written. Unrepresentable characters
        ///       are rejected on save instead of escaped. Decoding escape sequences in a voice
        ///       bank from UTAU would remove its backslashes.
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
                             VoiceBank::tr(
                                 "Nothing says what encoding \"%1\" is written in, so it was left "
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
                complain(
                    diagnostics,
                    VoiceBank::tr("The encoding \"%1\" is not available, so \"%2\" was left out.")
                        .arg(name, displayed(directory.path)));
                return std::nullopt;
            }
            return codec;
        }

        /// Decodes the text of one directory and records whether any of it was invalid.
        class Decoder {
        public:
            explicit Decoder(const TextCodec &codec) : m_codec(codec) {
            }

            /// The decoded text, or empty if the bytes are invalid in this encoding.
            QString operator()(const std::string &bytes) {
                return (*this)(viewOf(bytes));
            }

            QString operator()(QByteArrayView bytes) {
                auto text = m_codec.decode(bytes);
                if (!text) {
                    m_lossy = true;
                    return {};
                }
                return *text;
            }

            bool lossy() const {
                return m_lossy;
            }

        private:
            const TextCodec &m_codec;
            bool m_lossy = false;
        };

        int depthOf(const fs::path &relative) {
            return int(std::distance(relative.begin(), relative.end()));
        }

        /// Returns whether \a path equals \a base or lies under it. Both are relative to the
        /// same root, and an empty path denotes the root.
        bool isWithin(const fs::path &path, const fs::path &base) {
            auto p = path.begin();
            for (const auto &part : base) {
                if (p == path.end() || *p != part) {
                    return false;
                }
                ++p;
            }
            return true;
        }

        /// The encoding for rereading a previously read directory. The order of precedence is
        /// the encoding in its current configuration, then the encoding it was read in, and
        /// only then the selector. The user was already asked about a directory that was read
        /// or left out before, and asking again on every change would repeat an answered
        /// question.
        std::optional<TextCodec> codecAgain(const VoiceBankDirectorySource &source,
                                            const VoiceBankDirectory &before,
                                            VoiceBankCharsetSelector *selector,
                                            DiagnosticList &diagnostics) {
            if (!source.needsCharset()) {
                return std::nullopt;
            }
            if (source.config && !source.config->charset.isEmpty()) {
                return codecFor(source, nullptr, diagnostics);
            }
            if (!before.charset.isEmpty()) {
                const TextCodec codec(before.charset);
                if (codec.isValid()) {
                    return codec;
                }
            }
            if (before.leftOut) {
                return std::nullopt;
            }
            // The directory previously required no encoding and now requires one.
            return codecFor(source, selector, diagnostics);
        }

        QString stemOf(const QString &fileName) {
            return QString::fromStdU16String(
                fs::path(fileName.toStdU16String()).stem().u16string());
        }

        std::optional<QByteArray> readWhole(const fs::path &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return std::nullopt;
            }
            const std::string bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
            if (in.bad()) {
                return std::nullopt;
            }
            return QByteArray(bytes.data(), qsizetype(bytes.size()));
        }

        /// The file in \a directory whose name matches \a lowerCase case-insensitively, if any.
        std::optional<fs::path> findFolded(const fs::path &directory, const char *lowerCase) {
            std::error_code error;
            for (const auto &entry : fs::directory_iterator(directory, error)) {
                const auto u8 = entry.path().filename().u8string();
                std::string name(u8.begin(), u8.end());
                for (auto &c : name) {
                    if (c >= 'A' && c <= 'Z') {
                        c = char(c - 'A' + 'a');
                    }
                }
                if (name == lowerCase) {
                    return entry.path();
                }
            }
            return std::nullopt;
        }

        /// One decoded directory and its samples.
        struct DecodedDirectory {
            VoiceBankDirectory directory;
            QList<VoiceSample> samples;
        };

        /// Decodes \a directory of the voice bank at \a root with \a given , or leaves out
        /// all text that requires decoding if \a given is \c std::nullopt .
        DecodedDirectory decodeDirectory(const VoiceBankDirectorySource &directory,
                                         const fs::path &root, int directoryIndex,
                                         const std::optional<TextCodec> &given,
                                         DiagnosticList &diagnostics) {
            DecodedDirectory out;
            auto &decoded = out.directory;
            const auto absolute = directory.path.empty() ? root : root / directory.path;

            decoded.path = directory.path;
            decoded.config = directory.config;

            // A directory without an encoding loses only the text that requires decoding. Its
            // samples remain reachable by file name, which requires no encoding, and a voice
            // bank without an oto.ini is sung in exactly this way.
            std::optional<TextCodec> codec;
            if (directory.needsCharset()) {
                codec = given;
                if (codec) {
                    decoded.charset = codec->name();
                } else {
                    decoded.leftOut = true;
                }
            }

            // The audio files already covered by an entry. The remaining files are added
            // afterward as separate samples.
            QSet<QString> claimed;

            if (codec) {
                Decoder text(*codec);

                if (directory.character) {
                    const auto &from = *directory.character;
                    VoiceCharacter character;
                    character.name = text(from.name);
                    character.image = text(from.image);
                    character.sample = text(from.sample);
                    character.author = text(from.author);
                    character.web = text(from.web);
                    for (const auto &line : from.extraLines) {
                        character.extraLines.push_back(text(line));
                    }
                    decoded.character = character;
                }
                if (!directory.readme.isEmpty()) {
                    decoded.readme = text(directory.readme);
                }
                if (directory.prefixMap) {
                    QMap<int, VoicePrefix> map;
                    for (const auto &[noteNum, item] : directory.prefixMap->map) {
                        map.insert(noteNum, VoicePrefix{text(item.prefix), text(item.suffix)});
                    }
                    decoded.prefixMap = map;
                }

                if (directory.oto) {
                    for (const auto &[file, entries] : directory.oto->contents) {
                        // The name is in the encoding of the voice bank and identifies a file
                        // only after decoding. Used undecoded, it would be interpreted in the
                        // system code page and identify a different file wherever the two
                        // encodings differ.
                        const auto fileName = text(file);
                        const auto path = absolute / fs::path(fileName.toStdU16String());
                        claimed.insert(fileName);
                        for (const auto &entry : entries) {
                            VoiceSample sample;
                            sample.path = path;
                            sample.directory = directoryIndex;
                            sample.fileName = fileName;
                            sample.alias = text(entry.alias);
                            sample.offset = entry.offset;
                            sample.consonant = entry.consonant;
                            sample.cutoff = entry.cutoff;
                            sample.preUtterance = entry.preUtterance;
                            sample.voiceOverlap = entry.voiceOverlap;
                            sample.hasEntry = true;
                            std::copy(std::begin(entry.spellings), std::end(entry.spellings),
                                      sample.spellings.begin());
                            out.samples.push_back(sample);
                        }
                    }
                }

                if (text.lossy()) {
                    decoded.lossy = true;
                    complain(diagnostics,
                             VoiceBank::tr(
                                 "Some of the text in \"%1\" is not valid %2 and reads as empty. "
                                 "Nothing there will be saved, since saving would write the empty "
                                 "text back.")
                                 .arg(displayed(directory.path), codec->name()));
                }
            }

            for (const auto &name : directory.audioFiles) {
                if (claimed.contains(QString::fromStdU16String(name.u16string()))) {
                    continue;
                }
                VoiceSample sample;
                sample.path = absolute / name;
                sample.directory = directoryIndex;
                sample.fileName = QString::fromStdU16String(name.u16string());
                out.samples.push_back(sample);
            }

            return out;
        }

        /// Encodes the UTAU files of one directory. The result is what save() writes and what
        /// open() records as the baseline for comparison.
        ///
        /// \return the content of every file of the directory, or \c std::nullopt if any part
        ///         cannot be written, with the reason in \a diagnostics
        std::optional<std::map<VoiceBankFile, QByteArray>>
            encodeDirectory(const VoiceBankDirectory &directory, int index,
                            const QList<VoiceSample> &samples,
                            const std::map<VoiceBankFile, VoiceBankFileRecord> &files,
                            DiagnosticList &diagnostics) {
            std::map<VoiceBankFile, QByteArray> out;

            QList<const VoiceSample *> entries;
            for (const auto &sample : samples) {
                if (sample.directory == index && sample.hasEntry) {
                    entries.push_back(&sample);
                }
            }

            const auto had = [&files](VoiceBankFile file) { return files.count(file) != 0; };
            const bool hasText = !entries.isEmpty() || directory.character || directory.prefixMap ||
                                 !directory.readme.isEmpty() || had(VoiceBankFile::Oto) ||
                                 had(VoiceBankFile::PrefixMap) || had(VoiceBankFile::Character) ||
                                 had(VoiceBankFile::Readme);
            if (!hasText) {
                return out;
            }

            if (directory.charset.isEmpty()) {
                fail(diagnostics, VoiceBank::tr("Nothing says what encoding to write \"%1\" in.")
                                      .arg(displayed(directory.path)));
                return std::nullopt;
            }
            const TextCodec codec(directory.charset);
            if (!codec.isValid()) {
                fail(diagnostics,
                     VoiceBank::tr("The encoding \"%1\" is not available.").arg(directory.charset));
                return std::nullopt;
            }

            bool ok = true;
            const auto where = [&directory](VoiceBankFile file) {
                const auto found = directory.path / voiceBankFileName(file);
                return displayed(found);
            };
            const auto encode = [&](const QString &text, VoiceBankFile file) {
                if (!codec.canEncode(text)) {
                    fail(diagnostics, VoiceBank::tr("\"%1\" in \"%2\" cannot be written in %3.")
                                          .arg(text, where(file), codec.name()));
                    ok = false;
                    return std::string();
                }
                return codec.encode(text).toStdString();
            };
            const auto removed = [&](VoiceBankFile file) {
                fail(diagnostics, VoiceBank::tr("Saving does not remove \"%1\".").arg(where(file)));
                ok = false;
            };

            if (!entries.isEmpty() || had(VoiceBankFile::Oto)) {
                utau::OtoIni oto;
                for (const auto *sample : entries) {
                    utau::OtoEntry entry;
                    entry.fileName = encode(sample->fileName, VoiceBankFile::Oto);
                    entry.alias = encode(sample->alias, VoiceBankFile::Oto);
                    entry.offset = sample->offset;
                    entry.consonant = sample->consonant;
                    entry.cutoff = sample->cutoff;
                    entry.preUtterance = sample->preUtterance;
                    entry.voiceOverlap = sample->voiceOverlap;
                    std::copy(sample->spellings.begin(), sample->spellings.end(),
                              std::begin(entry.spellings));
                    oto.contents[entry.fileName].push_back(entry);
                }
                out[VoiceBankFile::Oto] = QByteArray::fromStdString(oto.write());
            }

            if (directory.character) {
                const auto &from = *directory.character;
                utau::CharacterTxt character;
                character.name = encode(from.name, VoiceBankFile::Character);
                character.image = encode(from.image, VoiceBankFile::Character);
                character.sample = encode(from.sample, VoiceBankFile::Character);
                character.author = encode(from.author, VoiceBankFile::Character);
                character.web = encode(from.web, VoiceBankFile::Character);
                for (const auto &line : from.extraLines) {
                    character.extraLines.push_back(encode(line, VoiceBankFile::Character));
                }
                out[VoiceBankFile::Character] = QByteArray::fromStdString(character.write());
            } else if (had(VoiceBankFile::Character)) {
                removed(VoiceBankFile::Character);
            }

            if (directory.prefixMap) {
                utau::PrefixMap map;
                for (auto it = directory.prefixMap->begin(); it != directory.prefixMap->end();
                     ++it) {
                    map.map[it.key()] = utau::PrefixMap::Item{
                        encode(it->prefix, VoiceBankFile::PrefixMap),
                        encode(it->suffix, VoiceBankFile::PrefixMap),
                    };
                }
                out[VoiceBankFile::PrefixMap] = QByteArray::fromStdString(map.write());
            } else if (had(VoiceBankFile::PrefixMap)) {
                removed(VoiceBankFile::PrefixMap);
            }

            // The failing text is not quoted, because a readme is too long for a message.
            if (!directory.readme.isEmpty() || had(VoiceBankFile::Readme)) {
                if (!codec.canEncode(directory.readme)) {
                    fail(diagnostics, VoiceBank::tr("Some of \"%1\" cannot be written in %2.")
                                          .arg(where(VoiceBankFile::Readme), codec.name()));
                    ok = false;
                } else {
                    out[VoiceBankFile::Readme] = codec.encode(directory.readme);
                }
            }

            if (!ok) {
                return std::nullopt;
            }
            return out;
        }

    }

    bool VoiceBank::isCharsetReadableByUtau(const QString &charset) {
#ifdef Q_OS_WIN
        const TextCodec codec(charset);
        return codec.isValid() && codec.name() == TextCodec(TextCodec::systemName()).name();
#else
        Q_UNUSED(charset)
        return false;
#endif
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

        for (const auto &directory : source.directories()) {
            std::optional<TextCodec> codec;
            if (directory.needsCharset()) {
                codec = codecFor(directory, selector, diagnostics);
            }
            auto decoded = decodeDirectory(directory, source.root(), int(bank.m_directories.size()),
                                           codec, diagnostics);
            bank.m_samples += decoded.samples;
            bank.m_directories.push_back(decoded.directory);

            Book book;
            book.files = directory.files;
            book.stamp = directory.stamp;
            bank.m_books.push_back(book);
        }

        bank.reindex();
        for (int i = 0; i < bank.m_directories.size(); ++i) {
            bank.takeBaseline(i);
        }

        if (bank.m_samples.isEmpty()) {
            complain(diagnostics, tr("This folder holds nothing that can be sung."));
        }
        return bank;
    }

    void VoiceBank::setSamples(QList<VoiceSample> samples) {
        m_samples = std::move(samples);
        reindex();
    }

    void VoiceBank::setDirectory(int index, VoiceBankDirectory directory) {
        auto &slot = m_directories[index];
        directory.path = slot.path;

        // A new encoding is recorded even if no file changes under it, as with plain ASCII.
        // Otherwise the choice would be lost, and the first text that does differ would be
        // written in the previous encoding.
        if (TextCodec(directory.charset).name() != TextCodec(slot.charset).name()) {
            m_books[index].remember = true;
        }
        slot = std::move(directory);
        reindex();
    }

    bool VoiceBank::reread(int index, const QString &charset, DiagnosticList &diagnostics) {
        if (index < 0 || index >= m_directories.size()) {
            fail(diagnostics, tr("This bank has no directory %1.").arg(index));
            return false;
        }
        const TextCodec codec(charset);
        if (!codec.isValid()) {
            fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(charset));
            return false;
        }
        const auto source =
            VoiceBankSource::readDirectory(m_root, m_directories.at(index).path, diagnostics);
        if (!source) {
            return false;
        }

        replaceDirectory(index, *source, codec, diagnostics);

        // An encoding in which the files are invalid is not recorded. The user can inspect the
        // decoded result and choose again.
        m_books[index].remember = !m_directories.at(index).lossy;

        reindex();
        takeBaseline(index);
        return true;
    }

    bool VoiceBank::isModified(int index) const {
        const auto &directory = m_directories.at(index);
        const auto &book = m_books.at(index);
        if (book.remember) {
            return true;
        }
        DiagnosticList ignored;
        const auto encoded = encodeDirectory(directory, index, m_samples, book.files, ignored);
        if (!encoded) {
            // Content that cannot be written counts as changed.
            return true;
        }
        if (encoded->size() != book.baseline.size()) {
            return true;
        }
        for (const auto &[file, bytes] : *encoded) {
            const auto base = book.baseline.find(file);
            if (base == book.baseline.end() || base->second != digestOf(bytes)) {
                return true;
            }
        }
        return false;
    }

    VoiceBankChanges VoiceBank::checkDisk() {
        return checkDisk(QList<fs::path>{m_root});
    }

    VoiceBankChanges VoiceBank::checkDisk(const QList<fs::path> &places) {
        VoiceBankChanges changes;
        std::error_code error;
        if (!fs::is_directory(m_root, error)) {
            changes.rootGone = true;
            return changes;
        }

        // The places relative to the root. A place outside the root is irrelevant to this voice
        // bank.
        const auto root = m_root.lexically_normal();
        std::vector<fs::path> scope;
        for (const auto &place : places) {
            auto relative = fs::path(place).lexically_normal().lexically_relative(root);
            if (relative.empty() || *relative.begin() == "..") {
                continue;
            }
            if (relative == ".") {
                relative.clear();
            }
            scope.push_back(relative);
        }

        // The directories to examine: every known directory under a place, and the nearest
        // known ancestor, because a new directory appears in the listing of its parent.
        std::set<int> look;
        for (const auto &place : scope) {
            // A known place reveals new entries in its own listing. Only an unknown place
            // requires examining its parent.
            int nearest = -1;
            bool placeKnown = false;
            for (int i = 0; i < m_directories.size(); ++i) {
                const auto &path = m_directories.at(i).path;
                if (isWithin(path, place)) {
                    look.insert(i);
                    placeKnown = placeKnown || path == place;
                } else if (isWithin(place, path) &&
                           (nearest < 0 ||
                            depthOf(path) > depthOf(m_directories.at(nearest).path))) {
                    nearest = i;
                }
            }
            if (!placeKnown && nearest >= 0) {
                look.insert(nearest);
            }
        }

        std::vector<int> order(look.begin(), look.end());
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            return depthOf(m_directories.at(a).path) < depthOf(m_directories.at(b).path);
        });

        std::set<int> removals;
        std::vector<int> rereads;
        std::vector<fs::path> arrivals;

        const auto known = [this](const fs::path &path) {
            return std::any_of(m_directories.begin(), m_directories.end(),
                               [&](const VoiceBankDirectory &d) { return d.path == path; });
        };
        const auto removeUnder = [&](const fs::path &path) {
            for (int i = 0; i < m_directories.size(); ++i) {
                if (isWithin(m_directories.at(i).path, path)) {
                    removals.insert(i);
                }
            }
        };

        for (const int i : order) {
            if (removals.count(i) != 0) {
                continue;
            }
            const auto &path = m_directories.at(i).path;
            auto &book = m_books[i];
            const auto now = VoiceBankDirectoryStamp::take(path.empty() ? m_root : m_root / path);
            if (!now) {
                removeUnder(path);
                continue;
            }

            // Added and removed subdirectories. A removed subdirectory includes its subtree.
            const auto directoriesOf = [](const VoiceBankDirectoryStamp &stamp) {
                std::set<fs::path> out;
                for (const auto &entry : stamp.entries) {
                    if (entry.directory) {
                        out.insert(entry.name);
                    }
                }
                return out;
            };
            const auto before = directoriesOf(book.stamp);
            const auto after = directoriesOf(*now);
            bool listed = false;
            for (const auto &name : after) {
                if (before.count(name) == 0 && !known(path / name)) {
                    arrivals.push_back(path / name);
                    listed = true;
                }
            }
            for (const auto &name : before) {
                if (after.count(name) == 0 && known(path / name)) {
                    removeUnder(path / name);
                    listed = true;
                }
            }

            // The files. A matching stamp is trusted except for racy entries, which were
            // written too close to the snapshot and are compared by content.
            const auto filesOf = [](const VoiceBankDirectoryStamp &stamp) {
                std::vector<VoiceBankDirectoryStamp::Entry> out;
                for (const auto &entry : stamp.entries) {
                    if (!entry.directory) {
                        out.push_back(entry);
                    }
                }
                return out;
            };
            bool changed = filesOf(book.stamp) != filesOf(*now);
            if (!changed) {
                for (const auto &entry : book.stamp.entries) {
                    if (entry.directory || !book.stamp.isRacy(entry)) {
                        continue;
                    }
                    const auto name = entry.name.u8string();
                    std::string folded(name.begin(), name.end());
                    for (auto &c : folded) {
                        if (c >= 'A' && c <= 'Z') {
                            c = char(c - 'A' + 'a');
                        }
                    }
                    const auto kind = voiceBankFileNamed(folded);
                    if (!kind) {
                        continue;
                    }
                    const auto record = book.files.find(*kind);
                    const auto bytes =
                        readWhole((path.empty() ? m_root : m_root / path) / entry.name);
                    if (record == book.files.end() || !bytes ||
                        digestOf(*bytes) != record->second.digest) {
                        changed = true;
                        break;
                    }
                }
            }

            if (changed) {
                rereads.push_back(i);
            } else if (!listed) {
                // No difference was found. The new stamp is taken later, so that entries racy
                // in the old stamp need not be read again at the next check. If a difference
                // was found, the old stamp is retained, so that every subsequent check reports
                // it again until it is reloaded. Otherwise a change reported once and missed
                // would be lost.
                book.stamp = *now;
            }
        }

        for (const int i : rereads) {
            if (removals.count(i) == 0) {
                changes.changed.push_back(m_directories.at(i).path);
            }
        }
        for (const int i : removals) {
            changes.removed.push_back(m_directories.at(i).path);
        }
        for (const auto &path : arrivals) {
            if (!changes.added.contains(path)) {
                changes.added.push_back(path);
            }
        }
        return changes;
    }


    VoiceBankChanges VoiceBank::reloadFromDisk(const VoiceBankChanges &changes,
                                               VoiceBankCharsetSelector *selector,
                                               DiagnosticList &diagnostics) {
        VoiceBankChanges done;
        const auto indexOf = [this](const fs::path &path) {
            for (int i = 0; i < m_directories.size(); ++i) {
                if (m_directories.at(i).path == path) {
                    return i;
                }
            }
            return -1;
        };
        const auto on = [this](const fs::path &path) {
            std::error_code error;
            return fs::is_directory(path.empty() ? m_root : m_root / path, error);
        };

        // The detected changes may be outdated, so each is examined again: a directory
        // removed since is not read, and a directory restored since is not removed.
        std::set<int> removals;
        for (const auto &path : changes.removed) {
            const int i = indexOf(path);
            if (i >= 0 && !on(path)) {
                for (int j = 0; j < m_directories.size(); ++j) {
                    if (isWithin(m_directories.at(j).path, path)) {
                        removals.insert(j);
                    }
                }
            }
        }

        // Changed directories are reread first, while the indices are still valid, discarding
        // unsaved changes, because a reload is the user choosing the version on disk.
        for (const auto &path : changes.changed) {
            const int i = indexOf(path);
            if (i < 0 || removals.count(i) != 0) {
                continue;
            }
            const auto source = VoiceBankSource::readDirectory(m_root, path, diagnostics);
            if (!source) {
                removals.insert(i);
                continue;
            }
            const auto codec = codecAgain(*source, m_directories.at(i), selector, diagnostics);
            replaceDirectory(i, *source, codec, diagnostics);
            done.changed.push_back(path);
        }

        // Removed directories next, in reverse order, so that the remaining indices stay valid.
        for (auto it = removals.rbegin(); it != removals.rend(); ++it) {
            done.removed.push_back(m_directories.at(*it).path);
            removeDirectory(*it);
        }

        // Added directories last, with their subtrees, appended at the end.
        for (const auto &path : changes.added) {
            if (indexOf(path) >= 0 || !on(path)) {
                continue;
            }
            const auto sources = VoiceBankSource::readTree(m_root, path, VoiceBankLimits(),
                                                           int(m_directories.size()), diagnostics);
            for (const auto &source : sources) {
                if (indexOf(source.path) >= 0) {
                    continue;
                }
                std::optional<TextCodec> codec;
                if (source.needsCharset()) {
                    codec = codecFor(source, selector, diagnostics);
                }
                appendDirectory(source, codec, diagnostics);
                done.added.push_back(source.path);
            }
        }

        if (!done.changed.isEmpty() || !done.removed.isEmpty() || !done.added.isEmpty()) {
            reindex();
            for (int i = 0; i < m_directories.size(); ++i) {
                const auto &path = m_directories.at(i).path;
                if (done.changed.contains(path) || done.added.contains(path)) {
                    takeBaseline(i);
                }
            }
        }
        return done;
    }

    VoiceBankChanges VoiceBank::reloadAllFromDisk(VoiceBankCharsetSelector *selector,
                                                  DiagnosticList &diagnostics) {
        // Added and removed directories are still detected by the listings, which compare
        // names rather than times. All other content is read regardless of its stamp.
        auto changes = checkDisk();
        if (changes.rootGone) {
            return changes;
        }
        for (const auto &directory : std::as_const(m_directories)) {
            if (!changes.removed.contains(directory.path) &&
                !changes.changed.contains(directory.path)) {
                changes.changed.push_back(directory.path);
            }
        }
        return reloadFromDisk(changes, selector, diagnostics);
    }

    void VoiceBank::replaceDirectory(int index, const VoiceBankDirectorySource &source,
                                     const std::optional<TextCodec> &codec,
                                     DiagnosticList &diagnostics) {
        auto decoded = decodeDirectory(source, m_root, index, codec, diagnostics);

        // Inserted at the former position of the directory's samples, so that the sample
        // order, and with it the precedence between duplicate aliases, is preserved.
        QList<VoiceSample> samples;
        bool placed = false;
        for (const auto &sample : std::as_const(m_samples)) {
            if (sample.directory != index) {
                samples.push_back(sample);
            } else if (!placed) {
                samples += decoded.samples;
                placed = true;
            }
        }
        if (!placed) {
            samples += decoded.samples;
        }
        m_samples = std::move(samples);
        m_directories[index] = decoded.directory;

        Book book;
        book.files = source.files;
        book.stamp = source.stamp;
        m_books[index] = book;
    }

    void VoiceBank::appendDirectory(const VoiceBankDirectorySource &source,
                                    const std::optional<TextCodec> &codec,
                                    DiagnosticList &diagnostics) {
        const int index = int(m_directories.size());
        auto decoded = decodeDirectory(source, m_root, index, codec, diagnostics);
        m_samples += decoded.samples;
        m_directories.push_back(decoded.directory);

        Book book;
        book.files = source.files;
        book.stamp = source.stamp;
        m_books.push_back(book);
    }

    void VoiceBank::removeDirectory(int index) {
        QList<VoiceSample> samples;
        for (auto sample : std::as_const(m_samples)) {
            if (sample.directory == index) {
                continue;
            }
            if (sample.directory > index) {
                --sample.directory;
            }
            samples.push_back(sample);
        }
        m_samples = std::move(samples);
        m_directories.removeAt(index);
        m_books.removeAt(index);
    }

    void VoiceBank::rememberCharset(int index) {
        if (index >= 0 && index < m_books.size()) {
            m_books[index].remember = true;
        }
    }

    bool VoiceBank::save(DiagnosticList &diagnostics) {
        struct Write {
            int directory;
            VoiceBankFile file;
            fs::path path;
            QByteArray bytes;
        };
        std::vector<Write> writes;
        bool ok = true;

        for (const auto &sample : std::as_const(m_samples)) {
            if (sample.directory < 0 || sample.directory >= m_directories.size()) {
                fail(diagnostics,
                     tr("\"%1\" belongs to no directory of this bank.").arg(sample.fileName));
                ok = false;
            }
        }
        if (!ok) {
            return false;
        }

        // All content is computed and validated before the first write, so that a voice bank
        // that cannot be saved remains unchanged rather than partially saved.
        for (int i = 0; i < m_directories.size(); ++i) {
            const auto &directory = m_directories.at(i);
            const auto &book = m_books.at(i);
            const auto absolute = directory.path.empty() ? m_root : m_root / directory.path;

            // No file of a directory that was never read may be written, because its files
            // would be replaced with empty content. Its samples are bare files and are not
            // written.
            if (directory.leftOut) {
                const bool touched =
                    std::any_of(m_samples.begin(), m_samples.end(), [i](const VoiceSample &sample) {
                        return sample.directory == i && sample.hasEntry;
                    });
                if (touched) {
                    fail(diagnostics, tr("\"%1\" was never read, so nothing can be saved into it.")
                                          .arg(displayed(directory.path)));
                    ok = false;
                }
                continue;
            }

            const auto encoded = encodeDirectory(directory, i, m_samples, book.files, diagnostics);
            if (!encoded) {
                ok = false;
                continue;
            }

            bool changed = false;
            for (const auto &[file, bytes] : *encoded) {
                const auto base = book.baseline.find(file);
                if (base != book.baseline.end() && base->second == digestOf(bytes)) {
                    continue;
                }
                // Invalid text was read as empty, and writing it would replace the original
                // with empty text.
                if (directory.lossy) {
                    fail(diagnostics,
                         tr("Some of the text in \"%1\" did not read in %2, so it cannot be "
                            "saved without losing it.")
                             .arg(displayed(directory.path / voiceBankFileName(file)),
                                  directory.charset));
                    ok = false;
                    continue;
                }
                const auto record = book.files.find(file);
                const auto path =
                    absolute / (record != book.files.end() ? record->second.name
                                                           : fs::path(voiceBankFileName(file)));
                writes.push_back({i, file, path, bytes});
                changed = true;
            }

            // The encoding is recorded together with the files. Otherwise the next open would
            // query the user again, and a configuration naming another encoding would decode
            // them incorrectly.
            if (changed || (book.remember && !directory.charset.isEmpty())) {
                const auto recorded = book.files.find(VoiceBankFile::Config);
                if (recorded != book.files.end() && !directory.config) {
                    fail(diagnostics,
                         tr("The HelloUTAU record in \"%1\" could not be read, so it is not "
                            "replaced, and nothing there is saved.")
                             .arg(displayed(directory.path)));
                    ok = false;
                    continue;
                }
                const QString name = TextCodec(directory.charset).name();
                if (!directory.config || directory.config->charset != name) {
                    VoiceBankConfig config = directory.config.value_or(VoiceBankConfig());
                    config.charset = name;
                    const auto path =
                        absolute / (recorded != book.files.end()
                                        ? recorded->second.name
                                        : fs::path(voiceBankFileName(VoiceBankFile::Config)));
                    writes.push_back({i, VoiceBankFile::Config, path, config.toJson()});
                }
            }
        }

        // Each file about to be replaced must still match the state that was read. Otherwise
        // changes made concurrently by another program, such as the setParam tool of UTAU,
        // would be overwritten.
        for (const auto &write : writes) {
            const auto &book = m_books.at(write.directory);
            const auto record = book.files.find(write.file);
            const auto &directory = m_directories.at(write.directory);
            const auto absolute = directory.path.empty() ? m_root : m_root / directory.path;

            bool same;
            if (record == book.files.end()) {
                same = !findFolded(absolute, voiceBankFileName(write.file));
            } else {
                const auto bytes = readWhole(write.path);
                same = bytes && digestOf(*bytes) == record->second.digest;
            }
            if (!same) {
                fail(diagnostics, tr("\"%1\" has changed since it was read, so it is not "
                                     "replaced. Open the bank again to see what it holds now.")
                                      .arg(displayed(directory.path / write.path.filename())));
                ok = false;
            }
        }

        if (!ok) {
            return false;
        }

        for (const auto &write : writes) {
            // Written to a temporary file beside the target and renamed over it, so that no
            // reader observes a partially written file.
            QSaveFile file(QString::fromStdU16String(write.path.u16string()));
            if (!file.open(QIODevice::WriteOnly) || file.write(write.bytes) != write.bytes.size() ||
                !file.commit()) {
                fail(diagnostics, tr("\"%1\" could not be written.").arg(displayed(write.path)));
                return false;
            }

            auto &book = m_books[write.directory];
            book.files[write.file] =
                VoiceBankFileRecord{write.path.filename(), digestOf(write.bytes)};
            if (write.file == VoiceBankFile::Config) {
                book.remember = false;
                auto &directory = m_directories[write.directory];
                VoiceBankConfig config = directory.config.value_or(VoiceBankConfig());
                config.charset = TextCodec(directory.charset).name();
                directory.config = config;
            } else {
                book.baseline[write.file] = digestOf(write.bytes);
            }
        }
        return true;
    }

    void VoiceBank::takeBaseline(int index) {
        // The current serialization of the directory, against which save() compares to skip
        // unchanged files. Taken from the encoder rather than from the bytes on disk, so that a
        // file this program would serialize differently, for example with LF line endings or in
        // a different order, is not rewritten by a save that did not modify it.
        auto &book = m_books[index];
        book.baseline.clear();
        const auto &directory = m_directories.at(index);
        if (directory.leftOut) {
            return;
        }
        DiagnosticList ignored;
        const auto encoded = encodeDirectory(directory, index, m_samples, book.files, ignored);
        if (encoded) {
            for (const auto &[file, bytes] : *encoded) {
                book.baseline[file] = digestOf(bytes);
            }
        }
    }

    void VoiceBank::reindex() {
        m_byAlias.clear();
        m_byStem.clear();
        m_prefixMap.clear();
        m_readme.clear();
        m_character = VoiceCharacter();
        m_character.name = QString::fromStdU16String(m_root.filename().u16string());

        // Only the character.txt, readme.txt and prefix.map of the root describe the voice
        // bank. A subdirectory with its own files is a separate voice bank, and applying them
        // here would rename the voice bank that was opened.
        for (const auto &directory : std::as_const(m_directories)) {
            if (!directory.path.empty()) {
                continue;
            }
            if (directory.character) {
                const auto name = m_character.name;
                m_character = *directory.character;
                if (m_character.name.isEmpty()) {
                    m_character.name = name;
                }
            }
            m_readme = directory.readme;
            if (directory.prefixMap) {
                m_prefixMap = *directory.prefixMap;
            }
        }

        for (int index = 0; index < m_samples.size(); ++index) {
            const auto &sample = m_samples.at(index);
            const auto stem = stemOf(sample.fileName);

            // An entry without an alias is found by its file name, which is how UTAU writes the
            // first entry of a sample. The alias of the sample remains empty, because the file
            // specifies it as empty and it is saved as empty.
            if (sample.hasEntry) {
                const auto alias = sample.alias.isEmpty() ? stem : sample.alias;
                if (!m_byAlias.contains(alias)) {
                    m_byAlias.insert(alias, index);
                }
            }

            // UTAU also treats the file name of a sample as an alias, which is why voice bank
            // authors prefix a file name with _ to exclude it. If an entry and a bare file share
            // a name, the entry takes precedence and supplies the timing.
            if (!m_byStem.contains(stem)) {
                m_byStem.insert(stem, index);
            }
        }
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
