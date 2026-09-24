#ifndef HELLOKIT_EDIT_PROJECTTREE_P_H
#define HELLOKIT_EDIT_PROJECTTREE_P_H

#include <memory>

#include <substate/ArrayNode.h>
#include <substate/Node.h>
#include <substate/VectorNode.h>
#include <qsubstate/MappingNode.h>
#include <qsubstate/StructNode.h>

#include <hellokit/Document/Project.h>

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

    /// Returns the tree of \a project as a free node.
    std::unique_ptr<ss::Node> treeOf(const Project &project);

    /// Returns the project of \a root, a tree with the structure produced by treeOf().
    Project projectOf(const ss::Node *root);

}

#endif // HELLOKIT_EDIT_PROJECTTREE_P_H
