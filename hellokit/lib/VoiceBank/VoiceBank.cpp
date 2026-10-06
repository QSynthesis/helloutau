#include "VoiceBank.h"

#include <algorithm>

#include "VoiceBankFileSystemState.h"

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
        auto opened = VoiceBankFileSystemState::open(root, selector, diagnostics);
        if (!opened) {
            return std::nullopt;
        }
        return std::move(opened->bank);
    }

    std::optional<VoiceBank> VoiceBank::fromSource(const VoiceBankSource &source,
                                                   VoiceBankCharsetSelector *selector,
                                                   DiagnosticList &diagnostics) {
        auto opened = VoiceBankFileSystemState::fromSource(source, selector, diagnostics);
        if (!opened) {
            return std::nullopt;
        }
        return std::move(opened->bank);
    }

    std::optional<fs::path> VoiceBank::imagePathOf(const fs::path &root, const QString &image) {
        if (image.isEmpty() || root.empty()) {
            return std::nullopt;
        }
        // character.txt is a file of UTAU, which writes backslashes.
        auto text = image;
        text.replace(u'\\', u'/');
        const fs::path relative(text.toStdU16String());
        // An absolute path is rejected before the file system is accessed, so that a UNC path
        // such as \\host\share\a.bmp does not make the system connect to the host.
        if (relative.has_root_name() || relative.has_root_directory()) {
            return std::nullopt;
        }
        std::error_code rootError;
        std::error_code pathError;
        auto base = fs::weakly_canonical(root, rootError);
        const auto path = fs::weakly_canonical(root / relative, pathError);
        if (rootError || pathError) {
            return std::nullopt;
        }
        // A trailing separator would leave an empty last component.
        if (!base.has_filename() && base.has_relative_path()) {
            base = base.parent_path();
        }
        // The paths are compared by components, because a string prefix would accept a sibling
        // directory such as bank2 beside bank.
        const auto [inBase, inPath] =
            std::mismatch(base.begin(), base.end(), path.begin(), path.end());
        if (inBase != base.end()) {
            return std::nullopt;
        }
        return path;
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
        slot = std::move(directory);
        reindex();
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

        // Explicit aliases take precedence over the fallback aliases of entries whose alias is
        // empty, regardless of the file name order in oto.ini.
        for (int index = 0; index < m_samples.size(); ++index) {
            const auto &sample = m_samples.at(index);
            if (sample.hasEntry && !sample.alias.isEmpty() && !m_byAlias.contains(sample.alias)) {
                m_byAlias.insert(sample.alias, index);
            }
        }
        for (int index = 0; index < m_samples.size(); ++index) {
            const auto &sample = m_samples.at(index);
            const auto stem = stemOf(sample.fileName);
            // An entry without an alias is found by its file name, which is how UTAU writes the
            // first entry of a sample. The alias of the sample remains empty, because the file
            // specifies it as empty and it is saved as empty.
            if (sample.hasEntry && sample.alias.isEmpty() && !m_byAlias.contains(stem)) {
                m_byAlias.insert(stem, index);
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
