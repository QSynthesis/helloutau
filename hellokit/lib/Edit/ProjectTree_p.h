#ifndef HELLOKIT_EDIT_PROJECTTREE_P_H
#define HELLOKIT_EDIT_PROJECTTREE_P_H

#include <memory>

#include <substate/ArrayNode.h>
#include <substate/Node.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/QCodec.h>
#include <qsubstate/StructNode.h>

#include <hellokit/Document/Project.h>

#include <hellokit/EditBase/private/NodeAccess_p.h>

#include <hellokit/Edit/HelloKitEditGlobal.h>
#include <hellokit/Edit/ProjectSchema.h>

namespace hello::kit {

    /// The node types of a project tree.
    ///
    /// A \c StructNode and an \c ArrayNode require a user type, because the decoder creates a node
    /// from its type, and the default types \c Struct and \c Bytes do not determine the number of
    /// slots or the element type. Lists and mappings use the default types of \c VectorNode and
    /// \c MappingNode.
    enum ProjectNodeType {
        ProjectType = ss::Node::User,
        SettingsType,
        TrackType,
        NoteType,
        PortamentoPointType,
        PitchBendType,
        PitchValuesType,
    };

    using ProjectNode = ss::StructNode<ProjectSlots::count>;
    using SettingsNode = ss::StructNode<SettingsSlots::count>;
    using TrackNode = ss::StructNode<TrackSlots::count>;
    using NoteNode = ss::StructNode<NoteSlots::count>;
    using PortamentoPointNode = ss::StructNode<PortamentoSlots::count>;
    using PitchBendNode = ss::StructNode<PitchBendSlots::count>;
    using PitchValuesNode = ss::ArrayNode<double>;

    // The conversions between each record type and its tree, see fromTree(). The conversions of
    // a project are exported for the tests, which exercise them and the codec without a session.

    HELLOKIT_EDIT_EXPORT std::unique_ptr<ss::Node> treeOf(const Project &project);
    std::unique_ptr<ss::Node> treeOf(const ProjectSettings &settings);
    std::unique_ptr<ss::Node> treeOf(const Track &track);
    std::unique_ptr<ss::Node> treeOf(const Note &note);
    std::unique_ptr<ss::Node> treeOf(const PortamentoPoint &point);
    std::unique_ptr<ss::Node> treeOf(const PitchBend &pitchBend);

    template <>
    HELLOKIT_EDIT_EXPORT Project edit::fromTree<Project>(const ss::Node *node);
    template <>
    ProjectSettings edit::fromTree<ProjectSettings>(const ss::Node *node);
    template <>
    Track edit::fromTree<Track>(const ss::Node *node);
    template <>
    Note edit::fromTree<Note>(const ss::Node *node);
    template <>
    PortamentoPoint edit::fromTree<PortamentoPoint>(const ss::Node *node);
    template <>
    PitchBend edit::fromTree<PitchBend>(const ss::Node *node);

    /// Registers the node types of a project tree with \a codec, and the value types stored in
    /// its slots with the Qt meta-type system, which a decoder requires to find a type by name.
    HELLOKIT_EDIT_EXPORT void registerProjectTypes(ss::QCodec &codec);

}

#endif // HELLOKIT_EDIT_PROJECTTREE_P_H
