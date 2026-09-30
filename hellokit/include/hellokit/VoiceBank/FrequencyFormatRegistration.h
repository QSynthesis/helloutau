#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATION_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATION_H

#include <memory>

#include <QtCore/QtGlobal>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Registers a format of frequency tables with every FrequencyFormatRegistry while the
    /// registration exists. See docs/Plugins.md.
    ///
    /// A plugin creates its registrations in initialize() and destroys them in
    /// aboutToShutdown(), before its library is unloaded. Registrations and registries are used
    /// on the thread of the application only.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormatRegistration {
    public:
        explicit FrequencyFormatRegistration(std::unique_ptr<FrequencyFormat> format);
        ~FrequencyFormatRegistration();

        FrequencyFormat *format() const;

    private:
        std::unique_ptr<FrequencyFormat> m_format;

        Q_DISABLE_COPY_MOVE(FrequencyFormatRegistration)
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATION_H
