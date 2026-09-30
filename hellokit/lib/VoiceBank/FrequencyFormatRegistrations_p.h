#ifndef HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATIONS_P_H
#define HELLOKIT_VOICEBANK_FREQUENCYFORMATREGISTRATIONS_P_H

#include <QtCore/QList>

#include <hellokit/VoiceBank/FrequencyFormat.h>

namespace hello::kit {

    /// The process-wide list of registered formats in the order of registration, and the
    /// listeners notified of changes.
    ///
    /// A stdc::DynamicRegistry is not used because it orders entries by name, and the
    /// precedence of formats for a resampler depends on the order of registration.
    class FrequencyFormatRegistrations {
    public:
        /// Receives a notification for each format registered or unregistered after the
        /// listener was added.
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
