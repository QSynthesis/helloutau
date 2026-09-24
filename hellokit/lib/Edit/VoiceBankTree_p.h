#ifndef HELLOKIT_EDIT_VOICEBANKTREE_P_H
#define HELLOKIT_EDIT_VOICEBANKTREE_P_H

#include <memory>

#include <substate/Node.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/QCodec.h>
#include <qsubstate/StructNode.h>

#include <hellokit/VoiceBank/VoiceBank.h>
#include <hellokit/VoiceBank/VoiceBankDiskState.h>

#include <hellokit/EditBase/private/NodeAccess_p.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/VoiceBankSchema.h>

namespace hello::kit {

    /// The node types of a voice bank tree, see ProjectNodeType. They follow the types of a
    /// project at a distance, so that a node type identifies its document.
    enum VoiceBankNodeType {
        VoiceBankType = ss::Node::User + 32,
        VoiceCharacterType,
        VoiceDirectoryType,
        OtoEntryType,
    };

    using VoiceBankNode = ss::StructNode<VoiceBankSlots::count>;
    using VoiceCharacterNode = ss::StructNode<VoiceCharacterSlots::count>;
    using VoiceDirectoryNode = ss::StructNode<VoiceDirectorySlots::count>;
    using OtoEntryNode = ss::StructNode<OtoEntrySlots::count>;

    // The conversions between a voice bank and its tree. The tree holds the entries and not the
    // samples without an entry, therefore a VoiceBank is assembled from the tree together with
    // the audio files that the disk state lists. The conversions of a voice bank are exported for
    // the tests, which exercise them and the codec without a session.

    HELLOKIT_EDIT_EXPORT std::unique_ptr<ss::Node> treeOf(const VoiceBank &bank);
    std::unique_ptr<ss::Node> treeOf(const VoiceCharacter &character);
    std::unique_ptr<ss::Node> treeOf(const VoiceOtoEntry &entry);

    /// Returns the mapping of a prefix map, keyed by the note number in decimal.
    std::unique_ptr<ss::Node> treeOf(const QMap<int, VoicePrefix> &map);

    /// Returns the voice bank in \a tree, at the root of \a disk and with its audio files.
    HELLOKIT_EDIT_EXPORT VoiceBank voiceBankOf(const ss::Node *tree,
                                               const VoiceBankDiskState &disk);

    template <>
    HELLOKIT_EDIT_EXPORT VoiceCharacter edit::fromTree<VoiceCharacter>(const ss::Node *node);
    template <>
    HELLOKIT_EDIT_EXPORT VoiceOtoEntry edit::fromTree<VoiceOtoEntry>(const ss::Node *node);
    template <>
    QMap<int, VoicePrefix> edit::fromTree<QMap<int, VoicePrefix>>(const ss::Node *node);

    /// Registers the node types of a voice bank tree with \a codec, and the value types stored in
    /// its slots with the Qt meta-type system, which a decoder requires to find a type by name.
    HELLOKIT_EDIT_EXPORT void registerVoiceBankTypes(ss::QCodec &codec);

}

#endif // HELLOKIT_EDIT_VOICEBANKTREE_P_H
