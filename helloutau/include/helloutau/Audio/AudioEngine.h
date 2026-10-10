#ifndef HELLOUTAU_AUDIO_AUDIOENGINE_H
#define HELLOUTAU_AUDIO_AUDIOENGINE_H

#include <memory>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtMultimedia/QAudioDevice>

#include <helloutau/Audio/AudioMixer.h>
#include <helloutau/Audio/DeviceClock.h>

namespace hello::daw {

    /// Owns the process-wide output stream. Control methods run on the application thread.
    /// Sources are registered at the selected device rate. Device changes invalidate sources.
    ///
    /// The process never opens more than one stream, because one channel of a device has been
    /// observed to go silent on Windows while two streams play on it (see docs/Audio.md). A source
    /// for a device other than that of the open stream reopens the stream on that device only while
    /// no source plays.
    ///
    /// The stream is closed when the last source stops, so that a source is normally the first
    /// one of its stream, as DeviceClock requires. The next source opens the stream again.
    class HELLOUTAU_AUDIO_EXPORT AudioEngine : public QObject {
        Q_OBJECT
    public:
        static AudioEngine *instance();
        ~AudioEngine();

        QByteArray deviceId() const;

        /// Returns the selected device, or the default device if none is selected or the
        /// selected device is absent.
        QAudioDevice device() const;

        /// Returns the device with \a id, or the default device if \a id is empty or absent.
        static QAudioDevice device(const QByteArray &id);

        /// Returns the IDs of the output devices.
        static QList<QByteArray> deviceIds();

        /// Returns the description of the output device with \a id, or an empty string if the
        /// device is absent.
        static QString deviceDescription(const QByteArray &id);

        /// Returns the sample rate of the open device stream if it is on the selected device, or
        /// else the preferred sample rate of the selected device. Returns 0 if no output device
        /// exists.
        int sampleRate() const;

        /// Returns sampleRate() for the device with \a id, see device(const QByteArray &).
        int sampleRate(const QByteArray &id) const;

        /// Changes the device after stopping all registered sources. Empty selects the default.
        void setDeviceId(const QByteArray &id);

        /// Plays \a source, whose samples are at \a sampleRate, on the selected device.
        std::optional<AudioMixer::SourceId> start(std::shared_ptr<AudioSource> source,
                                                  int sampleRate, QString *error = nullptr);

        /// Plays \a source, whose samples are at \a sampleRate, on the device with \a id. If the
        /// stream is open on another device, it is reopened on this device if no source plays,
        /// and the source is rejected otherwise.
        std::optional<AudioMixer::SourceId> start(std::shared_ptr<AudioSource> source,
                                                  const QByteArray &id, int sampleRate,
                                                  QString *error = nullptr);
        void stop(AudioMixer::SourceId id);
        bool isFinished(AudioMixer::SourceId id) const;
        std::shared_ptr<DeviceClock> clock(AudioMixer::SourceId id) const;
        int bufferedMilliseconds() const;

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

#endif // HELLOUTAU_AUDIO_AUDIOENGINE_H
