#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATIONS_P_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATIONS_P_H

#include <QtCore/QList>

#include <hellokit/VoiceBank/FrequencyFormat.h>

namespace hello::kit {

    /// The formats registered in the process, in the order of registration, and the registries
    /// that follow them.
    ///
    /// Kept here rather than in a stdc::DynamicRegistry, whose entries are ordered by name,
    /// since the later of two formats for a resampler takes precedence.
    class FrequencyFormatRegistrations {
    public:
        /// Told of each format registered or unregistered after it was added.
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void formatAdded(FrequencyFormat *format) = 0;
            virtual void formatRemoved(FrequencyFormat *format) = 0;
        };

        static FrequencyFormatRegistrations &instance();

        QList<FrequencyFormat *> formats() const;

        void add(FrequencyFormat *format);
        void remove(FrequencyFormat *format);

        void addListener(Listener *listener);
        void removeListener(Listener *listener);

    private:
        QList<FrequencyFormat *> m_formats;
        QList<Listener *> m_listeners;
    };

}

#endif // HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATIONS_P_H
