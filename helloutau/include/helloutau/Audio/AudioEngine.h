#ifndef HELLOUTAU_AUDIO_AUDIOENGINE_H
#define HELLOUTAU_AUDIO_AUDIOENGINE_H

#include <QtMultimedia/QAudioDevice>
#include <helloutau/Audio/AudioMixer.h>

namespace hello::daw {

    /// Owns the process-wide output stream. Control methods run on the application thread.
    /// Sources are registered at the selected device rate. Device changes invalidate sources.
    class HELLOUTAU_AUDIO_EXPORT AudioEngine : public QObject {
        Q_OBJECT
    public:
        static AudioEngine *instance();
        ~AudioEngine();

        QByteArray deviceId() const;
        QAudioDevice device() const;
        int sampleRate() const;
        /// Changes the device after stopping all registered sources. Empty selects the default.
        void setDeviceId(const QByteArray &id);
        std::optional<AudioMixer::SourceId> start(std::shared_ptr<AudioSource> source,
                                                  int sampleRate, QString *error = nullptr);
        void stop(AudioMixer::SourceId id);
        bool isFinished(AudioMixer::SourceId id) const;
        std::shared_ptr<DeviceClock> clock(AudioMixer::SourceId id) const;
        int bufferedMilliseconds() const;
        /// Returns the number of successful device stream openings, for diagnostics.
        quint64 streamGeneration() const;

    Q_SIGNALS:
        void invalidated(const QString &reason);
        void devicesChanged();

    private:
        explicit AudioEngine(QObject *parent);
        void close();
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif
