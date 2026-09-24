#include "VoiceBankRefs.h"

#include <hellokit/EditBase/private/NodeAccess_p.h>

#include "VoiceBankTree_p.h"

namespace hello::kit {

    template <>
    struct edit::NodeOf<VoiceBankRef> : edit::NodeTraits<VoiceBankNode, VoiceBankType> {};
    template <>
    struct edit::NodeOf<VoiceCharacterRef>
        : edit::NodeTraits<VoiceCharacterNode, VoiceCharacterType> {};
    template <>
    struct edit::NodeOf<PrefixMapRef> : edit::MappingTraits {};
    template <>
    struct edit::NodeOf<VoiceDirectoryListRef> : edit::ListTraits<VoiceDirectoryType> {};
    template <>
    struct edit::NodeOf<VoiceDirectoryRef>
        : edit::NodeTraits<VoiceDirectoryNode, VoiceDirectoryType> {};
    template <>
    struct edit::NodeOf<OtoEntryListRef> : edit::ListTraits<OtoEntryType> {};
    template <>
    struct edit::NodeOf<OtoEntryRef> : edit::NodeTraits<OtoEntryNode, OtoEntryType> {};

    // VoiceBankRef

    VoiceBankRef::VoiceBankRef(VoiceBankSession *session)
        : VoiceBankNodeRef(session, session->root()) {
    }

    VoiceCharacterRef VoiceBankRef::character() const {
        return edit::NodeAccess::child<VoiceCharacterRef>(*this, VoiceBankSlots::Character);
    }

    void VoiceBankRef::setCharacter(const std::optional<VoiceCharacter> &character) const {
        edit::NodeAccess::setChild(*this, VoiceBankSlots::Character, character);
    }

    PrefixMapRef VoiceBankRef::prefixMap() const {
        return edit::NodeAccess::child<PrefixMapRef>(*this, VoiceBankSlots::PrefixMap);
    }

    void VoiceBankRef::setPrefixMap(const std::optional<QMap<int, VoicePrefix>> &prefixMap) const {
        edit::NodeAccess::setChild(*this, VoiceBankSlots::PrefixMap, prefixMap);
    }

    QString VoiceBankRef::readme() const {
        return edit::NodeAccess::value(*this, VoiceBankSlots::Readme);
    }

    void VoiceBankRef::setReadme(const QString &readme) const {
        edit::NodeAccess::setValue(*this, VoiceBankSlots::Readme, readme);
    }

    VoiceDirectoryListRef VoiceBankRef::directories() const {
        return edit::NodeAccess::child<VoiceDirectoryListRef>(*this, VoiceBankSlots::Directories);
    }

    VoiceBank VoiceBankRef::toVoiceBank() const {
        return session()->snapshot();
    }

    // VoiceCharacterRef

