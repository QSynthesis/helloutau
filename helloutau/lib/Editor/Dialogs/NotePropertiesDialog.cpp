#include "NotePropertiesDialog.h"

#include <functional>

#include <QtCore/QRegularExpression>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>

#include <stdutau/utaconst.h>

#include <helloutau/Widgets/PianoKeyboard.h>

namespace hello::daw {

    namespace {

        // The keys of the box of the key, C1 and B8 as in UTAU
        constexpr int LowestKey = 24;
        constexpr int HighestKey = 119;

        QString numberText(double value) {
            return QString::number(value, 'g', 12);
        }

        // Returns the value that every note has, or std::nullopt if they differ
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

    NotePropertiesDialog::NotePropertiesDialog(const QList<kit::Note> &notes,
                                               const QList<Defaults> &defaults, QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Note Properties"));

        const auto various = tr("(various)");
        const auto integer =
            new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d*")), this);
        const auto number = new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("-?\\d*(\\.\\d*)?")), this);

        auto form = new QFormLayout();
        const auto add = [&](const QString &label, QLineEdit *edit) {
            edit->setClearButtonEnabled(true);
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
        const auto optional = [&](const QString &label, const QString &unset,
                                  const std::function<std::optional<double>(const kit::Note &)> &of,
                                  const QList<double> &inherited = QList<double>()) {
            const auto value = shared<std::optional<double>>(notes, of);
            auto edit = new QLineEdit();
            edit->setValidator(number);
            const auto inheritedText = [&] {
                if (inherited.size() == notes.size() && !inherited.isEmpty()) {
                    const auto first = inherited.first();
                    bool same = true;
                    for (const auto item : inherited) {
                        same = same && item == first;
                    }
                    if (same) {
                        return tr("(default: %1)").arg(numberText(first));
                    }
                }
                return various;
            };
            if (!value) {
                bool allUnset = true;
                for (const auto &note : notes) {
                    if (of(note)) {
                        allUnset = false;
                        break;
                    }
                }
                if (allUnset && inherited.size() == notes.size() && !inherited.isEmpty()) {
                    edit->setPlaceholderText(inheritedText());
                } else {
                    edit->setPlaceholderText(various);
                }
            } else if (*value) {
                edit->setText(numberText(**value));
            } else {
                edit->setPlaceholderText(inherited.isEmpty() ? unset : inheritedText());
            }
            add(label, edit);
        };

        // Returns the default of one property for every note, or an empty list unless every note
        // has a default
        const auto defaultsOf =
            [&](const std::function<std::optional<double>(const Defaults &)> &of) {
                QList<double> values;
                if (defaults.size() != notes.size()) {
                    return values;
                }
                for (const auto &item : defaults) {
                    const auto value = of(item);
                    if (!value) {
                        return QList<double>();
                    }
                    values.push_back(*value);
                }
                return values;
            };
        const auto defaulted = [&](double value) {
            return tr("(default: %1)").arg(numberText(value));
        };
        const auto voiceBankDefault = tr("(default: voice bank)");
        text(tr("&Lyric:"), [](const kit::Note &note) { return note.lyric; });
        {
            // The keys of UTAU, and a key of the notes outside them so that it is shown
            const auto key = shared<int>(notes, [](const kit::Note &note) { return note.noteNum; });
            m_key = new QComboBox();
            for (int noteNum = LowestKey; noteNum <= HighestKey; ++noteNum) {
                m_key->addItem(PianoKeyboard::keyName(noteNum), noteNum);
            }
            if (key && (*key < LowestKey || *key > HighestKey)) {
                m_key->insertItem(*key < LowestKey ? 0 : m_key->count(),
                                  PianoKeyboard::keyName(*key), *key);
            }
            m_key->setPlaceholderText(various);
            m_key->setCurrentIndex(key ? m_key->findData(*key) : -1);
            connect(m_key, &QComboBox::activated, this, [this] { m_keyEdited = true; });
            form->addRow(tr("&Key:"), m_key);
        }
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
        optional(
            tr("&Tempo:"), tr("(follows the tempo before)"),
            [](const kit::Note &note) { return note.tempo; },
            defaultsOf([](const Defaults &item) { return std::optional(item.tempo); }));
        optional(tr("&Intensity:"), defaulted(utau::DEFAULT_VALUE_INTENSITY),
                 [](const kit::Note &note) { return note.intensity; });
        optional(tr("&Modulation:"), defaulted(utau::DEFAULT_VALUE_MODULATION),
                 [](const kit::Note &note) { return note.modulation; });
        optional(tr("Consonant &velocity:"), defaulted(utau::DEFAULT_VALUE_VELOCITY),
                 [](const kit::Note &note) { return note.velocity; });
        optional(
            tr("&Pre-utterance:"), voiceBankDefault,
            [](const kit::Note &note) { return note.preUtterance; },
            defaultsOf([](const Defaults &item) { return item.preUtterance; }));
        optional(
            tr("&Overlap:"), voiceBankDefault,
            [](const kit::Note &note) { return note.voiceOverlap; },
            defaultsOf([](const Defaults &item) { return item.voiceOverlap; }));
        optional(tr("&Start point:"), defaulted(utau::DEFAULT_VALUE_START_POINT),
                 [](const kit::Note &note) { return note.startPoint; });
        text(tr("&Flags:"), [](const kit::Note &note) { return note.flags; });

        auto note = new QLabel(tr("Only the fields that are edited change, on every selected note. "
                                  "An emptied number returns the note to the default."));
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

        // Sets change to empty for the default, or to a number. Leaves it unchanged if the text
        // does not read as a number
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
        if (m_keyEdited && m_key->currentIndex() >= 0) {
            changes.noteNum = m_key->currentData().toInt();
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

    QComboBox *NotePropertiesDialog::keyBox() const {
        return m_key;
    }

    TempoDialog::TempoDialog(std::optional<double> tempo, double current, QWidget *parent)
        : QDialog(parent), m_original(tempo) {
        setWindowTitle(tr("Tempo"));

        m_tempo = new QDoubleSpinBox();
        m_tempo->setDecimals(2);
        m_tempo->setRange(utau::VALUE_TEMPO_MIN, utau::VALUE_TEMPO_MAX);
        m_tempo->setValue(tempo.value_or(current));
        m_tempo->setSuffix(tr(" BPM"));
        connect(m_tempo, &QDoubleSpinBox::valueChanged, this, [this] { m_tempoEdited = true; });
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
        if (m_follow->isChecked()) {
            return std::nullopt;
        }
        if (!m_tempoEdited && m_original) {
            return m_original;
        }
        return m_tempo->value();
    }

    QDoubleSpinBox *TempoDialog::tempoBox() const {
        return m_tempo;
    }

    QCheckBox *TempoDialog::followBox() const {
        return m_follow;
    }

}
