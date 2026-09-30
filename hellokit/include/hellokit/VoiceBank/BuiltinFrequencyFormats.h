#ifndef HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
#define HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H

#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    class FrequencyFormatRegistration;

    /// Registrations of the formats of this library for the lifetime of the object: frq of
    /// resampler.exe, dio of world4utau and mrq of moresampler, the formats with public layouts.
    ///
    /// The FrequencyEditor plugin owns an instance. A test that requires these formats owns its
    /// own instance.
    class HELLOKIT_VOICEBANK_EXPORT BuiltinFrequencyFormats {
    public:
        BuiltinFrequencyFormats();
        ~BuiltinFrequencyFormats();

    private:
        std::vector<std::unique_ptr<FrequencyFormatRegistration>> m_registrations;

        Q_DISABLE_COPY_MOVE(BuiltinFrequencyFormats)
    };

}

#endif // HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
