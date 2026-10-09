#include "AboutDialog_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/BuildInfo.h>

namespace hello::daw {

    void showAboutHelloUtau(QWidget *parent) {
        QDialog dialog(parent);
        const auto appName = QApplication::applicationDisplayName();
        dialog.setWindowTitle(QCoreApplication::translate("AboutDialog", "About %1").arg(appName));
        dialog.setMinimumWidth(460);

        auto label = new QLabel(&dialog);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);
        label->setText(
            QCoreApplication::translate(
                "AboutDialog",
                "<h2>%1</h2>"
                "<p>A cross-platform editor for UTAU projects and voice banks.</p>"
                "<p>Licensed under the Apache License, Version 2.0. "
                "<a href=\"https://www.apache.org/licenses/LICENSE-2.0\">License</a>.</p>"
                "<h3>Build Information</h3>"
                "<p>Version: %2<br>"
                "Branch: %3<br>"
                "Commit: %4<br>"
                "Build date: %5<br>"
                "Toolchain: %6 %7 %8<br>"
                "Built with Qt %9.</p>")
                .arg(appName.toHtmlEscaped(), QApplication::applicationVersion(),
                     QStringLiteral(HELLOUTAU_GIT_BRANCH),
                     QStringLiteral(HELLOUTAU_GIT_LAST_COMMIT_HASH),
                     QStringLiteral(HELLOUTAU_BUILD_TIME), QStringLiteral(HELLOUTAU_COMPILER_ARCH),
                     QStringLiteral(HELLOUTAU_COMPILER_ID),
                     QStringLiteral(HELLOUTAU_COMPILER_VERSION), QStringLiteral(QT_VERSION_STR)));

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

        auto layout = new QVBoxLayout(&dialog);
        layout->addWidget(label);
        layout->addWidget(buttons);
        dialog.exec();
    }

}
