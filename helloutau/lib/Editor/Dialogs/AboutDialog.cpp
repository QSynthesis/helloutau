#include "AboutDialog.h"

#include <QtCore/QString>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

#include <hellokit/BuildInfo.h>
#include <hellokit/Config.h>

#include "SoftwareBuildChannel.h"

namespace hello::daw {

    AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
        const auto appName = QApplication::applicationDisplayName();
        setWindowTitle(tr("About %1").arg(appName));
        setMinimumWidth(460);

        // The version, followed by the name of a build that is no release
        auto version = QApplication::applicationVersion();
        if (!SoftwareBuildChannel::isRelease()) {
            version = QStringLiteral("%1 (%2)").arg(version, SoftwareBuildChannel::buildName());
        }

        auto label = new QLabel(this);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setOpenExternalLinks(true);
        label->setText(
            tr("<h2>%1</h2>"
               "<p>A cross-platform editor for UTAU projects and voice banks.</p>"
               "<p>Licensed under the Apache License, Version 2.0. "
               "<a href=\"https://www.apache.org/licenses/LICENSE-2.0\">License</a>.</p>"
               "<h3>Build Information</h3>"
               "<p>Version: %2<br>"
               "Branch: %3<br>"
               "Commit: %4<br>"
               "Build date: %5<br>"
               "Toolchain: %6 %7 %8<br>"
               "Target platform: %10<br>"
               "Built with Qt %9.</p>")
                .arg(appName.toHtmlEscaped(), version, QStringLiteral(HELLOKIT_GIT_BRANCH),
                     QStringLiteral(HELLOKIT_GIT_LAST_COMMIT_HASH),
                     QStringLiteral(HELLOKIT_BUILD_TIME), QStringLiteral(HELLOKIT_COMPILER_ARCH),
                     QStringLiteral(HELLOKIT_COMPILER_ID),
                     QStringLiteral(HELLOKIT_COMPILER_VERSION), QStringLiteral(QT_VERSION_STR),
                     QStringLiteral(HELLOKIT_TRIPLET)));

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(label);
        layout->addWidget(buttons);
    }

    AboutDialog::~AboutDialog() = default;

}
