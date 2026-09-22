#include "VoiceBank.h"

#include <algorithm>
#include <fstream>
#include <iterator>
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

        /// The encoding one directory is to be read in, or nothing where nobody said.
        ///
        /// \note No escapes are undone, and none are written: what an encoding cannot hold is
        ///       refused when saving rather than escaped. Undoing escapes in a bank from UTAU
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

        /// Decodes one directory's text, and remembers whether anything did not decode.
        class Decoder {
        public:
            explicit Decoder(const TextCodec &codec) : m_codec(codec) {
            }

            /// The text, or empty where the bytes are not valid in this encoding.
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

        /// The file in \a directory whose name is \a lowerCase in any case, if there is one.
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

        /// One directory decoded, and the samples it holds.
        struct DecodedDirectory {
            VoiceBankDirectory directory;
            QList<VoiceSample> samples;
        };

        /// Decodes \a directory of the bank at \a root in \a given , or leaves out what had to
        /// be decoded where \a given is nothing.
        DecodedDirectory decodeDirectory(const VoiceBankDirectorySource &directory,
                                         const fs::path &root, int directoryIndex,
                                         const std::optional<TextCodec> &given,
                                         DiagnosticList &diagnostics) {
            DecodedDirectory out;
            auto &decoded = out.directory;
            const auto absolute = directory.path.empty() ? root : root / directory.path;

            decoded.path = directory.path;
            decoded.config = directory.config;

            // A directory nobody can name an encoding for loses only what had to be decoded.
            // Its samples are still reachable by file name, which needed no encoding, and that
            // is how a bank without an oto.ini is sung anyway.
            std::optional<TextCodec> codec;
            if (directory.needsCharset()) {
                codec = given;
                if (codec) {
                    decoded.charset = codec->name();
                } else {
                    decoded.leftOut = true;
                }
            }

            // Which audio files an entry already speaks for, so that the rest are added as
            // samples of their own afterwards.
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
                        // The name is in the bank's encoding, so it names a file only once
                        // decoded. Taken as it stands it would be read in the system's code page,
                        // and name another file wherever the two differ.
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

        /// Encodes one directory's UTAU files, which is what saving writes and what opening
        /// takes as the baseline to compare against.
        ///
        /// \return the bytes of every file this directory is to hold, or nothing where some of
        ///         it cannot be written, with the reason in \a diagnostics
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

            // Not quoted where it fails, since a readme is too long to put in a message.
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

    bool utauReadsHere(const QString &charset) {
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

        // A new encoding is written down even where no file comes out different in it, as
        // with plain ASCII. Otherwise the choice would be lost, and the first text that does
        // differ would be written in the old one.
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

        DiagnosticList decoding;
        auto decoded = decodeDirectory(*source, m_root, index, codec, decoding);
        const bool lossy = decoded.directory.lossy;
        diagnostics += decoding;

        // In the place the directory's samples had, so that the order of the bank, and with it
        // which of two equal aliases wins, stays what it was.
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
        book.files = source->files;

        // An encoding that does not read the files is not one to write down. The user can see
        // what it made of them, and choose again.
        book.remember = !lossy;
        m_books[index] = book;

        reindex();
        takeBaseline(index);
        return true;
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

        // Everything is worked out and checked before anything is written, so that a bank
        // that cannot be saved is left as it was rather than half saved.
        for (int i = 0; i < m_directories.size(); ++i) {
            const auto &directory = m_directories.at(i);
            const auto &book = m_books.at(i);
            const auto absolute = directory.path.empty() ? m_root : m_root / directory.path;

            // Nothing of a directory that was never read may be written, since its files would
            // be replaced with nothing. Its samples are the bare files and are not written.
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
                // Text that did not decode reads as empty, and writing it would put the empty
                // text where the original was.
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

            // The encoding goes with the files. Without it written down, the next open would
            // have to ask again, and a record naming another one would read them wrong.
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

        // Whatever is about to be replaced has to be what was read. Something else writing
        // the bank meanwhile, UTAU's setParam for one, would otherwise lose its work to ours.
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
            // Written beside the file and moved over it, so that nothing reading the bank ever
            // finds half of one.
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
        // What the directory would be written as right now, which is what a save compares
        // against to leave alone the files nobody changed. Taken from the encoder and not from
        // the bytes on disk, so that a file only this program would spell differently, with LF
        // line ends or out of order, is not rewritten by a save that did not touch it.
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

        // Only the root's character.txt, readme.txt and prefix.map describe the bank. A
        // subdirectory carrying its own is a bank in its own right, and reading it here would
        // let it rename the one that was opened.
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

            // An entry with no alias of its own is reached by its file name, which is how UTAU
            // writes the first entry of a sample. The alias stays empty in the sample, because
            // empty is what the file says and what is written back.
            if (sample.hasEntry) {
                const auto alias = sample.alias.isEmpty() ? stem : sample.alias;
                if (!m_byAlias.contains(alias)) {
                    m_byAlias.insert(alias, index);
                }
            }

            // UTAU reads a sample's file name as an alias as well, which is why bank authors
            // put a _ in front of a file name they do not want sung by it. Where an entry and a
            // bare file share a name, the entry comes first and carries the timing.
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
