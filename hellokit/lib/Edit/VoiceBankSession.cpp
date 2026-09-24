#include "VoiceBankSession.h"

#include <hellokit/EditBase/private/ChangeLog_p.h>
#include <hellokit/EditBase/private/EditSession_p.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankTree_p.h"
#include "VoiceBankValidation_p.h"

namespace hello::kit {

    std::unique_ptr<VoiceBankSession> VoiceBankSession::create(VoiceBankDiskState::Opened opened,
                                                               DiagnosticList &diagnostics,
                                                               QObject *parent) {
        const auto root = opened.bank.indexOf({});
        if (root < 0 || !isEditable(opened.bank.directories().at(root))) {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.message =
                root < 0 || opened.bank.directories().at(root).leftOut
                    ? tr("The voice bank cannot be edited, because no encoding was specified for "
                         "its folder.")
                    : tr("The voice bank cannot be edited, because part of the text in its folder "
                         "is not valid in its encoding. Open it in another encoding.");
            diagnostics.push_back(diagnostic);
            return nullptr;
        }
        return std::unique_ptr<VoiceBankSession>(new VoiceBankSession(std::move(opened), parent));
    }

    VoiceBankSession::VoiceBankSession(VoiceBankDiskState::Opened opened, QObject *parent)
        : edit::EditSession(parent), m_disk(std::move(opened.disk)) {
        for (const auto &directory : opened.bank.directories()) {
            if (!isEditable(directory)) {
                m_excluded.push_back(directory);
            }
        }
        edit::EditSessionPrivate::setRoot(*this, treeOf(opened.bank));
        registerVoiceBankValidators(*this);
    }

    VoiceBankSession::~VoiceBankSession() = default;

    const std::filesystem::path &VoiceBankSession::rootPath() const {
        return m_disk.root();
    }

    VoiceBank VoiceBankSession::snapshot() const {
        return voiceBankOf(edit::EditSessionPrivate::find(this, root()), m_disk);
    }

    std::optional<QJsonObject> VoiceBankSession::logEntry(const edit::Change &change) const {
        return edit::ChangeLog::entryOf(*this, change, voiceBankRecordOf);
    }

}
