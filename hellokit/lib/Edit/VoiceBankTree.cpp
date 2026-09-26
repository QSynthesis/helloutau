#include "VoiceBankTree_p.h"

#include <QtCore/QSet>

namespace hello::kit {

    namespace fs = std::filesystem;

    namespace {

        template <class T>
        void put(ss::StructNodeBase &node, edit::Slot<T> slot,
                 const typename edit::Slot<T>::ValueType &value) {
            node.setAt(slot.index, edit::SlotValue<T>::toVariant(value));
        }

        void put(ss::StructNodeBase &node, edit::ChildSlot slot, std::unique_ptr<ss::Node> child) {
            node.setAt(slot.index, std::move(child));
        }

        template <class T>
        T get(const ss::StructNodeBase &node, edit::Slot<T> slot) {
            return edit::SlotValue<T>::fromVariant(node.variant(slot.index));
        }

        // Returns the record of type type in node, which the structure of the tree guarantees.
        template <class NodeType>
        const NodeType &recordOf(const ss::Node *node, int type) {
            Q_UNUSED(type)
            Q_ASSERT(node && node->type() == type);
            return static_cast<const NodeType &>(*node);
        }

        const ss::VectorNode &listOf(const ss::Node *node) {
            Q_ASSERT(node && node->type() == ss::Node::Vector);
            return static_cast<const ss::VectorNode &>(*node);
        }

        // The path of a directory in the tree, with slashes as separators on every system.
        QString pathText(const fs::path &path) {
            return QString::fromStdU16String(path.generic_u16string());
        }

        // The path of a directory with the separators of the system, as reading produces it.
        fs::path pathFromText(const QString &text) {
            return fs::path(text.toStdU16String()).make_preferred();
        }

        // The name of an audio file as it is joined to its directory when reading.
        fs::path pathOf(const QString &fileName) {
            return fs::path(fileName.toStdU16String());
        }

        VoiceOtoEntry entryOf(const VoiceSample &sample) {
            VoiceOtoEntry entry;
            entry.fileName = sample.fileName;
            entry.alias = sample.alias;
            entry.offset = sample.offset;
            entry.consonant = sample.consonant;
            entry.cutoff = sample.cutoff;
            entry.preUtterance = sample.preUtterance;
            entry.voiceOverlap = sample.voiceOverlap;
            entry.spellings = sample.spellings;
            return entry;
        }

        // The sample of entry in the directory at index, whose absolute location is absolute.
        VoiceSample sampleOf(const VoiceOtoEntry &entry, int index, const fs::path &absolute) {
            VoiceSample sample;
            sample.path = absolute / pathOf(entry.fileName);
            sample.directory = index;
            sample.fileName = entry.fileName;
            sample.alias = entry.alias;
            sample.offset = entry.offset;
            sample.consonant = entry.consonant;
            sample.cutoff = entry.cutoff;
            sample.preUtterance = entry.preUtterance;
            sample.voiceOverlap = entry.voiceOverlap;
            sample.spellings = entry.spellings;
            sample.hasEntry = true;
            return sample;
        }

    }

    std::unique_ptr<ss::Node> directoryTreeOf(const VoiceBank &bank, int index) {
        const auto &directory = bank.directories().at(index);
        auto entries = std::make_unique<ss::VectorNode>();
        for (const auto &sample : bank.samples()) {
            if (sample.directory == index && sample.hasEntry) {
                entries->append(treeOf(entryOf(sample)));
            }
        }
        auto node = std::make_unique<VoiceDirectoryNode>(VoiceDirectoryType);
        put(*node, VoiceDirectorySlots::Path, pathText(directory.path));
        put(*node, VoiceDirectorySlots::Charset, directory.charset);
        put(*node, VoiceDirectorySlots::OtoEntries, std::move(entries));
        return node;
    }

