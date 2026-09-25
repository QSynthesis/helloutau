#include "VoiceBank.h"

#include <hellokit/Support/TextCodec.h>

#include "VoiceBankDiskState.h"

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        QString stemOf(const QString &fileName) {
            return QString::fromStdU16String(
                fs::path(fileName.toStdU16String()).stem().u16string());
        }

    }

    VoiceBank::VoiceBank(fs::path root, QList<VoiceBankDirectory> directories,
                         QList<VoiceSample> samples)
        : m_root(std::move(root)), m_directories(std::move(directories)),
          m_samples(std::move(samples)) {
        reindex();
    }

    std::optional<VoiceBank> VoiceBank::open(const fs::path &root,
                                             VoiceBankCharsetSelector *selector,
                                             DiagnosticList &diagnostics) {
        auto opened = VoiceBankDiskState::open(root, selector, diagnostics);
        if (!opened) {
            return std::nullopt;
        }
        return std::move(opened->bank);
    }

    std::optional<VoiceBank> VoiceBank::fromSource(const VoiceBankSource &source,
                                                   VoiceBankCharsetSelector *selector,
                                                   DiagnosticList &diagnostics) {
        auto opened = VoiceBankDiskState::fromSource(source, selector, diagnostics);
        if (!opened) {
            return std::nullopt;
        }
        return std::move(opened->bank);
    }

    int VoiceBank::indexOf(const fs::path &directory) const {
        for (int i = 0; i < m_directories.size(); ++i) {
            if (m_directories.at(i).path == directory) {
                return i;
            }
        }
        return -1;
    }

    void VoiceBank::setSamples(QList<VoiceSample> samples) {
        m_samples = std::move(samples);
        reindex();
    }

    void VoiceBank::setDirectory(int index, VoiceBankDirectory directory) {
        auto &slot = m_directories[index];
        directory.path = slot.path;
        if (canonicalCharset(directory.charset) != canonicalCharset(slot.charset)) {
            directory.otoCharset.clear();
        }
        slot = std::move(directory);
        reindex();
    }

    QString VoiceBank::canonicalCharset(const QString &charset) {
        // An empty encoding is kept empty. TextCodec would take it as the system encoding, which
        // is UTF-8 on most systems other than Windows and would equal a new UTF-8.
        return charset.isEmpty() ? QString() : TextCodec(charset).name();
    }

    void VoiceBank::reindex() {
        m_byAlias.clear();
        m_byStem.clear();
        m_prefixMap.clear();
        m_readme.clear();
        m_character = VoiceCharacter();
        m_character.name = QString::fromStdU16String(m_root.filename().u16string());

        // Only the root has a character.txt, readme.txt and prefix.map. See
        // VoiceBankDirectorySource::fileNamed().
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
