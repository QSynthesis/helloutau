#ifndef HELLOUTAU_EDITOR_COMMANDENTRIES_P_H
#define HELLOUTAU_EDITOR_COMMANDENTRIES_P_H

#include <QtCore/QList>
#include <QtGui/QAction>

#include <QAKCore/actionregistry.h>
#include <QAKWidgets/widgetactioncontext.h>

#include <helloutau/Widgets/CommandMatcher.h>

namespace hello::daw {

    /// The commands of a window as the command palette offers them: every action of \a context
    /// that is a command and is enabled now, as VS Code shows no disabled command, labelled with
    /// its category as "File: Save".
    inline QList<CommandEntry> commandEntriesOf(const QAK::ActionRegistry *registry,
                                                const QAK::WidgetActionContext *context) {
        QList<CommandEntry> entries;
        for (const auto &id : registry->actionIds()) {
            const auto info = registry->actionInfo(id);
            const auto action = context->action(id);
            if (!info || !info->isCommand() || !action || !action->isEnabled()) {
                continue;
            }
            const auto label = [](const QAK::ActionText &category, const QAK::ActionText &text) {
                const auto title = text.withoutMnemonic();
                const auto group = category.withoutMnemonic();
                return group.isEmpty() ? title : group + QStringLiteral(": ") + title;
            };
            const auto category = info->category();
            const auto text = info->text();
            const auto shown = label(category, text);
            const auto source = label({category.source, std::nullopt}, {text.source, std::nullopt});
            entries.push_back({id, shown, source == shown ? QString() : source, action->shortcut(),
                               action->isCheckable(), action->isChecked()});
        }
        return entries;
    }

}

#endif // HELLOUTAU_EDITOR_COMMANDENTRIES_P_H