    void setRootFiles(VoiceBankNode &root, const VoiceBankDirectory &directory) {
        root.setAt(VoiceBankSlots::Character.index, directory.character
                                                        ? ss::Property(treeOf(*directory.character))
                                                        : ss::Property());
        root.setAt(VoiceBankSlots::PrefixMap.index, directory.prefixMap
                                                        ? ss::Property(treeOf(*directory.prefixMap))
                                                        : ss::Property());
        put(root, VoiceBankSlots::Readme, directory.readme);
    }

    fs::path directoryPathOf(const ss::Node *node) {
        return pathFromText(
            get(recordOf<VoiceDirectoryNode>(node, VoiceDirectoryType), VoiceDirectorySlots::Path));
    }

    std::unique_ptr<ss::Node> treeOf(const VoiceBank &bank) {
        auto node = std::make_unique<VoiceBankNode>(VoiceBankType);
        auto directories = std::make_unique<ss::VectorNode>();
        for (int i = 0; i < bank.directories().size(); ++i) {
            // A directory that was not read is taken as absent, see VoiceDirectorySlots.
            const auto &directory = bank.directories().at(i);
            if (!isEditable(directory)) {
                continue;
            }
            directories->append(directoryTreeOf(bank, i));

            // Only the root has these files, see VoiceBankDirectorySource::fileNamed().
            if (directory.path.empty()) {
                setRootFiles(*node, directory);
            }
        }
        put(*node, VoiceBankSlots::Directories, std::move(directories));
        return node;
    }

    VoiceBank voiceBankOf(const ss::Node *tree, const VoiceBankDiskState &disk) {
        const auto &record = recordOf<VoiceBankNode>(tree, VoiceBankType);
        const auto &list = listOf(record.child(VoiceBankSlots::Directories.index));

        QList<VoiceBankDirectory> directories;
        QList<VoiceSample> samples;
        for (int i = 0; i < list.size(); ++i) {
            const auto &node = recordOf<VoiceDirectoryNode>(list.at(i), VoiceDirectoryType);
            VoiceBankDirectory directory;
            directory.path = pathFromText(get(node, VoiceDirectorySlots::Path));
            directory.charset = get(node, VoiceDirectorySlots::Charset);
            if (directory.path.empty()) {
                if (const auto character = record.child(VoiceBankSlots::Character.index)) {
                    directory.character = edit::fromTree<VoiceCharacter>(character);
                }
                if (const auto map = record.child(VoiceBankSlots::PrefixMap.index)) {
                    directory.prefixMap = edit::fromTree<QMap<int, VoicePrefix>>(map);
                }
                directory.readme = get(record, VoiceBankSlots::Readme);
            }

            // The entries in the order of the tree, then the audio files that no entry names,
            // in the order of the listing, as reading places them.
            const auto absolute =
                directory.path.empty() ? disk.root() : disk.root() / directory.path;
            const auto &entries = listOf(node.child(VoiceDirectorySlots::OtoEntries.index));
            QSet<QString> named;
            for (int j = 0; j < entries.size(); ++j) {
                const auto entry = edit::fromTree<VoiceOtoEntry>(entries.at(j));
                named.insert(entry.fileName);
                samples.push_back(sampleOf(entry, i, absolute));
            }
            for (const auto &name : disk.audioFiles(directory.path)) {
                if (named.contains(name)) {
                    continue;
                }
                VoiceSample sample;
                sample.path = absolute / pathOf(name);
                sample.directory = i;
                sample.fileName = name;
                samples.push_back(sample);
            }
            directories.push_back(directory);
        }
        return VoiceBank(disk.root(), directories, samples);
    }

    std::unique_ptr<ss::Node> treeOf(const VoiceCharacter &character) {
        auto node = std::make_unique<VoiceCharacterNode>(VoiceCharacterType);
        put(*node, VoiceCharacterSlots::Name, character.name);
        put(*node, VoiceCharacterSlots::Image, character.image);
        put(*node, VoiceCharacterSlots::Sample, character.sample);
        put(*node, VoiceCharacterSlots::Author, character.author);
        put(*node, VoiceCharacterSlots::Web, character.web);
        put(*node, VoiceCharacterSlots::ExtraLines, character.extraLines);
        return node;
    }

