#ifndef HELLOUTAU_AUDIO_AUDIOOUTPUT_H
#define HELLOUTAU_AUDIO_AUDIOOUTPUT_H

#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <helloutau/Audio/HelloUtauAudioGlobal.h>

namespace hello::daw {

    class AudioSource;

    /// Controls one source in the shared AudioEngine. Other outputs continue playing when this
    /// handle stops or is destroyed. Control methods run on the application thread.
    class HELLOUTAU_AUDIO_EXPORT AudioOutput : public QObject {
        Q_OBJECT
    public:
        explicit AudioOutput(QObject *parent = nullptr);
        ~AudioOutput() override;

        /// Replaces this handle's source, whose samples are at \a sampleRate. The source is
        /// rejected if \a sampleRate differs from the rate of the device stream, as for samples
        /// converted before the device or its rate changed.
        ///
        /// \return whether the device started, with the reason in \a error otherwise
        bool start(std::shared_ptr<AudioSource> source, int sampleRate, QString *error = nullptr);

        /// Replaces this handle's source with \a source on the device with \a id instead of the
        /// selected device, see AudioEngine::start(). The source is rejected while a source plays
        /// on another device.
        ///
        /// \return whether the device started, with the reason in \a error otherwise
        bool start(std::shared_ptr<AudioSource> source, const QByteArray &id, int sampleRate,
                   QString *error = nullptr);

        /// Stops playing. Emits finished() if something played.
        void stop();

        bool isPlaying() const;

        /// Returns the AudioSource::position() of the source that the device plays now, see
        /// DeviceClock, or \c std::nullopt before the device has pulled from the source.
        std::optional<double> heardPosition() const;

    Q_SIGNALS:
        /// Playing ended, at the end of the source or on stop().
        void finished();
        /// Reports device loss or an output error after stopping this source.
        void failed(const QString &reason);

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_AUDIO_AUDIOOUTPUT_H
