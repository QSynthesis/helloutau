#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

#include <helloutau/Widgets/SettingPage.h>
#include <helloutau/Widgets/SettingsDialog.h>

using namespace hello::daw;

namespace {

    // A page that edits a text, applied to value, and rejects an empty text
    class TextPage : public SettingPage {
    public:
        TextPage(const QString &id, const QString &title, const QString &label)
            : SettingPage(id), m_label(label) {
            setTitle(title);
        }

        QString value = QStringLiteral("start");
        QPointer<QLineEdit> edit;
        int settingsAppliedCalls = 0;

        void settingsApplied() override {
            ++settingsAppliedCalls;
        }

        bool isModified() const override {
            return edit && edit->text() != value;
        }

        bool apply(QString *error) override {
            if (edit->text().isEmpty()) {
                *error = QStringLiteral("The text is empty.");
                return false;
            }
            value = edit->text();
            Q_EMIT modifiedChanged();
            return true;
        }

    protected:
        QWidget *createWidget() override {
            auto widget = new QWidget();
            auto form = new QFormLayout(widget);
            edit = new QLineEdit(value);
            form->addRow(m_label, edit);
            form->addRow(new QCheckBox(QStringLiteral("Enable clipping &check")));
            connect(edit, &QLineEdit::textChanged, this, &SettingPage::modifiedChanged);
            return widget;
        }

    private:
        QString m_label;
    };

    // A page of many rows above a tree, higher than the dialog
    class TallPage : public SettingPage {
    public:
        TallPage() : SettingPage(QStringLiteral("tall")) {
            setTitle(QStringLiteral("Tall"));
        }

    protected:
        QWidget *createWidget() override {
            auto widget = new QWidget();
            auto layout = new QVBoxLayout(widget);
            for (int i = 0; i < 30; ++i) {
                layout->addWidget(new QLineEdit());
            }
            layout->addWidget(new QTreeWidget());
            return widget;
        }
    };

    // A page with a high part that can be hidden above a line edit
    class FoldingPage : public SettingPage {
    public:
        FoldingPage() : SettingPage(QStringLiteral("folding")) {
            setTitle(QStringLiteral("Folding"));
        }

        QPointer<QWidget> high;

    protected:
        QWidget *createWidget() override {
            auto widget = new QWidget();
            auto layout = new QVBoxLayout(widget);
            high = new QWidget();
            high->setMinimumHeight(900);
            layout->addWidget(high);
            layout->addWidget(new QLineEdit());
            return widget;
        }
    };

    // Audio, a category of Output and Export, and Appearance
    struct Pages {
        SettingCatalog catalog;
        SettingPage *audio = new SettingPage(QStringLiteral("audio"));
        TextPage *output = new TextPage(QStringLiteral("audio.output"), QStringLiteral("Output"),
                                        QStringLiteral("Buffer size:"));
        TextPage *exporting = new TextPage(QStringLiteral("audio.export"), QStringLiteral("Export"),
                                           QStringLiteral("Format:"));
        TextPage *appearance = new TextPage(QStringLiteral("appearance"),
                                            QStringLiteral("Appearance"), QStringLiteral("Theme:"));

        Pages() {
            audio->setTitle(QStringLiteral("Audio"));
            audio->setDescription(QStringLiteral("Devices and files"));
            audio->addPage(output);
            audio->addPage(exporting);
            appearance->setKeywords({QStringLiteral("colors")});
            catalog.addPage(audio);
            catalog.addPage(appearance);
        }
    };

    QStringList titlesOf(const QList<SettingPage *> &pages) {
        QStringList titles;
        for (const auto page : pages) {
            titles.push_back(page->title());
        }
        return titles;
    }

}

