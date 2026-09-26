#ifndef HELLOKIT_EDIT_VOICEBANKSCHEMA_H
#define HELLOKIT_EDIT_VOICEBANKSCHEMA_H

#include <optional>
#include <string>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>

#include <hellokit/VoiceBank/VoiceBank.h>

#include <hellokit/EditBase/Slot.h>

namespace hello::kit {

    // The slots of the nodes of a voice bank tree, see the section on the voice bank in
    // docs/Editing.md. Each record node has the slots of one namespace below. The tree holds the
    // entries of the oto.ini files only: the samples without an entry are the audio files that no
    // entry names, which VoiceBankFileSystemState::audioFiles() lists.

    /// The slots of the root node. The character, the prefix map and the readme belong to the
    /// voice bank as a whole and are read from its root directory only.
    namespace VoiceBankSlots {
        /// A record with the slots of \c VoiceCharacterSlots, or empty if the root has no
        /// \c character.txt .
        inline constexpr edit::ChildSlot Character{0, "character"};

        /// A mapping from the note number, written in decimal, to a \c VoicePrefix, or empty if
        /// the root has no \c prefix.map . An empty mapping denotes a file without entries.
        inline constexpr edit::ChildSlot PrefixMap{1, "prefixMap"};

        inline constexpr edit::Slot<QString> Readme{2, "readme"};

        /// A list of records with the slots of \c VoiceDirectorySlots: the root first, then each
        /// directory read, in the order of reading. Read-only.
        inline constexpr edit::ChildSlot Directories{3, "directories"};

        inline constexpr int count = 4;
    }

    /// The slots of \c character.txt .
    ///
    /// \sa VoiceCharacter
    namespace VoiceCharacterSlots {
        inline constexpr edit::Slot<QString> Name{0, "name"};
        inline constexpr edit::Slot<QString> Image{1, "image"};
        inline constexpr edit::Slot<QString> Sample{2, "sample"};
        inline constexpr edit::Slot<QString> Author{3, "author"};
        inline constexpr edit::Slot<QString> Web{4, "web"};

        /// The lines other than entries, as one value.
        inline constexpr edit::Slot<QStringList> ExtraLines{5, "extraLines"};

        inline constexpr int count = 6;
    }

    /// The slots of one directory. Every slot except the entries is read-only: the path is a fact
    /// of the disk, and the encoding changes through a domain function.
    ///
    /// A directory that was not read is not in the tree.
    ///
    /// \sa VoiceBankDirectory, VoiceBankSession::excludedDirectories()
    namespace VoiceDirectorySlots {
        /// The location relative to the root, with slashes as separators. Empty for the root.
        inline constexpr edit::Slot<QString> Path{0, "path"};

        /// The encoding of every text file of the directory.
        ///
        /// \sa VoiceBankDirectory::charset
        inline constexpr edit::Slot<QString> Charset{1, "charset"};

        /// A list of records with the slots of \c OtoEntrySlots, in the order of reading. Empty
        /// both if the directory has no \c oto.ini and if its \c oto.ini has no entries. The disk
        /// state distinguishes the two, and a missing file is created only for an entry.
        inline constexpr edit::ChildSlot OtoEntries{2, "otoEntries"};

        inline constexpr int count = 3;
    }

    /// The original text of the five numbers of an entry.
    ///
    /// \sa VoiceSample::spellings
    using OtoSpellings = decltype(VoiceOtoEntry::spellings);

    /// The slots of one entry of an \c oto.ini .
    ///
    /// \sa VoiceOtoEntry
    namespace OtoEntrySlots {
        inline constexpr edit::Slot<QString> FileName{0, "fileName"};
        inline constexpr edit::Slot<QString> Alias{1, "alias"};
        inline constexpr edit::Slot<double> Offset{2, "offset"};
        inline constexpr edit::Slot<double> Consonant{3, "consonant"};
        inline constexpr edit::Slot<double> Cutoff{4, "cutoff"};
        inline constexpr edit::Slot<double> PreUtterance{5, "preUtterance"};
        inline constexpr edit::Slot<double> VoiceOverlap{6, "voiceOverlap"};

        /// Internal to the document layer. A spelling that no longer reads as its number is not
        /// written, so a changed number needs no change here.
        inline constexpr edit::Slot<OtoSpellings> Spellings{7, "spellings"};

        inline constexpr int count = 8;
    }

    /// Stores the spellings as a list of five byte arrays, with an invalid value for a number
    /// that was not read, which requires no registration with the Qt meta-type system.
    template <>
    struct edit::SlotValue<OtoSpellings> {
        static inline QVariant toVariant(const OtoSpellings &spellings) {
            QVariantList list;
            for (const auto &spelling : spellings) {
                list.push_back(spelling ? QVariant(QByteArray::fromStdString(*spelling))
                                        : QVariant());
            }
            return list;
        }

        static inline OtoSpellings fromVariant(const QVariant &variant) {
            const auto list = variant.toList();
            OtoSpellings spellings;
            for (qsizetype i = 0; i < qsizetype(spellings.size()) && i < list.size(); ++i) {
                if (list.at(i).isValid()) {
                    spellings[size_t(i)] = list.at(i).toByteArray().toStdString();
                }
            }
            return spellings;
        }
    };

}

#endif // HELLOKIT_EDIT_VOICEBANKSCHEMA_H