    template <>
    VoiceCharacter edit::fromTree<VoiceCharacter>(const ss::Node *node) {
        const auto &record = recordOf<VoiceCharacterNode>(node, VoiceCharacterType);
        VoiceCharacter character;
        character.name = get(record, VoiceCharacterSlots::Name);
        character.image = get(record, VoiceCharacterSlots::Image);
        character.sample = get(record, VoiceCharacterSlots::Sample);
        character.author = get(record, VoiceCharacterSlots::Author);
        character.web = get(record, VoiceCharacterSlots::Web);
        character.extraLines = get(record, VoiceCharacterSlots::ExtraLines);
        return character;
    }

    std::unique_ptr<ss::Node> treeOf(const QMap<int, VoicePrefix> &map) {
        auto node = std::make_unique<ss::MappingNode>();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            node->setProperty(QString::number(it.key()),
                              edit::SlotValue<VoicePrefix>::toVariant(it.value()));
        }
        return node;
    }

    template <>
    QMap<int, VoicePrefix> edit::fromTree<QMap<int, VoicePrefix>>(const ss::Node *node) {
        Q_ASSERT(node && node->type() == ss::Node::Mapping);
        const auto &mapping = static_cast<const ss::MappingNode &>(*node);
        QMap<int, VoicePrefix> map;
        for (const auto &key : mapping.keys()) {
            map.insert(key.toInt(),
                       edit::SlotValue<VoicePrefix>::fromVariant(mapping.variant(key)));
        }
        return map;
    }

    std::unique_ptr<ss::Node> treeOf(const VoiceOtoEntry &entry) {
        auto node = std::make_unique<OtoEntryNode>(OtoEntryType);
        put(*node, OtoEntrySlots::FileName, entry.fileName);
        put(*node, OtoEntrySlots::Alias, entry.alias);
        put(*node, OtoEntrySlots::Offset, entry.offset);
        put(*node, OtoEntrySlots::Consonant, entry.consonant);
        put(*node, OtoEntrySlots::Cutoff, entry.cutoff);
        put(*node, OtoEntrySlots::PreUtterance, entry.preUtterance);
        put(*node, OtoEntrySlots::VoiceOverlap, entry.voiceOverlap);
        put(*node, OtoEntrySlots::Spellings, entry.spellings);
        return node;
    }

    template <>
    VoiceOtoEntry edit::fromTree<VoiceOtoEntry>(const ss::Node *node) {
        const auto &record = recordOf<OtoEntryNode>(node, OtoEntryType);
        VoiceOtoEntry entry;
        entry.fileName = get(record, OtoEntrySlots::FileName);
        entry.alias = get(record, OtoEntrySlots::Alias);
        entry.offset = get(record, OtoEntrySlots::Offset);
        entry.consonant = get(record, OtoEntrySlots::Consonant);
        entry.cutoff = get(record, OtoEntrySlots::Cutoff);
        entry.preUtterance = get(record, OtoEntrySlots::PreUtterance);
        entry.voiceOverlap = get(record, OtoEntrySlots::VoiceOverlap);
        entry.spellings = get(record, OtoEntrySlots::Spellings);
        return entry;
    }

    void registerVoiceBankTypes(ss::QCodec &codec) {
        codec.registerNodeType(VoiceBankType,
                               [] { return std::make_unique<VoiceBankNode>(VoiceBankType); });
        codec.registerNodeType(VoiceCharacterType, [] {
            return std::make_unique<VoiceCharacterNode>(VoiceCharacterType);
        });
        codec.registerNodeType(VoiceDirectoryType, [] {
            return std::make_unique<VoiceDirectoryNode>(VoiceDirectoryType);
        });
        codec.registerNodeType(OtoEntryType,
                               [] { return std::make_unique<OtoEntryNode>(OtoEntryType); });

        qRegisterMetaType<VoicePrefix>();
    }

}
