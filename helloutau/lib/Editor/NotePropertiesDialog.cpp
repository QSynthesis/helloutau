#include "NotePropertiesDialog.h"

#include <functional>

#include <QtCore/QRegularExpression>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

namespace hello::daw {

    namespace {

        // The tempo a box offers, which a note may exceed, since UTAU writes any value
        constexpr double MaximumTempo = 1000;

        QString numberText(double value) {
            return QString::number(value, 'g', 12);
        }

        // The value that every note has, or none if they differ
        template <class Value>
        std::optional<Value> shared(const QList<kit::Note> &notes,
                                    const std::function<Value(const kit::Note &)> &of) {
            if (notes.isEmpty()) {
                return std::nullopt;
            }
            const auto first = of(notes.first());
            for (const auto &note : notes) {
                if (!(of(note) == first)) {
                    return std::nullopt;
                }
            }
            return first;
        }

    }

    NotePropertiesDialog::NotePropertiesDialog(const QList<kit::Note> &notes, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Note Properties"));

        const auto various = tr("(various)");
        const auto integer =
            new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d*")), this);
        const auto number = new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("-?\\d*(\\.\\d*)?")), this);

        auto form = new QFormLayout();
        const auto add = [&](const QString &label, QLineEdit *edit) {
            const int index = int(m_fields.size());
            m_fields.push_back(edit);
            m_edited.push_back(false);
            connect(edit, &QLineEdit::textEdited, this, [this, index] { m_edited[index] = true; });
            form->addRow(label, edit);
        };

        // A text property: the shared value, or nothing with "(various)"
        const auto text = [&](const QString &label,
                              const std::function<QString(const kit::Note &)> &of) {
            const auto value = shared<QString>(notes, of);
            auto edit = new QLineEdit(value.value_or(QString()));
            if (!value) {
                edit->setPlaceholderText(various);
            }
            add(label, edit);
        };

        // A property that a note may leave to the default: the shared value, "(default)" where
        // all leave it, or "(various)"
        const auto optional =
            [&](const QString &label, const QString &unset,
                const std::function<std::optional<double>(const kit::Note &)> &of) {
                const auto value = shared<std::optional<double>>(notes, of);
                auto edit = new QLineEdit();
                edit->setValidator(number);
                if (!value) {
                    edit->setPlaceholderText(various);
                } else if (*value) {
                    edit->setText(numberText(**value));
                } else {
                    edit->setPlaceholderText(unset);
                }
                add(label, edit);
            };

        const auto defaulted = tr("(default)");
        text(tr("&Lyric:"), [](const kit::Note &note) { return note.lyric; });
        {
            const auto length =
                shared<int>(notes, [](const kit::Note &note) { return note.length; });
            auto edit = new QLineEdit(length ? QString::number(*length) : QString());
            edit->setValidator(integer);
            if (!length) {
                edit->setPlaceholderText(various);
            }
            add(tr("Len&gth (ticks):"), edit);
        }
        optional(tr("&Tempo:"), tr("(follows the tempo before)"),
                 [](const kit::Note &note) { return note.tempo; });
        optional(tr("&Intensity:"), defaulted,
                 [](const kit::Note &note) { return note.intensity; });
        optional(tr("&Modulation:"), defaulted,
                 [](const kit::Note &note) { return note.modulation; });
        optional(tr("Consonant &velocity:"), defaulted,
                 [](const kit::Note &note) { return note.velocity; });
        optional(tr("&Pre-utterance:"), defaulted,
                 [](const kit::Note &note) { return note.preUtterance; });
        optional(tr("&Overlap:"), defaulted,
                 [](const kit::Note &note) { return note.voiceOverlap; });
        optional(tr("&Start point:"), defaulted,
                 [](const kit::Note &note) { return note.startPoint; });
        text(tr("&Flags:"), [](const kit::Note &note) { return note.flags; });

        auto note = new QLabel(tr("Only the fields you edit change, on every selected note. An "
                                  "emptied number returns the note to the default."));
        note->setWordWrap(true);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(note);
        layout->addWidget(buttons);
        resize(420, 0);
    }

    NotePropertiesDialog::~NotePropertiesDialog() = default;

    kit::NotePropertyChanges NotePropertiesDialog::changes() const {
        kit::NotePropertyChanges changes;
        const auto edited = [this](Field field) { return m_edited[field]; };
        const auto textOf = [this](Field field) { return m_fields[field]->text(); };

        // Empty for the default, or a number; left out if it does not read as one
        const auto optional = [&](Field field, std::optional<std::optional<double>> &change) {
            if (!edited(field)) {
                return;
            }
            const auto text = textOf(field).trimmed();
            if (text.isEmpty()) {
                change = std::optional<double>();
                return;
            }
            bool ok = false;
            const double value = text.toDouble(&ok);
            if (ok) {
                change = std::optional(value);
            }
        };

        if (edited(Lyric)) {
            changes.lyric = textOf(Lyric);
        }
        if (edited(Length)) {
            bool ok = false;
            const int length = textOf(Length).toInt(&ok);
            if (ok && length > 0) {
                changes.length = length;
            }
        }
        optional(Tempo, changes.tempo);
        optional(Intensity, changes.intensity);
        optional(Modulation, changes.modulation);
        optional(Velocity, changes.velocity);
        optional(PreUtterance, changes.preUtterance);
        optional(VoiceOverlap, changes.voiceOverlap);
        optional(StartPoint, changes.startPoint);
        if (edited(Flags)) {
            changes.flags = textOf(Flags);
        }
        return changes;
    }

    QLineEdit *NotePropertiesDialog::field(Field field) const {
        return m_fields.value(field);
    }

    TempoDialog::TempoDialog(std::optional<double> tempo, double current, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Tempo"));

        m_tempo = new QDoubleSpinBox();
        m_tempo->setDecimals(2);
        m_tempo->setRange(0.01, MaximumTempo);
        m_tempo->setValue(tempo.value_or(current));
        m_tempo->setSuffix(tr(" BPM"));
        m_follow = new QCheckBox(tr("&Follow the tempo before"));
        m_follow->setChecked(!tempo);
        m_tempo->setEnabled(bool(tempo));
        connect(m_follow, &QCheckBox::toggled, m_tempo,
                [this](bool follow) { m_tempo->setEnabled(!follow); });

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto form = new QFormLayout();
        form->addRow(tr("&Tempo:"), m_tempo);
        form->addRow(m_follow);
        auto layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    TempoDialog::~TempoDialog() = default;

    std::optional<double> TempoDialog::tempo() const {
        return m_follow->isChecked() ? std::nullopt : std::optional(m_tempo->value());
    }

    QDoubleSpinBox *TempoDialog::tempoBox() const {
        return m_tempo;
    }

    QCheckBox *TempoDialog::followBox() const {
        return m_follow;
    }

}
