#ifndef HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
#define HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H

#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/FrequencyFormatRegistry.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Registrations of the formats of this library in \a registry for the lifetime of the
    /// object: frq of resampler.exe, dio of world4utau and mrq of moresampler, the formats with
    /// public layouts.
    ///
    /// The FrequencyEditor plugin owns an instance for the registry of the editor. A test that
    /// requires these formats owns its own instance.
    class HELLOKIT_VOICEBANK_EXPORT BuiltinFrequencyFormats {
    public:
        explicit BuiltinFrequencyFormats(FrequencyFormatRegistry &registry);
        ~BuiltinFrequencyFormats();

    private:
        std::vector<FrequencyFormatRegistry::AddFactory> m_registrations;

        Q_DISABLE_COPY_MOVE(BuiltinFrequencyFormats)
    };

}

#endif // HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
