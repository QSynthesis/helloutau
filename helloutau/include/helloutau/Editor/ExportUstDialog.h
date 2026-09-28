#ifndef HELLOUTAU_EDITOR_EXPORTUSTDIALOG_H
#define HELLOUTAU_EDITOR_EXPORTUSTDIALOG_H

#include <filesystem>

#include <QtWidgets/QDialog>

#include <helloutau/Editor/HelloUtauEditorGlobal.h>

class QComboBox;
class QLineEdit;

namespace hello::daw {

    /// Asks where to export a UST and in which encoding.
    class HELLOUTAU_EDITOR_EXPORT ExportUstDialog : public QDialog {
        Q_OBJECT
    public:
        /// \param path the file initially proposed
        /// \param charset the encoding initially selected, the one of AppSettings
        ExportUstDialog(const std::filesystem::path &path, const QString &charset,
                        QWidget *parent = nullptr);
        ~ExportUstDialog();

        std::filesystem::path path() const;
        QString charset() const;

        /// The encodings offered: UTF-8, followed by \c TextCodec::ustCandidates().
        static QStringList charsets();

    private:
        QLineEdit *m_path;
        QComboBox *m_charset;

        void browse();
    };

}

#endif // HELLOUTAU_EDITOR_EXPORTUSTDIALOG_H
