#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATPLUGIN_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATPLUGIN_H

#include <memory>
#include <vector>

#include <QtCore/QtPlugin>

#include <hellokit/VoiceBank/FrequencyFormat.h>
#include <hellokit/VoiceBank/HelloKitVoiceBankGlobal.h>

namespace hello::kit {

    /// Provides formats of frequency tables to the application, the sixth kind of plugin of
    /// docs/note.md, registered in a FrequencyFormatRegistry beside the built-in formats.
    ///
    /// \note Ownership of the formats is transferred. A plugin must not retain pointers to the
    ///       returned formats, because the registry may destroy them at any time.
    class HELLOKIT_VOICEBANK_EXPORT FrequencyFormatPlugin {
    public:
        virtual ~FrequencyFormatPlugin();

        virtual std::vector<std::unique_ptr<FrequencyFormat>> createFormats() = 0;
    };

}

Q_DECLARE_INTERFACE(hello::kit::FrequencyFormatPlugin,
                    "org.qsynthesis.HelloUtau.FrequencyFormatPlugin/1.0")

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATPLUGIN_H
