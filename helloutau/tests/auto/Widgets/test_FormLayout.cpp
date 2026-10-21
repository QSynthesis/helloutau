#include <memory>

#include <QtTest/QTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QWidget>

#include <helloutau/Widgets/FormLayout.h>

using namespace hello::daw;

namespace {

    // The layout of forms of the style of macOS over Fusion, which every platform has: fields at
    // their size hints, a centered form, right-aligned labels, and a vertical spacing that
    // depends on the controls
    class MacLikeStyle : public QProxyStyle {
    public:
        MacLikeStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {
        }

        int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                      QStyleHintReturn *returnData) const override {
            switch (hint) {
                case SH_FormLayoutFieldGrowthPolicy:
                    return QFormLayout::FieldsStayAtSizeHint;
                case SH_FormLayoutFormAlignment:
                    return int(Qt::AlignHCenter | Qt::AlignTop);
                case SH_FormLayoutLabelAlignment:
                    return int(Qt::AlignRight);
                default:
                    return QProxyStyle::styleHint(hint, option, widget, returnData);
            }
        }

        int pixelMetric(PixelMetric metric, const QStyleOption *option,
                        const QWidget *widget) const override {
            // A negative spacing, for which the layouts ask layoutSpacing()
            if (metric == PM_LayoutVerticalSpacing || metric == PM_LayoutHorizontalSpacing) {
                return -1;
            }
            return QProxyStyle::pixelMetric(metric, option, widget);
        }

        int layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2,
                          Qt::Orientation orientation, const QStyleOption *option,
                          const QWidget *widget) const override {
            Q_UNUSED(orientation);
            Q_UNUSED(option);
            Q_UNUSED(widget);
            return (control1 | control2) & QSizePolicy::PushButton ? 20 : 10;
        }
    };

}

class test_FormLayout : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // The fields grow to the width of the form, which starts at the leading edge, and the
    // labels keep the alignment of the style.
    void the_fields_grow_under_the_style_of_macos() {
        MacLikeStyle style;
        QWidget window;
        window.setStyle(&style);
        auto form = new FormLayout(&window);
        QCOMPARE(form->fieldGrowthPolicy(), QFormLayout::AllNonFixedFieldsGrow);
        QCOMPARE(form->formAlignment(), Qt::AlignLeading | Qt::AlignTop);
        QCOMPARE(form->labelAlignment(), Qt::AlignRight);

        auto name = new QLineEdit();
        auto path = new QLineEdit();
        path->setMinimumWidth(300);
        form->addRow(QStringLiteral("Name:"), name);
        form->addRow(QStringLiteral("Path:"), path);
        window.resize(600, 100);
        form->activate();
        QCOMPARE(name->width(), path->width());
        QCOMPARE(name->geometry().right(), form->contentsRect().right());
    }

    // A row with a push button has the spacing of a row of line edits.
    void the_vertical_spacing_ignores_the_controls() {
        MacLikeStyle style;
        QWidget window;
        window.setStyle(&style);
        auto form = new FormLayout(&window);
        QCOMPARE(form->verticalSpacing(), 10);

        auto first = new QLineEdit();
        auto second = new QLineEdit();
        auto third = new QLineEdit();
        auto browse = new QHBoxLayout();
        browse->addWidget(second, 1);
        browse->addWidget(new QPushButton(QStringLiteral("Browse...")));
        form->addRow(QStringLiteral("First:"), first);
        form->addRow(QStringLiteral("Second:"), browse);
        form->addRow(QStringLiteral("Third:"), third);
        window.resize(600, 200);
        form->activate();
        QCOMPARE(browse->geometry().top() - first->geometry().bottom() - 1, 10);
        QCOMPARE(third->geometry().top() - browse->geometry().bottom() - 1, 10);
    }

    // A style with a fixed vertical spacing keeps it.
    void a_fixed_spacing_of_the_style_is_kept() {
        const std::unique_ptr<QStyle> style(QStyleFactory::create(QStringLiteral("Fusion")));
        QWidget window;
        window.setStyle(style.get());
        const auto form = new FormLayout(&window);
        QCOMPARE(form->verticalSpacing(), style->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
    }
};

QTEST_MAIN(test_FormLayout)

#include "test_FormLayout.moc"
