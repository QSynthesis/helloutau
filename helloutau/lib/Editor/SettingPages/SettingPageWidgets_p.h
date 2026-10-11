#ifndef HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H
#define HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H

#include <QtCore/QString>

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

}

#endif // HELLOUTAU_EDITOR_SETTINGPAGES_SETTINGPAGEWIDGETS_P_H
