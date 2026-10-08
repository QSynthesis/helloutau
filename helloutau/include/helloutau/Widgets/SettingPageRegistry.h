#ifndef HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H
#define HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H

#include <memory>

#include <QtCore/QString>

#include <stdcorelib/adt/linked_map.h>
#include <stdcorelib/support/dynamicregistry.h>

#include <helloutau/Widgets/SettingPage.h>

namespace hello::daw {

    /// A page created by a factory of a SettingPageRegistry, with its position in the catalog.
    struct SettingPagePlacement {
        std::unique_ptr<SettingPage> page;

        /// The id of the page under which the page is placed, or empty for the top level. The
        /// page is placed at the top level if no page has this id.
        QString parent;

        /// The id of the sibling before which the page is placed, or empty to place the page
        /// last, as in SettingCatalog::addPage().
        QString before;
    };

    /// The traits of SettingPageRegistry, whose factories return a SettingPagePlacement.
    struct SettingPageRegistryTraits {
        using result_type = SettingPagePlacement;

        static result_type empty() {
            return {};
        }
    };

    /// The setting pages of a host, such as an editor, in the order of registration. See the
    /// registration interfaces in docs/Plugins.md.
    ///
    /// The host creates and holds the registry, creates the page of each entry once, and adds it
    /// to its setting catalog. The name of an entry is the id of its page. The host rejects a
    /// page whose id differs from the name. A plugin registers a page with an \c AddFactory
    /// object, creates the object in initialize() and destroys it in aboutToShutdown(), before
    /// its library is unloaded, because the page runs code of the library. The registry is used
    /// only on the application thread.
    using SettingPageRegistry =
        stdc::DynamicRegistry<SettingPage, SettingPageRegistryTraits, stdc::linked_map>;

}

#endif // HELLOUTAU_WIDGETS_SETTINGPAGEREGISTRY_H