class test_SettingsDialog : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The tree of pages, a category showing links to its pages, and the path of a page as its
    // title
    void the_pages_form_a_tree() {
        Pages pages;
        QCOMPARE(pages.catalog.page(QStringLiteral("audio.export")), pages.exporting);
        QCOMPARE(pages.exporting->parentPage(), pages.audio);
        QCOMPARE(titlesOf(pages.catalog.allPages()),
                 (QStringList{QStringLiteral("Audio"), QStringLiteral("Output"),
                              QStringLiteral("Export"), QStringLiteral("Appearance")}));

        SettingsDialog dialog(&pages.catalog);
        QCOMPARE(dialog.currentPage(), pages.audio);
        const auto links = dialog.findChildren<QPushButton *>();
        QPushButton *exportLink = nullptr;
        for (const auto link : links) {
            if (link->text() == QStringLiteral("Export")) {
                exportLink = link;
            }
        }
        QVERIFY(exportLink);
        exportLink->click();
        QCOMPARE(dialog.currentPage(), pages.exporting);
        bool path = false;
        for (const auto label : dialog.findChildren<QLabel *>()) {
            path = path || label->text() == QStringLiteral("Audio > Export");
        }
        QVERIFY(path);

        QVERIFY(dialog.selectPage(QStringLiteral("appearance")));
        QCOMPARE(dialog.currentPage(), pages.appearance);
        QVERIFY(!dialog.selectPage(QStringLiteral("none")));
    }

    // A page added later takes its place before a given page of the same level, or comes last
    // if that page is not at that level.
    void a_page_is_added_before_another() {
        Pages pages;
        pages.catalog.addPage(new SettingPage(QStringLiteral("plugins")),
                              QStringLiteral("appearance"));
        pages.catalog.addPage(new SettingPage(QStringLiteral("tools")),
                              QStringLiteral("audio.output"));
        pages.audio->addPage(new SettingPage(QStringLiteral("audio.input")),
                             QStringLiteral("audio.export"));
        pages.audio->addPage(new SettingPage(QStringLiteral("audio.midi")),
                             QStringLiteral("appearance"));
        const auto idsOf = [](const QList<SettingPage *> &list) {
            QStringList ids;
            for (const auto page : list) {
                ids.push_back(page->id());
            }
            return ids;
        };
        QCOMPARE(idsOf(pages.catalog.pages()),
                 (QStringList{"audio", "plugins", "appearance", "tools"}));
        QCOMPARE(idsOf(pages.audio->pages()),
                 (QStringList{"audio.output", "audio.input", "audio.export", "audio.midi"}));
        QCOMPARE(pages.catalog.page(QStringLiteral("audio.input"))->parentPage(), pages.audio);
    }

    // The search finds pages by title, description, keywords and the text of their controls,
    // keeps their parents, marks the controls that match, and reports when no page matches.
    void the_search_finds_settings_within_pages() {
        Pages pages;
        SettingsDialog dialog(&pages.catalog);
        dialog.selectPage(QStringLiteral("appearance"));

        dialog.searchBox()->setText(QStringLiteral("buffer"));
        QCOMPARE(titlesOf(dialog.visiblePages()),
                 (QStringList{QStringLiteral("Audio"), QStringLiteral("Output")}));
        QCOMPARE(dialog.currentPage(), pages.output);
        QCOMPARE(dialog.highlighted().size(), 1);
        QCOMPARE(qobject_cast<QLabel *>(dialog.highlighted().first())->text(),
                 QStringLiteral("Buffer size:"));

        // A mnemonic does not hide a word, and a match in every page keeps the current one.
        dialog.searchBox()->setText(QStringLiteral("clipping check"));
        QCOMPARE(dialog.visiblePages().size(), 4);
        QCOMPARE(dialog.currentPage(), pages.output);
        QCOMPARE(dialog.highlighted().size(), 1);

        dialog.searchBox()->setText(QStringLiteral("COLORS"));
        QCOMPARE(titlesOf(dialog.visiblePages()), QStringList{QStringLiteral("Appearance")});
        QCOMPARE(dialog.currentPage(), pages.appearance);
        QVERIFY(dialog.highlighted().isEmpty());

        dialog.searchBox()->setText(QStringLiteral("devices and"));
        QCOMPARE(titlesOf(dialog.visiblePages()), QStringList{QStringLiteral("Audio")});

        dialog.searchBox()->setText(QStringLiteral("nothing like it"));
        QVERIFY(dialog.visiblePages().isEmpty());
        QCOMPARE(dialog.messageLabel()->text(), QStringLiteral("No matching settings"));
        QVERIFY(!dialog.messageLabel()->isHidden());

        dialog.searchBox()->clear();
        QCOMPARE(dialog.visiblePages().size(), 4);
        QVERIFY(dialog.highlighted().isEmpty());
        QVERIFY(dialog.messageLabel()->isHidden());
    }

    // Apply is enabled while a page is modified, which is bold. Apply applies the modified pages.
    // A page that fails to apply shows its reason until it changes, and keeps OK from closing.
    void modified_pages_are_applied() {
        Pages pages;
        SettingsDialog dialog(&pages.catalog);
        QSignalSpy applied(&dialog, &SettingsDialog::applied);
        QVERIFY(!dialog.applyButton()->isEnabled());

        dialog.selectPage(QStringLiteral("audio.output"));
        pages.output->edit->setText(QStringLiteral("512"));
        QVERIFY(dialog.applyButton()->isEnabled());
        const auto item = dialog.tree()->currentItem();
        QVERIFY(item->font(0).bold());
        dialog.applyButton()->click();
        QCOMPARE(pages.output->value, QStringLiteral("512"));
        QVERIFY(!dialog.applyButton()->isEnabled());
        QVERIFY(!item->font(0).bold());
        QCOMPARE(applied.size(), 1);

        pages.output->edit->clear();
        dialog.selectPage(QStringLiteral("appearance"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(dialog.currentPage(), pages.output);
        QCOMPARE(dialog.messageLabel()->text(), QStringLiteral("The text is empty."));
        QVERIFY(!dialog.messageLabel()->isHidden());
        QCOMPARE(applied.size(), 1);

        // A change of the page hides the reason, which may no longer hold.
        pages.output->edit->setText(QStringLiteral("1024"));
        QVERIFY(dialog.messageLabel()->isHidden());
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(pages.output->value, QStringLiteral("1024"));
        QVERIFY(!pages.output->hasWidget());
    }

    // A page higher than the dialog keeps its preferred height in a scroll area, so that the
    // dialog can be lower than the page.
    void a_high_page_scrolls_in_a_lower_dialog() {
        SettingCatalog catalog;
        const auto page = new TallPage();
        catalog.addPage(page);
        SettingsDialog dialog(&catalog);
        dialog.selectPage(QStringLiteral("tall"));
        const auto widget = page->widget();
        const int preferred = widget->sizeHint().height();
        QVERIFY(dialog.minimumSizeHint().height() < preferred);
        dialog.resize(800, 300);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(dialog.height() < preferred);
        QVERIFY(widget->height() >= preferred);
    }

    // The least height of the page follows its preferred height when a part of it is hidden, so
    // that the page does not keep the height of the hidden part.
    void the_page_follows_a_change_of_its_height() {
        SettingCatalog catalog;
        const auto page = new FoldingPage();
        catalog.addPage(page);
        SettingsDialog dialog(&catalog);
        dialog.selectPage(QStringLiteral("folding"));
        dialog.resize(800, 400);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        const auto widget = page->widget();
        QVERIFY(widget->minimumHeight() >= 900);
        page->high->hide();
        QTRY_VERIFY(widget->minimumHeight() < 900);
        page->high->show();
        QTRY_VERIFY(widget->minimumHeight() >= 900);
    }

    // Cancel releases the pages, and their edits with them. The next dialog reads the values
    // anew.
    void cancel_discards_the_edits() {
        Pages pages;
        {
            SettingsDialog dialog(&pages.catalog);
            dialog.selectPage(QStringLiteral("audio.export"));
            pages.exporting->edit->setText(QStringLiteral("flac"));
            dialog.reject();
            QVERIFY(!pages.exporting->hasWidget());
        }
        QCOMPARE(pages.exporting->value, QStringLiteral("start"));
        SettingsDialog dialog(&pages.catalog);
        dialog.selectPage(QStringLiteral("audio.export"));
        QCOMPARE(pages.exporting->edit->text(), QStringLiteral("start"));
    }

    // After every page has applied its settings, settingsApplied() is called once for each page
    // with a widget, so that a widget that shows values of other pages shows them as applied. It
    // is called for no page if a page fails to apply.
    void settings_applied_is_called_for_each_page_with_a_widget() {
        Pages pages;
        SettingsDialog dialog(&pages.catalog);
        dialog.selectPage(QStringLiteral("audio.export"));
        dialog.selectPage(QStringLiteral("audio.output"));
        QVERIFY(!pages.appearance->hasWidget());

        pages.output->edit->setText(QStringLiteral("512"));
        dialog.applyButton()->click();
        QCOMPARE(pages.output->settingsAppliedCalls, 1);
        QCOMPARE(pages.exporting->settingsAppliedCalls, 1);
        QCOMPARE(pages.appearance->settingsAppliedCalls, 0);

        pages.output->edit->clear();
        dialog.applyButton()->click();
        QCOMPARE(pages.output->settingsAppliedCalls, 1);
        QCOMPARE(pages.exporting->settingsAppliedCalls, 1);
    }
};

int main(int argc, char *argv[]) {
    // Runs without a display
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    test_SettingsDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_SettingsDialog.moc"
