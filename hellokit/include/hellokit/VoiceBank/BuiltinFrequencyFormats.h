#ifndef HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
#define HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H

#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    class FrequencyFormatRegistration;

    /// Registers the formats of this library while it exists: frq of resampler.exe, dio of
    /// world4utau and mrq of moresampler, the formats whose layouts are public.
    ///
    /// The plugin FrequencyEditor holds one, and so does a test that needs them.
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
