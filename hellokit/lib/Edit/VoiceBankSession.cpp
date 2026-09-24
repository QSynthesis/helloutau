#include "VoiceBankSession.h"

#include <hellokit/EditBase/private/ChangeLog_p.h>
#include <hellokit/EditBase/private/EditSession_p.h>

#include "VoiceBankFields_p.h"
#include "VoiceBankTree_p.h"
#include "VoiceBankValidation_p.h"

namespace hello::kit {

    VoiceBankSession::VoiceBankSession(VoiceBankDiskState::Opened opened, QObject *parent)
        : edit::EditSession(parent), m_disk(std::move(opened.disk)) {
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
