#ifndef HELLOUTAU_EDITOR_DIALOGS_EDITFREQUENCYMAPSDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_EDITFREQUENCYMAPSDIALOG_H

#include <filesystem>
#include <memory>

#include <QtCore/QList>
#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeWidget;

namespace QAK {
    class WidgetActionContext;
}

namespace hello::kit {
    class FrequencyFormat;
    class FrequencyMapRevisionBuffer;
}

namespace hello::daw {

    class Editor;
    class FrequencyMapView;
    class SamplePreview;

    /// The dialog that edits the frequency maps of the audio files of a voice bank in one format.
    /// See the editing in docs/FrequencyMaps.md.
    ///
    /// The files are listed with the presence of their maps and of each analysis cache of the
    /// format, and whether their maps are modified. The map of the current file is read when the
    /// file becomes current, and is edited in the editing area. A file without a map, or with a
    /// map that cannot be read, cannot be edited. The audio, its spectrogram and the map are read
    /// in the background with a progress dialog. A cancellation keeps the previous file current
    /// and is reported.
    ///
    /// Each map has a history of its own, which ends with the dialog. Save writes the current
    /// map, and Save All writes every modified map in order and stops at the first failure. A
    /// save removes the analysis caches of the map if removeCachesBox() is checked, initially
    /// the setting FrequencyMapSettings::removesCachesOnSave(), and asks before it overwrites a
    /// map file that changed on disk after it was read. Closing the dialog with a modified map
    /// asks whether to save every modified map.
    ///
    /// Beside the file list the dialog has the right side of the window of frqeditor: the key
    /// with its button Update, the list of the frequencies of the frames, which selects the
    /// frames of the editing area, the frequency of the current frame with its button Set, the
    /// preview, the information of the audio file, the editing area and a row of buttons for the
    /// selected frames. The preview synthesizes the audio file at the key with the map as edited
    /// and the resampler of the settings, or plays the audio file as it is with \c $direct.
    ///
    /// The menus and the shortcuts are those of Editor::FrequencyMapEditorKind. The actions of
    /// contributions are added when the dialog is created.
    class HELLOUTAU_EDITOR_EXPORT EditFrequencyMapsDialog : public QDialog {
        Q_OBJECT
    public:
        /// An audio file of the voice bank.
        struct File {
            /// The absolute path
            std::filesystem::path path;
            /// Whether the file has a map in the format
            bool hasMap = false;
            /// Whether the file has each analysis cache of the format, in their order
            QList<bool> caches;
            /// Whether the file is selected in the window, which makes the first such file
            /// current
            bool selected = false;
        };

        /// Creates the dialog of \a editor for \a files of the voice bank at \a root, with the
        /// maps in \a format, which can write maps. The paths of the files are shown relative to
        /// \a root. The first selected file is current, or else the first file with a map.
        EditFrequencyMapsDialog(Editor *editor, const kit::FrequencyFormat &format,
                                QList<File> files, const std::filesystem::path &root,
                                QWidget *parent = nullptr);
        ~EditFrequencyMapsDialog();

        /// Returns the index of the current file, or -1 before the first file is read once the
        /// dialog is shown.
        int currentFile() const;

        /// Makes the file \a index current after reading it in the background. Returns false if
        /// the user cancels the reading, which keeps the current file.
        bool setCurrentFile(int index);

        /// Sets the milliseconds after which the progress dialog of a reading appears, 400 by
        /// default. With 0 the progress dialog appears at once.
        void setProgressDelay(int milliseconds);

        /// Returns the buffer of the map of the file \a index, or null if the map was not read,
        /// could not be read, or was released after it was left without a history.
        kit::FrequencyMapRevisionBuffer *bufferOf(int index) const;

        /// Returns whether the map of the file \a index is modified.
        bool isModified(int index) const;

        /// Writes the map of the current file. Returns false if it is not modified, the user
        /// keeps a map file that changed on disk, or the write fails.
        bool save();

        /// Writes every modified map in order, and stops at the first that save() does not
        /// write. Returns whether every modified map was written.
        bool saveAll();

        QTreeWidget *fileList() const;
        FrequencyMapView *view() const;
        /// Returns the edit of the key, which sets the key when its editing finishes
        QLineEdit *keyEdit() const;
        /// Returns the list of the frequencies of the frames, one row for each frame
        QListWidget *frameList() const;
        /// Returns the edit of the frequency of the current frame, which Set writes
        QLineEdit *frequencyEdit() const;
        QPushButton *setButton() const;
        QPushButton *playButton() const;
        /// Returns the box that plays the audio file as it is, as \c $direct=True of UTAU does
        QCheckBox *directBox() const;
        /// Returns the preview that the button Play starts.
        SamplePreview *preview() const;
        /// Returns the box that removes the analysis caches on save, for this dialog only
        QCheckBox *removeCachesBox() const;
        /// Returns the label that explains why the current file cannot be edited, hidden if it
        /// can
        QLabel *messageLabel() const;
        QAK::WidgetActionContext *actionContext() const;

    public Q_SLOTS:
        /// Closes the dialog after asking whether to save every modified map, if any.
        void reject() override;

    protected:
        void showEvent(QShowEvent *event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_EDITFREQUENCYMAPSDIALOG_H
