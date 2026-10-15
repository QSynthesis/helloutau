#ifndef HELLOUTAU_EDITOR_FREQUENCYMAPVIEW_H
#define HELLOUTAU_EDITOR_FREQUENCYMAPVIEW_H

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include <QtGui/QColor>

#include <helloutau/Widgets/KeyboardModifierBindings.h>
#include <helloutau/Widgets/SceneView.h>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::kit {
    class FrequencyMapRevisionBuffer;
    class Spectrogram;
}

namespace hello::daw {

    /// The editing area of the dialog that edits frequency maps: the curve of a frequency map
    /// over the spectrogram of its audio file, with milliseconds across and note numbers up and
    /// down. See the editing in docs/FrequencyMaps.md.
    ///
    /// The drags follow the bindings of FrequencyViewKeyboardModifiers:
    /// - Draw and Unvoice write the frames under the pointer as one stroke of the buffer, which
    ///   Escape cancels.
    /// - SelectFrames selects the frames between the press and the pointer.
    /// - DragZoom zooms about the press.
    ///
    /// The wheel scrolls and zooms as the wheel scene of the bindings selects.
    class HELLOUTAU_EDITOR_EXPORT FrequencyMapView : public SceneView {
        Q_OBJECT
        Q_PROPERTY(QColor frequencyColor READ frequencyColor WRITE setFrequencyColor)
        Q_PROPERTY(QColor savedFrequencyColor READ savedFrequencyColor WRITE setSavedFrequencyColor)
        Q_PROPERTY(QColor spectrumColor READ spectrumColor WRITE setSpectrumColor)
        Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor)
    public:
        /// The lowest and the highest note number of the view, C1 and B8
        static constexpr int lowestNote = 24;
        static constexpr int highestNote = 107;

        explicit FrequencyMapView(QWidget *parent = nullptr);
        ~FrequencyMapView();

        /// Returns the buffer that the view shows and edits, or null.
        kit::FrequencyMapRevisionBuffer *buffer() const;

        /// Sets the buffer that the view shows and edits, or null for none, clears the selection
        /// and fits the view to the map. The caller owns the buffer and keeps it until another
        /// buffer is set. Requires no gesture in progress.
        void setBuffer(kit::FrequencyMapRevisionBuffer *buffer);

        /// Returns the spectrogram drawn behind the curve, or null if none is drawn.
        std::shared_ptr<const kit::Spectrogram> spectrogram() const;
        void setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram);

        /// Sets the bindings of FrequencyViewKeyboardModifiers that select the drags and the
        /// wheel.
        void setKeyboardModifierBindings(const KeyboardModifierBindings &bindings);

        /// Returns the selected frames as the first frame and the count, or \c std::nullopt if
        /// no frame is selected.
        std::optional<std::pair<std::size_t, std::size_t>> selection() const;

        /// Selects \a count frames from \a first, limited to the frames of the map.
        void setSelection(std::size_t first, std::size_t count);
        void selectAll();
        void clearSelection();

        /// Returns the frame nearest to the view coordinate \a x, or \c std::nullopt if the map
        /// has no frames.
        std::optional<std::size_t> frameAt(double x) const;

        /// Returns the frequency in hertz at the view coordinate \a y.
        double frequencyAt(double y) const;

        /// Returns the view coordinate y of \a frequency, which is above 0.
        double yOf(double frequency) const;

        /// Returns the view coordinate x of \a time in milliseconds.
        double xOf(double time) const;

        QColor frequencyColor() const;
        void setFrequencyColor(const QColor &color);

        /// Returns the color of the curve as last saved, drawn while the map is modified
        QColor savedFrequencyColor() const;
        void setSavedFrequencyColor(const QColor &color);

        QColor spectrumColor() const;
        void setSpectrumColor(const QColor &color);

        QColor selectionColor() const;
        void setSelectionColor(const QColor &color);

    Q_SIGNALS:
        /// Emitted when a drag added a step to the buffer.
        void edited();

        void selectionChanged();

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_FREQUENCYMAPVIEW_H
