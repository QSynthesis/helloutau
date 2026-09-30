#include "InterchangeWizardSupport.h"

#include <QtCore/QDir>
#include <QtWidgets/QApplication>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QStyle>

namespace hello::daw {

    QString InterchangeWizardSupport::textOf(const std::filesystem::path &path) {
        return QDir::toNativeSeparators(QString::fromStdU16String(path.u16string()));
    }

    std::filesystem::path InterchangeWizardSupport::pathOf(const QString &text) {
        return std::filesystem::path(QDir::fromNativeSeparators(text.trimmed()).toStdU16String());
    }

    QString InterchangeWizardSupport::filterOf(const QString &name, const QStringList &suffixes) {
        QStringList patterns;
        for (const auto &suffix : suffixes) {
            patterns.push_back(QStringLiteral("*.") + suffix);
        }
        return QStringLiteral("%1 (%2)").arg(name, patterns.join(QLatin1Char(' ')));
    }

    void InterchangeWizardSupport::showDiagnostics(QListWidget *list,
                                                   const kit::DiagnosticList &diagnostics) {
        list->clear();
        const auto style = QApplication::style();
        for (const auto &diagnostic : diagnostics) {
            auto text = diagnostic.message;
            if (diagnostic.noteIndex) {
                text = tr("Note %1 of the file: %2").arg(*diagnostic.noteIndex + 1).arg(text);
            }
            auto item = new QListWidgetItem(text, list);
            switch (diagnostic.severity) {
                case kit::DiagnosticSeverity::Error:
                    item->setIcon(style->standardIcon(QStyle::SP_MessageBoxCritical));
                    break;
                case kit::DiagnosticSeverity::Warning:
                    item->setIcon(style->standardIcon(QStyle::SP_MessageBoxWarning));
                    break;
                case kit::DiagnosticSeverity::Note:
                    item->setIcon(style->standardIcon(QStyle::SP_MessageBoxInformation));
                    break;
            }
        }
    }

}
