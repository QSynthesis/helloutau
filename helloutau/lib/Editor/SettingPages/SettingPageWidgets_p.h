#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H

#include <optional>

#include <QtCore/QString>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QWidget;

namespace hello::daw {

    /// Returns the path in \a edit with forward slashes, trimmed.
    QString pathText(const QLineEdit *edit);

    /// Adds a row to \a form with a line edit and a button that browses for a directory if
    /// \a directory is true, or for a program otherwise, and returns the line edit.
    QLineEdit *addPathRow(QFormLayout *form, QWidget *owner, const QString &label,
                          const QString &text, bool directory);

    /// Returns a label with wrapped \a text, for an explanation below a setting.
    QLabel *note(const QString &text);

    /// Returns an editable box of a number of threads, set to \a current. It offers Automatic,
    /// which stands for one thread per hardware thread and is stored as zero, the powers of two
    /// below the number of hardware threads, and that number. Any other count can be typed in,
    /// without an upper limit, as there is none for the jobs of ninja and make.
    QComboBox *createThreadCountBox(int current);

    /// Returns the number of threads in \a box of createThreadCountBox(), zero for Automatic, or
    /// \c std::nullopt if the text typed in is not a positive count.
    std::optional<int> threadCountOf(const QComboBox *box);

    /// Returns a box of the runs held in a log of the output of synth tools, the latest run only
    /// or every run, set to every run if \a accumulated.
    QComboBox *createLogModeBox(bool accumulated);

    /// Returns an editable box of the size limit of a log, set to \a bytes, with presets from
    /// 256 KiB to 16 MiB. A size is typed in as a number with B, KiB, MiB or GiB.
    QComboBox *createLogLimitBox(int bytes);

    /// Returns the size limit in bytes in \a box of createLogLimitBox(), or \c std::nullopt if
    /// the text typed in is not a whole number of bytes from 1024 bytes to 1 GiB.
    std::optional<int> logLimitOf(const QComboBox *box);

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H
