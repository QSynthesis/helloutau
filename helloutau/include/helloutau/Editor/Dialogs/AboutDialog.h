#ifndef HELLOUTAU_EDITOR_DIALOGS_ABOUTDIALOG_H
#define HELLOUTAU_EDITOR_DIALOGS_ABOUTDIALOG_H

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

namespace hello::daw {

    /// Shows the name, the license and the build information of the application: the version,
    /// the branch and commit, the build date, the toolchain and the version of Qt.
    class HELLOUTAU_EDITOR_EXPORT AboutDialog : public QDialog {
        Q_OBJECT
    public:
        explicit AboutDialog(QWidget *parent = nullptr);
        ~AboutDialog();
    };

}

#endif // HELLOUTAU_EDITOR_DIALOGS_ABOUTDIALOG_H
