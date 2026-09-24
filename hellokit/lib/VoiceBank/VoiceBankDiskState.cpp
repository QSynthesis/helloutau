#include "VoiceBankDiskState.h"

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
                             VoiceBankDiskState::tr("The encoding of \"%1\" is not specified, "
                                                    "so the directory was left out.")
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
                         VoiceBankDiskState::tr(
                             "The encoding \"%1\" is not available, so \"%2\" was left out.")
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

        /// The file names \a names as VoiceSample::fileName holds them.
        QStringList namesOf(const std::vector<fs::path> &names) {
            QStringList out;
            out.reserve(qsizetype(names.size()));
            for (const auto &name : names) {
                out.push_back(QString::fromStdU16String(name.u16string()));
            }
            return out;
        }

        /// The name \a name in UTF-8 with its ASCII letters in lowercase, for comparison with the
        /// names of VoiceBankDirectorySource::File .
        std::string foldedName(const fs::path &name) {
            const auto u8 = name.u8string();
            std::string folded(u8.begin(), u8.end());
            for (auto &c : folded) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
            }
            return folded;
        }

        /// The file in \a directory whose name matches \a lowerCase case-insensitively, if any.
        std::optional<fs::path> findFolded(const fs::path &directory, const char *lowerCase) {
            std::error_code error;
            for (const auto &entry : fs::directory_iterator(directory, error)) {
                if (foldedName(entry.path().filename()) == lowerCase) {
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

            // The oto.ini may declare its own encoding, which takes precedence over the encoding
            // recorded or selected for the directory.
            std::optional<TextCodec> declared;
            if (directory.oto && !directory.oto->charset.empty()) {
                const auto &raw = directory.oto->charset;
                decoded.otoCharset = QString::fromLatin1(raw.data(), qsizetype(raw.size()));
                const TextCodec candidate(decoded.otoCharset);
                if (candidate.isValid()) {
                    declared = candidate;
                } else {
                    complain(diagnostics,
                             VoiceBankDiskState::tr(
                                 "The oto.ini in \"%1\" declares the encoding \"%2\", "
                                 "which is not available, so it is read in the encoding "
                                 "of the directory.")
                                 .arg(displayed(directory.path), decoded.otoCharset));
                }
            }
            if (declared) {
                QString other;
                if (codec) {
                    other = codec->name();
                } else if (directory.config && !directory.config->charset.isEmpty()) {
                    other = TextCodec(directory.config->charset).name();
                }
                if (!other.isEmpty() && other != declared->name()) {
                    complain(diagnostics,
                             VoiceBankDiskState::tr(
                                 "The oto.ini in \"%1\" declares the encoding %2, so it "
                                 "is read in %2 rather than in %3.")
                                 .arg(displayed(directory.path), declared->name(), other));
                }
                if (!codec && !decoded.leftOut) {
                    decoded.charset = declared->name();
                }
            }

            const auto reportLossy = [&](const TextCodec &used) {
                decoded.lossy = true;
                complain(diagnostics,
                         VoiceBankDiskState::tr(
                             "Some of the text in \"%1\" is not valid %2 and was read as "
                             "empty. No file in this directory will be saved, because "
                             "saving would overwrite the original text with empty text.")
                             .arg(displayed(directory.path), used.name()));
            };

            // The audio files already covered by an entry. The remaining files are added
            // afterward as separate samples.
            QSet<QString> claimed;

            const auto otoCodec = declared ? declared : codec;
            if (otoCodec && directory.oto && !decoded.leftOut) {
                Decoder text(*otoCodec);
                for (const auto &[file, entries] : directory.oto->contents) {
                    // The name is in the encoding of the voice bank and identifies a file only
                    // after decoding. Used undecoded, it would be interpreted in the system
                    // code page and identify a different file wherever the two encodings
                    // differ.
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
                if (text.lossy()) {
                    reportLossy(*otoCodec);
                }
            }

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

                if (text.lossy()) {
                    reportLossy(*codec);
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
        std::optional<std::map<VoiceBankDirectorySource::File, QByteArray>> encodeDirectory(
            const VoiceBankDirectory &directory, int index, const QList<VoiceSample> &samples,
            const std::map<VoiceBankDirectorySource::File, VoiceBankFileRecord> &files,
            DiagnosticList &diagnostics) {
            std::map<VoiceBankDirectorySource::File, QByteArray> out;

            QList<const VoiceSample *> entries;
            for (const auto &sample : samples) {
                if (sample.directory == index && sample.hasEntry) {
                    entries.push_back(&sample);
                }
            }

            const auto had = [&files](VoiceBankDirectorySource::File file) {
                return files.count(file) != 0;
            };
            const bool hasText =
                !entries.isEmpty() || directory.character || directory.prefixMap ||
                !directory.readme.isEmpty() || had(VoiceBankDirectorySource::Oto) ||
                had(VoiceBankDirectorySource::PrefixMap) ||
                had(VoiceBankDirectorySource::Character) || had(VoiceBankDirectorySource::Readme);
            if (!hasText) {
                return out;
            }

            // The oto.ini is written in the encoding it declares, if available, and the other
            // files in the encoding of the directory.
            std::optional<TextCodec> otherCodec;
            if (!directory.charset.isEmpty()) {
                const TextCodec codec(directory.charset);
                if (!codec.isValid()) {
                    fail(diagnostics,
                         VoiceBankDiskState::tr("The encoding \"%1\" is not available.")
                             .arg(directory.charset));
                    return std::nullopt;
                }
                otherCodec = codec;
            }
            auto otoCodec = otherCodec;
            if (!directory.otoCharset.isEmpty()) {
                const TextCodec codec(directory.otoCharset);
                if (codec.isValid()) {
                    otoCodec = codec;
                }
            }

            const bool writesOto = !entries.isEmpty() || had(VoiceBankDirectorySource::Oto);
            const bool writesOther =
                directory.character || directory.prefixMap || !directory.readme.isEmpty() ||
                had(VoiceBankDirectorySource::PrefixMap) ||
                had(VoiceBankDirectorySource::Character) || had(VoiceBankDirectorySource::Readme);
            if ((writesOto && !otoCodec) || (writesOther && !otherCodec)) {
                fail(diagnostics,
                     VoiceBankDiskState::tr("The encoding for writing \"%1\" is not specified.")
                         .arg(displayed(directory.path)));
                return std::nullopt;
            }

            bool ok = true;
            const auto where = [&directory](VoiceBankDirectorySource::File file) {
                const auto found = directory.path / VoiceBankDirectorySource::fileName(file);
                return displayed(found);
            };
            const auto encode = [&](const QString &text, VoiceBankDirectorySource::File file) {
                const auto &codec = file == VoiceBankDirectorySource::Oto ? *otoCodec : *otherCodec;
                if (!codec.canEncode(text)) {
                    fail(diagnostics,
                         VoiceBankDiskState::tr("\"%1\" in \"%2\" cannot be written in %3.")
                             .arg(text, where(file), codec.name()));
                    ok = false;
                    return std::string();
                }
                return codec.encode(text).toStdString();
            };
            const auto removed = [&](VoiceBankDirectorySource::File file) {
                fail(diagnostics,
                     VoiceBankDiskState::tr("Saving does not remove \"%1\".").arg(where(file)));
                ok = false;
            };

            if (writesOto) {
                utau::OtoIni oto;
                if (!directory.otoCharset.isEmpty()) {
                    oto.charset = directory.otoCharset.toLatin1().toStdString();
                } else if (otoCodec->isUtf8()) {
                    oto.charset = "UTF-8";
                }
                for (const auto *sample : entries) {
                    utau::OtoEntry entry;
                    entry.fileName = encode(sample->fileName, VoiceBankDirectorySource::Oto);
                    entry.alias = encode(sample->alias, VoiceBankDirectorySource::Oto);
                    entry.offset = sample->offset;
                    entry.consonant = sample->consonant;
                    entry.cutoff = sample->cutoff;
                    entry.preUtterance = sample->preUtterance;
                    entry.voiceOverlap = sample->voiceOverlap;
                    std::copy(sample->spellings.begin(), sample->spellings.end(),
                              std::begin(entry.spellings));
                    oto.contents[entry.fileName].push_back(entry);
                }
                out[VoiceBankDirectorySource::Oto] = QByteArray::fromStdString(oto.write());
            }

            if (directory.character) {
                const auto &from = *directory.character;
                utau::CharacterTxt character;
                character.name = encode(from.name, VoiceBankDirectorySource::Character);
                character.image = encode(from.image, VoiceBankDirectorySource::Character);
                character.sample = encode(from.sample, VoiceBankDirectorySource::Character);
                character.author = encode(from.author, VoiceBankDirectorySource::Character);
                character.web = encode(from.web, VoiceBankDirectorySource::Character);
                for (const auto &line : from.extraLines) {
                    character.extraLines.push_back(
                        encode(line, VoiceBankDirectorySource::Character));
                }
                out[VoiceBankDirectorySource::Character] =
                    QByteArray::fromStdString(character.write());
            } else if (had(VoiceBankDirectorySource::Character)) {
                removed(VoiceBankDirectorySource::Character);
            }

            if (directory.prefixMap) {
                utau::PrefixMap map;
                for (auto it = directory.prefixMap->begin(); it != directory.prefixMap->end();
                     ++it) {
                    map.map[it.key()] = utau::PrefixMap::Item{
                        encode(it->prefix, VoiceBankDirectorySource::PrefixMap),
                        encode(it->suffix, VoiceBankDirectorySource::PrefixMap),
                    };
                }
                out[VoiceBankDirectorySource::PrefixMap] = QByteArray::fromStdString(map.write());
            } else if (had(VoiceBankDirectorySource::PrefixMap)) {
                removed(VoiceBankDirectorySource::PrefixMap);
            }

            // The failing text is not quoted, because a readme is too long for a message.
            if (!directory.readme.isEmpty() || had(VoiceBankDirectorySource::Readme)) {
                if (!otherCodec->canEncode(directory.readme)) {
                    fail(diagnostics,
                         VoiceBankDiskState::tr("Part of \"%1\" cannot be represented in %2.")
                             .arg(where(VoiceBankDirectorySource::Readme), otherCodec->name()));
                    ok = false;
                } else {
                    out[VoiceBankDirectorySource::Readme] = otherCodec->encode(directory.readme);
                }
            }

            if (!ok) {
                return std::nullopt;
            }
            return out;
        }

    }

    std::optional<VoiceBankDiskState::Opened>
        VoiceBankDiskState::open(const fs::path &root, VoiceBankCharsetSelector *selector,
                                 DiagnosticList &diagnostics) {
        const auto source = VoiceBankSource::open(root, diagnostics);
        if (!source) {
            return std::nullopt;
        }
        return fromSource(*source, selector, diagnostics);
    }

    std::optional<VoiceBankDiskState::Opened>
        VoiceBankDiskState::fromSource(const VoiceBankSource &source,
                                       VoiceBankCharsetSelector *selector,
                                       DiagnosticList &diagnostics) {
        Opened opened{VoiceBank(), VoiceBankDiskState()};
        auto &bank = opened.bank;
        auto &disk = opened.disk;
        bank.m_root = source.root();
        disk.m_root = source.root();

        for (const auto &directory : source.directories()) {
            std::optional<TextCodec> codec;
            if (directory.needsCharset()) {
                codec = codecFor(directory, selector, diagnostics);
            }
            disk.appendDirectory(bank, directory, codec, diagnostics);
        }

        bank.reindex();
        for (int i = 0; i < bank.m_directories.size(); ++i) {
            takeBaseline(bank, i, disk.m_books[bank.m_directories.at(i).path]);
        }

        if (bank.m_samples.isEmpty()) {
            complain(diagnostics, tr("This folder contains no samples."));
        }
        return opened;
    }

    bool VoiceBankDiskState::reread(VoiceBank &bank, const fs::path &directory,
                                    const QString &charset, DiagnosticList &diagnostics) {
        const int index = bank.indexOf(directory);
        if (index < 0 || m_books.count(directory) == 0) {
            fail(diagnostics,
                 tr("This voice bank has no directory \"%1\".").arg(displayed(directory)));
            return false;
        }
        const TextCodec codec(charset);
        if (!codec.isValid()) {
            fail(diagnostics, tr("The encoding \"%1\" is not available.").arg(charset));
            return false;
        }
        const auto source = VoiceBankSource::readDirectory(m_root, directory, diagnostics);
        if (!source) {
            return false;
        }

        replaceDirectory(bank, index, *source, codec, diagnostics);

        // An encoding in which the files are invalid is not recorded. The user can inspect the
        // decoded result and choose again.
        m_books[directory].remember = !bank.m_directories.at(index).lossy;

        bank.reindex();
        takeBaseline(bank, index, m_books[directory]);
        return true;
    }

    bool VoiceBankDiskState::isModified(const VoiceBank &bank, const fs::path &directory) const {
        const int index = bank.indexOf(directory);
        const auto it = m_books.find(directory);
        if (index < 0 || it == m_books.end()) {
            return index >= 0;
        }
        const auto &decoded = bank.m_directories.at(index);
        const auto &book = it->second;
        if (book.remember || (!decoded.charset.isEmpty() &&
                              VoiceBank::canonicalCharset(decoded.charset) != book.charset)) {
            return true;
        }
        DiagnosticList ignored;
        const auto encoded = encodeDirectory(decoded, index, bank.m_samples, book.files, ignored);
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

    QList<fs::path> VoiceBankDiskState::directories() const {
        QList<fs::path> paths;
        for (const auto &[path, book] : m_books) {
            Q_UNUSED(book)
            paths.push_back(path);
        }
        return paths;
    }

    QStringList VoiceBankDiskState::audioFiles(const fs::path &directory) const {
        const auto it = m_books.find(directory);
        return it == m_books.end() ? QStringList() : it->second.audioFiles;
    }

    VoiceBankChanges VoiceBankDiskState::checkDisk() {
        return checkDisk(QList<fs::path>{m_root});
    }

    VoiceBankChanges VoiceBankDiskState::checkDisk(const QList<fs::path> &places) {
        VoiceBankChanges changes;
        std::error_code error;
        if (!fs::is_directory(m_root, error)) {
            changes.rootNotFound = true;
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
        std::set<fs::path> look;
        for (const auto &place : scope) {
            // A known place reveals new entries in its own listing. Only an unknown place
            // requires examining its parent.
            const fs::path *nearest = nullptr;
            bool placeKnown = false;
            for (const auto &[path, book] : m_books) {
                if (isWithin(path, place)) {
                    look.insert(path);
                    placeKnown = placeKnown || path == place;
                } else if (isWithin(place, path) &&
                           (!nearest || depthOf(path) > depthOf(*nearest))) {
                    nearest = &path;
                }
            }
            if (!placeKnown && nearest) {
                look.insert(*nearest);
            }
        }

        std::vector<fs::path> order(look.begin(), look.end());
        std::stable_sort(order.begin(), order.end(), [](const fs::path &a, const fs::path &b) {
            return depthOf(a) < depthOf(b);
        });

        std::set<fs::path> removals;
        std::vector<fs::path> rereads;
        std::vector<fs::path> audios;
        std::vector<fs::path> arrivals;

        const auto known = [this](const fs::path &path) { return m_books.count(path) != 0; };
        const auto removeUnder = [&](const fs::path &path) {
            for (const auto &[other, book] : m_books) {
                if (isWithin(other, path)) {
                    removals.insert(other);
                }
            }
        };

        for (const auto &path : order) {
            if (removals.count(path) != 0) {
                continue;
            }
            auto &book = m_books[path];
            const auto now =
                VoiceBankDirectoryStamp::take(path.empty() ? m_root : m_root / path, path.empty());
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

            // The files, text and audio apart: a changed text file requires rereading the
            // directory, while a changed set of audio files only changes its bare samples. A
            // matching stamp is trusted except for racy text entries, which were written too
            // close to the snapshot and are compared by content.
            const auto kindOf = [&path](const VoiceBankDirectoryStamp::Entry &entry) {
                return VoiceBankDirectorySource::fileNamed(foldedName(entry.name), path.empty());
            };
            const auto entriesOf = [&kindOf](const VoiceBankDirectoryStamp &stamp, bool text) {
                std::vector<VoiceBankDirectoryStamp::Entry> out;
                for (const auto &entry : stamp.entries) {
                    if (!entry.directory && kindOf(entry).has_value() == text) {
                        out.push_back(entry);
                    }
                }
                return out;
            };
            bool changed = entriesOf(book.stamp, true) != entriesOf(*now, true);
            if (!changed) {
                for (const auto &entry : book.stamp.entries) {
                    if (entry.directory || !book.stamp.isRacy(entry)) {
                        continue;
                    }
                    const auto kind = kindOf(entry);
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
            const bool audio = entriesOf(book.stamp, false) != entriesOf(*now, false);

            if (changed) {
                rereads.push_back(path);
            } else if (audio) {
                audios.push_back(path);
            } else if (!listed) {
                // No difference was found. The new stamp is taken later, so that entries racy
                // in the old stamp need not be read again at the next check. If a difference
                // was found, the old stamp is retained, so that every subsequent check reports
                // it again until it is reloaded. Otherwise a change reported once and missed
                // would be lost.
                book.stamp = *now;
            }
        }

        for (const auto &path : audios) {
            if (removals.count(path) == 0) {
                changes.audio.push_back(path);
            }
        }
        for (const auto &path : rereads) {
            if (removals.count(path) == 0) {
                changes.changed.push_back(path);
            }
        }
        for (const auto &path : removals) {
            changes.removed.push_back(path);
        }
        for (const auto &path : arrivals) {
            if (!changes.added.contains(path)) {
                changes.added.push_back(path);
            }
        }
        return changes;
    }

    VoiceBankChanges VoiceBankDiskState::reloadFromDisk(VoiceBank &bank,
                                                        const VoiceBankChanges &changes,
                                                        VoiceBankCharsetSelector *selector,
                                                        DiagnosticList &diagnostics) {
        VoiceBankChanges done;
        const auto on = [this](const fs::path &path) {
            std::error_code error;
            return fs::is_directory(path.empty() ? m_root : m_root / path, error);
        };

        // The detected changes may be outdated, so each is examined again: a directory
        // removed since is not read, and a directory restored since is not removed.
        std::set<int> removals;
        for (const auto &path : changes.removed) {
            const int i = bank.indexOf(path);
            if (i >= 0 && !on(path)) {
                for (int j = 0; j < bank.m_directories.size(); ++j) {
                    if (isWithin(bank.m_directories.at(j).path, path)) {
                        removals.insert(j);
                    }
                }
            }
        }

        // Changed directories are reread first, while the indices are still valid, discarding
        // unsaved changes, because a reload is the user choosing the version on disk.
        for (const auto &path : changes.changed) {
            const int i = bank.indexOf(path);
            if (i < 0 || removals.count(i) != 0) {
                continue;
            }
            const auto source = VoiceBankSource::readDirectory(m_root, path, diagnostics);
            if (!source) {
                removals.insert(i);
                continue;
            }
            const auto codec = codecAgain(*source, bank.m_directories.at(i), selector, diagnostics);
            replaceDirectory(bank, i, *source, codec, diagnostics);
            done.changed.push_back(path);
        }

        // Directories in which only audio files changed, while the indices are still valid.
        // Nothing is discarded, so these are applied regardless of unsaved changes. A directory
        // that is gone by now is left alone, and the next check reports its removal.
        for (const auto &path : changes.audio) {
            const int i = bank.indexOf(path);
            if (i < 0 || removals.count(i) != 0 || done.changed.contains(path)) {
                continue;
            }
            const auto source = VoiceBankSource::readDirectory(m_root, path, diagnostics);
            if (!source) {
                continue;
            }
            refreshAudio(bank, i, *source);
            done.audio.push_back(path);
        }

        // Removed directories next, in reverse order, so that the remaining indices stay valid.
        for (auto it = removals.rbegin(); it != removals.rend(); ++it) {
            done.removed.push_back(bank.m_directories.at(*it).path);
            removeDirectory(bank, *it);
        }

        // Added directories last, with their subtrees, appended at the end.
        for (const auto &path : changes.added) {
            if (bank.indexOf(path) >= 0 || !on(path)) {
                continue;
            }
            const auto sources = VoiceBankSource::readTree(
                m_root, path, VoiceBankLimits(), int(bank.m_directories.size()), diagnostics);
            for (const auto &source : sources) {
                if (bank.indexOf(source.path) >= 0) {
                    continue;
                }
                std::optional<TextCodec> codec;
                if (source.needsCharset()) {
                    codec = codecFor(source, selector, diagnostics);
                }
                appendDirectory(bank, source, codec, diagnostics);
                done.added.push_back(source.path);
            }
        }

        if (!done.changed.isEmpty() || !done.audio.isEmpty() || !done.removed.isEmpty() ||
            !done.added.isEmpty()) {
            bank.reindex();
            for (int i = 0; i < bank.m_directories.size(); ++i) {
                const auto &path = bank.m_directories.at(i).path;
                if (done.changed.contains(path) || done.added.contains(path)) {
                    takeBaseline(bank, i, m_books[path]);
                }
            }
        }
        return done;
    }

    VoiceBankChanges VoiceBankDiskState::reloadAllFromDisk(VoiceBank &bank,
                                                           VoiceBankCharsetSelector *selector,
                                                           DiagnosticList &diagnostics) {
        // Added and removed directories are still detected by the listings, which compare
        // names rather than times. All other content is read regardless of its stamp.
        auto changes = checkDisk();
        if (changes.rootNotFound) {
            return changes;
        }
        for (const auto &directory : std::as_const(bank.m_directories)) {
            if (!changes.removed.contains(directory.path) &&
                !changes.changed.contains(directory.path)) {
                changes.changed.push_back(directory.path);
            }
        }
        return reloadFromDisk(bank, changes, selector, diagnostics);
    }

    void VoiceBankDiskState::replaceDirectory(VoiceBank &bank, int index,
                                              const VoiceBankDirectorySource &source,
                                              const std::optional<TextCodec> &codec,
                                              DiagnosticList &diagnostics) {
        auto decoded = decodeDirectory(source, m_root, index, codec, diagnostics);

        // Inserted at the former position of the directory's samples, so that the sample
        // order, and with it the precedence between duplicate aliases, is preserved.
        QList<VoiceSample> samples;
        bool placed = false;
        for (const auto &sample : std::as_const(bank.m_samples)) {
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
        bank.m_samples = std::move(samples);
        bank.m_directories[index] = decoded.directory;
        m_books[source.path] = bookOf(source, decoded.directory);
    }

    void VoiceBankDiskState::appendDirectory(VoiceBank &bank,
                                             const VoiceBankDirectorySource &source,
                                             const std::optional<TextCodec> &codec,
                                             DiagnosticList &diagnostics) {
        const int index = int(bank.m_directories.size());
        auto decoded = decodeDirectory(source, m_root, index, codec, diagnostics);
        bank.m_samples += decoded.samples;
        bank.m_directories.push_back(decoded.directory);
        m_books[source.path] = bookOf(source, decoded.directory);
    }

    void VoiceBankDiskState::refreshAudio(VoiceBank &bank, int index,
                                          const VoiceBankDirectorySource &source) {
        const auto absolute = source.path.empty() ? m_root : m_root / source.path;

        // The audio files covered by an entry, as when decoding. The rest become samples
        // without an entry.
        QSet<QString> claimed;
        for (const auto &sample : std::as_const(bank.m_samples)) {
            if (sample.directory == index && sample.hasEntry) {
                claimed.insert(sample.fileName);
            }
        }
        QList<VoiceSample> bare;
        for (const auto &name : source.audioFiles) {
            const auto fileName = QString::fromStdU16String(name.u16string());
            if (claimed.contains(fileName)) {
                continue;
            }
            VoiceSample sample;
            sample.path = absolute / name;
            sample.directory = index;
            sample.fileName = fileName;
            bare.push_back(sample);
        }

        // Placed after the entries of the directory, where decoding places them.
        QList<VoiceSample> samples;
        int at = -1;
        for (const auto &sample : std::as_const(bank.m_samples)) {
            if (sample.directory == index && !sample.hasEntry) {
                continue;
            }
            samples.push_back(sample);
            if (sample.directory == index) {
                at = int(samples.size());
            }
        }
        if (at < 0) {
            at = int(samples.size());
            for (int i = 0; i < samples.size(); ++i) {
                if (samples.at(i).directory > index) {
                    at = i;
                    break;
                }
            }
        }
        for (int i = 0; i < bare.size(); ++i) {
            samples.insert(at + i, bare.at(i));
        }
        bank.m_samples = std::move(samples);

        auto &book = m_books[source.path];
        book.audioFiles = namesOf(source.audioFiles);

        // The new stamp is taken only if every text file is still as it was read. Otherwise a
        // text file changed since the check would be taken as read and never reported.
        bool same = source.files.size() == book.files.size();
        for (const auto &[file, record] : source.files) {
            const auto it = book.files.find(file);
            same = same && it != book.files.end() && it->second.digest == record.digest;
        }
        if (same) {
            book.stamp = source.stamp;
        }
    }

    void VoiceBankDiskState::removeDirectory(VoiceBank &bank, int index) {
        QList<VoiceSample> samples;
        for (auto sample : std::as_const(bank.m_samples)) {
            if (sample.directory == index) {
                continue;
            }
            if (sample.directory > index) {
                --sample.directory;
            }
            samples.push_back(sample);
        }
        bank.m_samples = std::move(samples);
        m_books.erase(bank.m_directories.at(index).path);
        bank.m_directories.removeAt(index);
    }

    void VoiceBankDiskState::rememberCharset(const fs::path &directory) {
        const auto it = m_books.find(directory);
        if (it != m_books.end()) {
            it->second.remember = true;
        }
    }

    bool VoiceBankDiskState::hasUnrecordedCharsets() const {
        return std::any_of(m_books.begin(), m_books.end(),
                           [](const auto &item) { return item.second.remember; });
    }

    bool VoiceBankDiskState::save(const VoiceBank &bank, DiagnosticList &diagnostics) {
        struct Write {
            fs::path directory;
            VoiceBankDirectorySource::File file;
            fs::path path;
            QByteArray bytes;
        };
        std::vector<Write> writes;
        // The directories whose encoding is recorded on disk once the writes succeed, with the
        // canonical name of that encoding.
        std::vector<std::pair<fs::path, QString>> recorded;
        bool ok = true;

        const auto &directories = bank.m_directories;
        const auto &samples = bank.m_samples;
        for (const auto &sample : samples) {
            if (sample.directory < 0 || sample.directory >= directories.size()) {
                fail(diagnostics, tr("\"%1\" does not belong to any directory of this voice bank.")
                                      .arg(sample.fileName));
                ok = false;
            }
        }
        if (!ok) {
            return false;
        }

        // A directory without state was not read from disk, for example one that a reload
        // removed and an undo restored. It is saved as a new directory, created if missing, with
        // an empty state: every file is written, and a file already there is not replaced,
        // because it was not read.
        //
        // Without the root, no file that was read exists any longer, and every directory is new.
        std::error_code rootError;
        const bool rootGone = !fs::is_directory(m_root, rootError);
        const Book none;
        std::set<fs::path> created;
        const auto bookOf = [this, rootGone, &none](const fs::path &path) -> const Book & {
            const auto found = m_books.find(path);
            return rootGone || found == m_books.end() ? none : found->second;
        };

        // All content is computed and validated before the first write, so that a voice bank
        // that cannot be saved remains unchanged rather than partially saved.
        for (int i = 0; i < directories.size(); ++i) {
            const auto &directory = directories.at(i);
            const auto absolute = directory.path.empty() ? m_root : m_root / directory.path;
            if (rootGone || m_books.count(directory.path) == 0) {
                std::error_code error;
                if (fs::exists(absolute, error) && !fs::is_directory(absolute, error)) {
                    fail(diagnostics, tr("\"%1\" is a file, so the folder cannot be created.")
                                          .arg(displayed(directory.path)));
                    ok = false;
                    continue;
                }
                created.insert(directory.path);
            }
            const auto &book = bookOf(directory.path);

            // No file of a directory that was never read may be written, because its files
            // would be replaced with empty content. Its samples are bare files and are not
            // written.
            if (directory.leftOut) {
                const bool touched =
                    std::any_of(samples.begin(), samples.end(), [i](const VoiceSample &sample) {
                        return sample.directory == i && sample.hasEntry;
                    });
                if (touched) {
                    fail(diagnostics, tr("\"%1\" was not read, so nothing can be saved into it.")
                                          .arg(displayed(directory.path)));
                    ok = false;
                }
                continue;
            }

            const auto encoded = encodeDirectory(directory, i, samples, book.files, diagnostics);
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
                         tr("Some of the text in \"%1\" is not valid %2, so saving would lose it.")
                             .arg(displayed(directory.path /
                                            VoiceBankDirectorySource::fileName(file)),
                                  directory.charset));
                    ok = false;
                    continue;
                }
                const auto record = book.files.find(file);
                const auto path =
                    absolute / (record != book.files.end()
                                    ? record->second.name
                                    : fs::path(VoiceBankDirectorySource::fileName(file)));
                writes.push_back({directory.path, file, path, bytes});
                changed = true;
            }

            // The encoding is recorded together with the files, and when it changed. Otherwise
            // the next open would query the user again, and a configuration naming another
            // encoding would decode the files incorrectly.
            const QString name = VoiceBank::canonicalCharset(directory.charset);
            if (changed || (!name.isEmpty() && (book.remember || name != book.charset))) {
                const auto configRecord = book.files.find(VoiceBankDirectorySource::Config);
                if (configRecord != book.files.end() && !book.config) {
                    fail(diagnostics,
                         tr("The HelloUtau configuration in \"%1\" could not be read, so it is not "
                            "replaced and no file in this directory is saved.")
                             .arg(displayed(directory.path)));
                    ok = false;
                    continue;
                }
                recorded.emplace_back(directory.path, name);
                if (!book.config || book.config->charset != name) {
                    VoiceBankConfig config = book.config.value_or(VoiceBankConfig());
                    config.charset = name;
                    const auto path = absolute / (configRecord != book.files.end()
                                                      ? configRecord->second.name
                                                      : fs::path(VoiceBankDirectorySource::fileName(
                                                            VoiceBankDirectorySource::Config)));
                    writes.push_back(
                        {directory.path, VoiceBankDirectorySource::Config, path, config.toJson()});
                }
            }
        }

        // Each file about to be replaced must still match the state that was read. Otherwise
        // changes made concurrently by another program, such as the setParam tool of UTAU,
        // would be overwritten.
        for (const auto &write : writes) {
            const auto &book = bookOf(write.directory);
            const auto record = book.files.find(write.file);
            const auto absolute = write.directory.empty() ? m_root : m_root / write.directory;

            bool same;
            if (record == book.files.end()) {
                same = !findFolded(absolute, VoiceBankDirectorySource::fileName(write.file));
            } else {
                const auto bytes = readWhole(write.path);
                same = bytes && digestOf(*bytes) == record->second.digest;
            }
            if (!same) {
                fail(diagnostics,
                     tr("\"%1\" has been modified since it was read, so it is not replaced. Reopen "
                        "the voice bank to load its current contents.")
                         .arg(displayed(write.directory / write.path.filename())));
                ok = false;
            }
        }

        if (!ok) {
            return false;
        }

        // The state of the files that were read describes nothing on disk any longer.
        if (rootGone) {
            m_books.clear();
        }

        for (const auto &write : writes) {
            if (created.count(write.directory) != 0) {
                std::error_code error;
                fs::create_directories(write.path.parent_path(), error);
                if (error) {
                    fail(diagnostics, tr("The folder \"%1\" could not be created.")
                                          .arg(displayed(write.directory)));
                    return false;
                }
            }
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
            if (write.file == VoiceBankDirectorySource::Config) {
                VoiceBankConfig config = book.config.value_or(VoiceBankConfig());
                config.charset = VoiceBank::canonicalCharset(
                    directories.at(bank.indexOf(write.directory)).charset);
                book.config = config;
            } else {
                book.baseline[write.file] = digestOf(write.bytes);
            }
        }
        for (const auto &[path, name] : recorded) {
            auto &book = m_books[path];
            book.charset = name;
            book.remember = false;
        }

        // A directory saved without state now has files on disk, whose listing and audio files
        // are taken as the state, as if it had been read.
        for (const auto &path : created) {
            const auto found = m_books.find(path);
            if (found == m_books.end()) {
                continue;
            }
            DiagnosticList ignored;
            if (const auto source = VoiceBankSource::readDirectory(m_root, path, ignored)) {
                found->second.stamp = source->stamp;
                found->second.audioFiles = namesOf(source->audioFiles);
            }
        }
        return true;
    }

    std::optional<VoiceBankDiskState::Opened>
        VoiceBankDiskState::saveAs(const VoiceBank &bank, const fs::path &folder,
                                   bool copyOtherFiles, DiagnosticList &diagnostics) {
        std::error_code error;
        if (fs::exists(folder, error) &&
            (!fs::is_directory(folder, error) || !fs::is_empty(folder, error))) {
            fail(diagnostics, tr("\"%1\" is not an empty folder, so the voice bank is not saved "
                                 "into it.")
                                  .arg(displayed(folder)));
            return std::nullopt;
        }
        fs::create_directories(folder, error);
        if (error) {
            fail(diagnostics, tr("The folder \"%1\" could not be created.").arg(displayed(folder)));
            return std::nullopt;
        }

        // The text files first, into a state without any directory, which checks everything
        // before the first write.
        VoiceBankDiskState state;
        state.m_root = folder;
        if (!state.save(bank, diagnostics)) {
            return std::nullopt;
        }

        if (copyOtherFiles && !fs::is_directory(bank.m_root, error)) {
            complain(diagnostics, tr("The original folder no longer exists, so only the text "
                                     "files are saved."));
        } else if (copyOtherFiles) {
            // Each file that saving writes for a directory of the bank is skipped, whether or not
            // it was written, so that nothing written is replaced by its original.
            const auto options = fs::directory_options::skip_permission_denied;
            for (auto it = fs::recursive_directory_iterator(bank.m_root, options, error);
                 !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
                const auto &entry = *it;
                std::error_code status;
                if (entry.is_symlink(status) || !entry.is_regular_file(status)) {
                    continue;
                }
                const auto relative = entry.path().lexically_relative(bank.m_root);
                const auto directory = relative.parent_path();
                if (bank.indexOf(directory) >= 0 &&
                    VoiceBankDirectorySource::fileNamed(foldedName(relative.filename()),
                                                        directory.empty())) {
                    continue;
                }
                const auto target = folder / relative;
                std::error_code copying;
                fs::create_directories(target.parent_path(), copying);
                if (!copying) {
                    fs::copy_file(entry.path(), target, copying);
                }
                if (copying) {
                    fail(diagnostics, tr("\"%1\" could not be copied.").arg(displayed(relative)));
                    return std::nullopt;
                }
            }
            if (error) {
                fail(diagnostics,
                     tr("The original folder could not be read, so not every file is copied."));
                return std::nullopt;
            }
        }
        return open(folder, nullptr, diagnostics);
    }

    VoiceBankDiskState::Book VoiceBankDiskState::bookOf(const VoiceBankDirectorySource &source,
                                                        const VoiceBankDirectory &decoded) {
        Book book;
        book.files = source.files;
        book.config = source.config;
        book.charset = VoiceBank::canonicalCharset(decoded.charset);
        book.stamp = source.stamp;
        book.audioFiles = namesOf(source.audioFiles);
        return book;
    }

    void VoiceBankDiskState::takeBaseline(const VoiceBank &bank, int index, Book &book) {
        // The current serialization of the directory, against which save() compares to skip
        // unchanged files. Taken from the encoder rather than from the bytes on disk, so that a
        // file this program would serialize differently, for example with LF line endings or in
        // a different order, is not rewritten by a save that did not modify it.
        book.baseline.clear();
        const auto &directory = bank.m_directories.at(index);
        if (directory.leftOut) {
            return;
        }
        DiagnosticList ignored;
        const auto encoded = encodeDirectory(directory, index, bank.m_samples, book.files, ignored);
        if (encoded) {
            for (const auto &[file, bytes] : *encoded) {
                book.baseline[file] = digestOf(bytes);
            }
        }
    }

}
