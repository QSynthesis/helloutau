#ifndef HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H
#define HELLOKIT_VOICEBANK_BUILTINFREQUENCYFORMATS_H

#include <vector>

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/FrequencyFormats.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Registrations of the formats of this library in \a registry for the lifetime of the
    /// object: frq of resampler.exe, the format of the resampler of UTAU. The other formats are
    /// registered by plugins, such as dio and mrq by the SynthToolSupport plugin.
    ///
    /// The editor owns an instance for its registry, so that frq is registered without any
    /// plugin. A test that requires the format owns its own instance.
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