    QString VoiceCharacterRef::name() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::Name);
    }

    void VoiceCharacterRef::setName(const QString &name) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::Name, name);
    }

    QString VoiceCharacterRef::image() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::Image);
    }

    void VoiceCharacterRef::setImage(const QString &image) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::Image, image);
    }

    QString VoiceCharacterRef::sample() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::Sample);
    }

    void VoiceCharacterRef::setSample(const QString &sample) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::Sample, sample);
    }

    QString VoiceCharacterRef::author() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::Author);
    }

    void VoiceCharacterRef::setAuthor(const QString &author) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::Author, author);
    }

    QString VoiceCharacterRef::web() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::Web);
    }

    void VoiceCharacterRef::setWeb(const QString &web) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::Web, web);
    }

    QStringList VoiceCharacterRef::extraLines() const {
        return edit::NodeAccess::value(*this, VoiceCharacterSlots::ExtraLines);
    }

    void VoiceCharacterRef::setExtraLines(const QStringList &extraLines) const {
        edit::NodeAccess::setValue(*this, VoiceCharacterSlots::ExtraLines, extraLines);
    }

    VoiceCharacter VoiceCharacterRef::toVoiceCharacter() const {
        return edit::NodeAccess::toValue<VoiceCharacter>(*this);
    }

    // PrefixMapRef

    QList<int> PrefixMapRef::keys() const {
        QList<int> keys;
        for (const auto &key : edit::NodeAccess::keys(*this)) {
            keys.push_back(key.toInt());
        }
        return keys;
    }

    bool PrefixMapRef::contains(int noteNum) const {
        return edit::NodeAccess::contains(*this, QString::number(noteNum));
    }

    VoicePrefix PrefixMapRef::value(int noteNum) const {
        return edit::NodeAccess::entry<VoicePrefix>(*this, QString::number(noteNum));
    }

    void PrefixMapRef::setValue(int noteNum, const VoicePrefix &prefix) const {
        edit::NodeAccess::setEntry(*this, QString::number(noteNum), prefix);
    }

    void PrefixMapRef::remove(int noteNum) const {
        edit::NodeAccess::removeEntry(*this, QString::number(noteNum));
    }

    // VoiceDirectoryListRef

    int VoiceDirectoryListRef::size() const {
        return edit::NodeAccess::size(*this);
    }

    VoiceDirectoryRef VoiceDirectoryListRef::at(int index) const {
        return edit::NodeAccess::at<VoiceDirectoryRef>(*this, index);
    }

    // VoiceDirectoryRef

    std::filesystem::path VoiceDirectoryRef::path() const {
        // Stored with slashes, and returned with the separators of the system, as reading
        // produces it.
        return std::filesystem::path(
                   edit::NodeAccess::value(*this, VoiceDirectorySlots::Path).toStdU16String())
            .make_preferred();
    }

    QString VoiceDirectoryRef::charset() const {
        return edit::NodeAccess::value(*this, VoiceDirectorySlots::Charset);
    }

    QString VoiceDirectoryRef::otoCharset() const {
        return edit::NodeAccess::value(*this, VoiceDirectorySlots::OtoCharset);
    }

    OtoEntryListRef VoiceDirectoryRef::otoEntries() const {
        return edit::NodeAccess::child<OtoEntryListRef>(*this, VoiceDirectorySlots::OtoEntries);
    }

    // OtoEntryListRef

    int OtoEntryListRef::size() const {
        return edit::NodeAccess::size(*this);
    }

    OtoEntryRef OtoEntryListRef::at(int index) const {
        return edit::NodeAccess::at<OtoEntryRef>(*this, index);
    }

    void OtoEntryListRef::insert(int index, const QList<VoiceOtoEntry> &entries) const {
        edit::NodeAccess::insert(*this, index, entries);
    }

    void OtoEntryListRef::remove(int index, int count) const {
        edit::NodeAccess::remove(*this, index, count);
    }

    void OtoEntryListRef::move(int index, int count, int destination) const {
        edit::NodeAccess::move(*this, index, count, destination);
    }

    // OtoEntryRef

    QString OtoEntryRef::fileName() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::FileName);
    }

    void OtoEntryRef::setFileName(const QString &fileName) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::FileName, fileName);
    }

    QString OtoEntryRef::alias() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::Alias);
    }

    void OtoEntryRef::setAlias(const QString &alias) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::Alias, alias);
    }

    double OtoEntryRef::offset() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::Offset);
    }

    void OtoEntryRef::setOffset(double offset) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::Offset, offset);
    }

    double OtoEntryRef::consonant() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::Consonant);
    }

    void OtoEntryRef::setConsonant(double consonant) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::Consonant, consonant);
    }

    double OtoEntryRef::cutoff() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::Cutoff);
    }

    void OtoEntryRef::setCutoff(double cutoff) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::Cutoff, cutoff);
    }

    double OtoEntryRef::preUtterance() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::PreUtterance);
    }

    void OtoEntryRef::setPreUtterance(double preUtterance) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::PreUtterance, preUtterance);
    }

    double OtoEntryRef::voiceOverlap() const {
        return edit::NodeAccess::value(*this, OtoEntrySlots::VoiceOverlap);
    }

    void OtoEntryRef::setVoiceOverlap(double voiceOverlap) const {
        edit::NodeAccess::setValue(*this, OtoEntrySlots::VoiceOverlap, voiceOverlap);
    }

    VoiceOtoEntry OtoEntryRef::toVoiceOtoEntry() const {
        return edit::NodeAccess::toValue<VoiceOtoEntry>(*this);
    }

}
