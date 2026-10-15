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

class QToolButton;
class QSlider;

namespace hello::kit {
    class FrequencyMapRevisionBuffer;
    class Spectrogram;
}

namespace hello::daw {

    /// The editing area of the dialog that edits frequency maps, after the frequency table
    /// control of frqeditor. See the editing in docs/FrequencyMaps.md.
    ///
    /// The frames run across and the note numbers up and down, with a column of labels at the
    /// left. The view shows either rows of the keys with the amplitudes of the frames, or the
    /// spectrogram of the audio file in one of six color schemes and seven levels of brightness.
    /// Over either it shows a line every 17 frames, the key and the frames of the map as last
    /// saved, and the key and the frames of the map, the selected frames in their own color. The
    /// button \c s in the corner switches the view, and opens the color schemes on a right click.
    /// The button \c * beside it shows the brightness slider.
    ///
    /// The left drags depend on tool(). The drags follow the bindings of
    /// FrequencyViewKeyboardModifiers:
    /// - Draw and Unvoice write the frames under the pointer as one stroke of the buffer, which
    ///   Escape cancels.
    /// - SelectFrames selects the frames between the press and the pointer, which a band over the
    ///   whole height marks while the drag lasts.
    /// - DragZoom zooms about the press.
    /// - DragKey sets the key, if the map records one, from the key line in the labels.
    ///
    /// A double click in the labels sets every frame and the key to the frequency there. The
    /// wheel scrolls and zooms as the wheel scene of the bindings selects. The view keeps its
    /// zoom, its view and its spectrogram settings when another buffer is set.
    class HELLOUTAU_EDITOR_EXPORT FrequencyMapView : public SceneView {
        Q_OBJECT
        Q_PROPERTY(QColor whiteRowColor READ whiteRowColor WRITE setWhiteRowColor)
        Q_PROPERTY(QColor blackRowColor READ blackRowColor WRITE setBlackRowColor)
        Q_PROPERTY(QColor rowLineColor READ rowLineColor WRITE setRowLineColor)
        Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)
        Q_PROPERTY(QColor frameLineColor READ frameLineColor WRITE setFrameLineColor)
        Q_PROPERTY(QColor keyLineColor READ keyLineColor WRITE setKeyLineColor)
        Q_PROPERTY(QColor curveColor READ curveColor WRITE setCurveColor)
        Q_PROPERTY(QColor selectedCurveColor READ selectedCurveColor WRITE setSelectedCurveColor)
        Q_PROPERTY(QColor savedColor READ savedColor WRITE setSavedColor)
        Q_PROPERTY(QColor amplitudeColor READ amplitudeColor WRITE setAmplitudeColor)
        Q_PROPERTY(QColor bandColor READ bandColor WRITE setBandColor)
    public:
        /// The views of the editing area
        enum ViewMode {
            KeyRowsView,  ///< The rows of the keys with the amplitudes, the default
            SpectrumView, ///< The spectrogram
        };

        /// The effect of a left drag in the plot, as the tools of the piano roll
        enum Tool {
            SelectTool, ///< Selects the frames in a span of time.
            PenTool,    ///< Draws the frequencies as frqeditor does, and is the default.
        };

        /// The lowest note number of the view, C1
        static constexpr int lowestNote = 24;

        /// The number of zoom levels, as in frqeditor
        static constexpr int zoomLevelCount = 7;

        /// The pixels of a frame and of a semitone at each zoom level, as in frqeditor
        static constexpr int framePixels[zoomLevelCount] = {2, 4, 7, 10, 15, 20, 40};
        static constexpr int semitonePixels[zoomLevelCount] = {3, 5, 7, 10, 15, 20, 40};

        /// The number of color schemes of the spectrogram
        static constexpr int colorSchemeCount = 6;

        /// The lowest and the highest brightness of the spectrogram, the lowest the brightest
        static constexpr int lowestBrightness = -3;
        static constexpr int highestBrightness = 3;

        explicit FrequencyMapView(QWidget *parent = nullptr);
        ~FrequencyMapView();

        /// Returns the buffer that the view shows and edits, or null.
        kit::FrequencyMapRevisionBuffer *buffer() const;

        /// Sets the buffer that the view shows and edits, or null for none, clears the selection
        /// and scrolls to the first frame. The caller owns the buffer and keeps it until another
        /// buffer is set. Requires no gesture in progress.
        void setBuffer(kit::FrequencyMapRevisionBuffer *buffer);

        /// Returns the spectrogram that SpectrumView shows, or null.
        std::shared_ptr<const kit::Spectrogram> spectrogram() const;
        void setSpectrogram(std::shared_ptr<const kit::Spectrogram> spectrogram);

        /// Returns the name drawn at the top left, the name of the audio file.
        QString title() const;
        void setTitle(const QString &title);

        ViewMode viewMode() const;
        void setViewMode(ViewMode mode);

        Tool tool() const;

        /// Sets the tool of the left presses after the call.
        void setTool(Tool tool);

        /// Returns the brightness of the spectrogram from lowestBrightness to highestBrightness,
        /// 0 by default.
        int brightness() const;
        void setBrightness(int brightness);

        /// Returns the color scheme of the spectrogram from 0 to colorSchemeCount - 1, in the
        /// order of frqeditor: cool, purple, jade, grassy, flame and ruby.
        int colorScheme() const;
        void setColorScheme(int scheme);

        /// Returns the zoom level whose pixels of a frame are nearest to those of the view.
        int zoomLevel() const;

        /// Sets both axes to the pixels of \a level, limited to the levels, keeping the first
        /// frame and the lowest note of the view in place. The view starts at level 0 with C1 at
        /// the bottom.
        void setZoomLevel(int level);

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

        /// Returns the width of the column of labels at the left.
        int labelWidth() const;

        /// Returns the button that switches the view.
        QToolButton *viewButton() const;

        /// Returns the button that shows the brightness slider.
        QToolButton *brightnessButton() const;

        QSlider *brightnessSlider() const;

        QColor whiteRowColor() const;
        void setWhiteRowColor(const QColor &color);
        QColor blackRowColor() const;
        void setBlackRowColor(const QColor &color);
        QColor rowLineColor() const;
        void setRowLineColor(const QColor &color);
        QColor labelColor() const;
        void setLabelColor(const QColor &color);
        QColor frameLineColor() const;
        void setFrameLineColor(const QColor &color);
        QColor keyLineColor() const;
        void setKeyLineColor(const QColor &color);
        QColor curveColor() const;
        void setCurveColor(const QColor &color);
        QColor selectedCurveColor() const;
        void setSelectedCurveColor(const QColor &color);
        /// Returns the color of the key and the frames of the map as last saved.
        QColor savedColor() const;
        void setSavedColor(const QColor &color);
        QColor amplitudeColor() const;
        void setAmplitudeColor(const QColor &color);
        /// Returns the color of the band of a drag with the select tool, filled translucent.
        QColor bandColor() const;
        void setBandColor(const QColor &color);

    Q_SIGNALS:
        /// Emitted when a drag or a double click added a step to the buffer.
        void edited();

        void selectionChanged();

        /// Emitted when the view, the brightness or the color scheme changed through the
        /// controls of the view or the setters.
        void viewModeChanged();
        void spectrumChanged();

    protected:
        void resizeEvent(QResizeEvent *event) override;
        /// Returns a black cross with a white outline for Qt::CrossCursor, the cursor of the pen,
        /// which stays visible over the rows and the spectrogram.
        QCursor cursorOf(Qt::CursorShape shape) const override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_FREQUENCYMAPVIEW_H
