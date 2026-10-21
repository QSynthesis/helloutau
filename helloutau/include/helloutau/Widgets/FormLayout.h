#ifndef HELLOUTAU_WIDGETS_FORMLAYOUT_H
#define HELLOUTAU_WIDGETS_FORMLAYOUT_H

#include <QtWidgets/QFormLayout>

#include <helloutau/Widgets/HelloUtauWidgetsGlobal.h>

namespace hello::daw {

    /// The form layout of every form of the application, which lays out a form as the styles of
    /// Windows do on every platform. The fields grow to the width of the form, and the form is
    /// aligned to the leading edge. If the style derives the vertical spacing from the kinds of
    /// two controls, the spacing is fixed at the spacing between two line edits. The labels keep
    /// the alignment of the style.
    ///
    /// The style of macOS keeps each field at its size hint, centers the form, and spaces a row
    /// with a push button farther from its neighbors than a row of line edits, so that the fields
    /// of a form differ in width and the rows with a push button leave gaps. A proxy style over
    /// the style of the application, which would change the same hints for every form, is
    /// rejected because the style sheets of the application no longer apply under it (author's
    /// decision, 2026-10-21).
    class HELLOUTAU_WIDGETS_EXPORT FormLayout : public QFormLayout {
    public:
        explicit FormLayout(QWidget *parent = nullptr);
        ~FormLayout();
    };

}

#endif // HELLOUTAU_WIDGETS_FORMLAYOUT_H
