#include "PianoRoll.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMimeData>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QVBoxLayout>

#include <stdcorelib/pimpl.h>

#include <hellokit/Document/DocumentConstants.h>
#include <hellokit/Edit/ProjectEdits.h>
#include <hellokit/Edit/ProjectRefs.h>
#include <hellokit/Edit/ProjectSession.h>
#include <hellokit/Edit/TrackTimeline.h>
#include <hellokit/Synth/PitchCurve.h>
#include <hellokit/VoiceBank/VoiceBank.h>

#include <helloutau/Audio/AudioEngine.h>
#include <helloutau/Audio/AudioOutput.h>
#include <helloutau/Audio/PianoToneSource.h>
#include <helloutau/Widgets/PianoKeyboard.h>
#include <helloutau/Widgets/SceneView.h>
#include <helloutau/Widgets/TimelineRuler.h>

#include "PianoRollLayers_p.h"
#include "PianoRollParameterLayers_p.h"
#include "PianoRollState_p.h"

namespace hello::daw {

    namespace {

        constexpr int LowestPianoKey = 24;   // C1
        constexpr int HighestPianoKey = 107; // B7

        // Calls a function with each event of an object
        class EventWatcher : public QObject {
        public:
            EventWatcher(QObject *watched, std::function<void(QEvent *)> seen)
                : QObject(watched), m_seen(std::move(seen)) {
                watched->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                m_seen(event);
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void(QEvent *)> m_seen;
        };

        // Reports where the pointer is over a widget, and none once it leaves
        class PointerTracker : public QObject {
        public:
            PointerTracker(QWidget *widget, std::function<void(std::optional<QPointF>)> moved)
                : QObject(widget), m_moved(std::move(moved)) {
                widget->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() == QEvent::MouseMove) {
                    m_moved(static_cast<QMouseEvent *>(event)->position());
                } else if (event->type() == QEvent::Leave) {
                    m_moved(std::nullopt);
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            std::function<void(std::optional<QPointF>)> m_moved;
        };

    }

    class PianoRoll::Impl : public PianoRollState {
    public:
        using Decl = PianoRoll;

        explicit Impl(Decl *decl) : _decl(decl) {
            widget = decl;
        }

        Decl *_decl;
    };

    PianoRoll::PianoRoll(kit::ProjectSession *session, QWidget *parent)
        : QWidget(parent), _impl(std::make_unique<Impl>(this)) {
        stdc_impl_t;
        impl.session = session;
        impl.timeline = new kit::TrackTimeline(session, 0, this);
        impl.view = new SceneView();
        impl.view->setKeyRange(LowestPianoKey, HighestPianoKey);
        impl.ruler = new TimelineRuler(impl.view);
        impl.keyboard = new PianoKeyboard(impl.view);
        auto keyOutput = new AudioOutput(this);
        connect(impl.keyboard, &PianoKeyboard::keyPressed, this, [keyOutput](int key) {
            const int rate = AudioEngine::instance()->sampleRate();
            if (rate <= 0) {
                return;
            }
            constexpr double duration = 0.35;
            const double frequency = 440 * std::pow(2.0, (key - 69) / 12.0);
            keyOutput->start(std::make_shared<PianoToneSource>(rate, frequency, duration), rate);
        });
        impl.voiceBankButton = new QToolButton();
        impl.voiceBankButton->setAutoRaise(true);
        impl.voiceBankButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        impl.voiceBankButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        impl.voiceBankButton->setIconSize(QSize(64, 64));
        impl.voiceBankButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        impl.voiceBankButton->setToolTip(tr("Open Project Properties"));
        connect(impl.voiceBankButton, &QToolButton::clicked, this,
                [this] { Q_EMIT voiceBankRequested(); });
        impl.ruler->setTicksPerBeat(impl.beatTicks);
        impl.ruler->setBeatsPerBar(impl.barTicks / impl.beatTicks);
        connect(impl.ruler, &TimelineRuler::markDoubleClicked, this, [this](int mark) {
            stdc_impl_t;
            if (mark >= 0 && mark < impl.markNotes.size()) {
                Q_EMIT tempoRequested(impl.markNotes[mark]);
            }
        });
        connect(impl.ruler, &TimelineRuler::sectionDoubleClicked, this, [this](int section) {
            stdc_impl_t;
            if (section < 0 || section >= impl.sectionNotes.size() ||
                !impl.isCurrent(impl.sectionNotes[section])) {
                return;
            }
            const auto notes = impl.sectionNotes[section];
            if (notes.label) {
                Q_EMIT labelRequested(notes.first);
            } else {
                Q_EMIT regionEditRequested(kit::Region{notes.name, notes.first, notes.last});
            }
        });
        connect(impl.ruler, &TimelineRuler::sectionClicked, this, [this](int section) {
            stdc_impl_t;
            if (section >= 0 && section < impl.sectionNotes.size() &&
                impl.isCurrent(impl.sectionNotes.at(section))) {
                const auto notes = impl.sectionNotes.at(section);
                impl.selectRange(notes.first, notes.last);
            }
        });
        connect(impl.ruler, &TimelineRuler::menuRequested, this,
                [this](double tick, const QPoint &globalPosition) {
                    stdc_impl_t;
                    impl.showRulerMenu(tick, globalPosition);
                });
        connect(impl.ruler, &TimelineRuler::positionPressed, this,
                [this](double tick, Qt::KeyboardModifiers modifiers) {
                    stdc_impl_t;
                    if (!impl.cursorEnabled) {
                        return;
                    }
                    const bool disabled = impl.noteModifiers.isHeld(
                        NoteViewModifiers::DisableNoteSnap, modifiers);
                    const double at = std::max<double>(0, double(impl.snapped(tick, disabled)));
                    const bool moved = at != impl.cursor;
                    setCursorPosition(at);
                    if (moved) {
                        Q_EMIT cursorMoved(at);
                    }
                });

        impl.view->addLayer(std::make_unique<Impl::GridLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::NoteLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::NoteEnvelopeLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::NoteParameterLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::RenderedPitchLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::PitchLayer>(&impl));
        impl.view->addLayer(std::make_unique<Impl::OverlayLayer>(&impl));
        new PointerTracker(impl.view->viewport(), [this](std::optional<QPointF> position) {
            stdc_impl_t;
            impl.hover(position);
        });

        // The parameter area: the time axis of the roll, and volumes in percent for keys, all
        // of them in view
        impl.parameters = new SceneView();
        // The parameter area scrolls and zooms with the wheel of the note area.
        const auto wheelActions =
            [this](Qt::KeyboardModifiers modifiers) -> std::optional<SceneView::WheelAction> {
            stdc_impl_t;
            const auto activation = impl.activate(NoteViewModifiers::WheelScene, modifiers);
            if (!activation) {
                return std::nullopt;
            }
            switch (activation->operation) {
                case NoteViewModifiers::VerticalScroll:
                    return SceneView::VerticalScroll;
                case NoteViewModifiers::HorizontalScroll:
                    return SceneView::HorizontalScroll;
                case NoteViewModifiers::TimeZoom:
                    return SceneView::TimeZoom;
                case NoteViewModifiers::KeyZoom:
                    return SceneView::KeyZoom;
                default:
                    return std::nullopt;
            }
        };
        impl.view->setWheelActions(wheelActions);
        impl.parameters->setWheelActions(wheelActions);
        impl.parameters->setFixedHeight(PianoRollState::ParameterHeight);
        impl.parameters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        impl.parameters->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        impl.parameters->setKeyScaleRange(0.01, 100);
        // Room for the margins beyond the volumes
        impl.parameters->setKeyRange(-int(PianoRollState::EnvelopeRange / 10),
                                     int(PianoRollState::EnvelopeRange * 1.1));
        impl.parameters->addLayer(std::make_unique<Impl::EnvelopeLayer>(&impl));
        impl.parameters->addLayer(std::make_unique<Impl::ValueLayer>(&impl));
        impl.parameters->addLayer(std::make_unique<Impl::DragLabelLayer>(&impl));

        // The buttons that choose what the parameter area shows, one above the other
        // (QSynthesis)
        impl.laneBar = new QWidget();
        impl.laneButtons = new QButtonGroup(this);
        auto laneLayout = new QVBoxLayout(impl.laneBar);
        laneLayout->setContentsMargins(2, 2, 2, 2);
        laneLayout->setSpacing(1);
        const struct {
            Lane lane;
            const char *text;
            const char *toolTip;
        } lanes[] = {
            {EnvelopeLane,   QT_TR_NOOP("Env"), QT_TR_NOOP("Envelope")  },
            {IntensityLane,  QT_TR_NOOP("Int"), QT_TR_NOOP("Intensity") },
            {ModulationLane, QT_TR_NOOP("Mod"), QT_TR_NOOP("Modulation")},
            {VelocityLane,   QT_TR_NOOP("Vel"), QT_TR_NOOP("Velocity")  },
        };
        for (const auto &lane : lanes) {
            auto button = new QToolButton();
            button->setText(tr(lane.text));
            button->setToolTip(tr(lane.toolTip));
            button->setCheckable(true);
            button->setChecked(lane.lane == impl.lane);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            impl.laneButtons->addButton(button, lane.lane);
            laneLayout->addWidget(button);
        }
        connect(impl.laneButtons, &QButtonGroup::idClicked, this,
                [this](int id) { setLane(Lane(id)); });
        new EventWatcher(impl.parameters->viewport(), [this](QEvent *event) {
            stdc_impl_t;
            if (event->type() == QEvent::Resize) {
                impl.fitParameters();
            }
        });
        connect(impl.parameters, &SceneView::keyAxisChanged, this, [this] {
            stdc_impl_t;
            impl.fitParameters();
        });
        connect(impl.view, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.parameters->setTimeAxis(impl.view->timeAxis());
        });
        connect(impl.parameters, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.view->setTimeAxis(impl.parameters->timeAxis());
        });

        impl.editor = new PianoRollState::LyricEditor(impl.view->viewport());
        impl.editor->hide();
        impl.editor->committed = [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        };
        impl.editor->cancelled = [this] {
            stdc_impl_t;
            impl.finishEditing(false);
        };
        impl.editor->tabbed = [this](bool forward) {
            stdc_impl_t;
            impl.editNext(forward);
        };

        auto layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(impl.ruler, 0, 1);
        layout->addWidget(impl.voiceBankButton, 0, 0);
        layout->addWidget(impl.keyboard, 1, 0);
        layout->addWidget(impl.view, 1, 1);
        layout->addWidget(impl.laneBar, 2, 0);
        layout->addWidget(impl.parameters, 2, 1);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(1, 1);

        connect(impl.timeline, &kit::TrackTimeline::invalidated, this, [this] {
            stdc_impl_t;
            impl.timingsStale = true;
            impl.view->viewport()->update();
            impl.parameters->viewport()->update();
            impl.scheduleRefresh();
            // The note being edited may be gone, as may selected ones.
            if (impl.editing && impl.indexOf(impl.editing) < 0) {
                impl.finishEditing(false);
            }
            // With Mode2 off, the points are hidden and none is selected.
            if (impl.mode1()) {
                impl.selectedPoints.clear();
                impl.hovered = -1;
            }
            Q_EMIT selectionChanged();
        });
        // Scrolling and zooming move the note away from the editor.
        connect(impl.view, &SceneView::timeAxisChanged, this, [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        });
        connect(impl.view, &SceneView::keyAxisChanged, this, [this] {
            stdc_impl_t;
            impl.finishEditing(true);
        });
        impl.refresh();
        scrollToNotes();
    }

    PianoRoll::~PianoRoll() = default;

    SceneView *PianoRoll::view() const {
        stdc_impl_t;
        return impl.view;
    }

    void PianoRoll::setModifierBindings(const ModifierBindings &bindings) {
        stdc_impl_t;
        if (&bindings.scheme() == &ParameterViewModifiers::scheme()) {
            impl.parameterModifiers = bindings;
            return;
        }
        Q_ASSERT(&bindings.scheme() == &NoteViewModifiers::scheme());
        impl.noteModifiers = bindings;
    }

    SceneView *PianoRoll::parameterView() const {
        stdc_impl_t;
        return impl.parameters;
    }

    PianoRoll::Lane PianoRoll::lane() const {
        stdc_impl_t;
        return impl.lane;
    }

    void PianoRoll::setLane(Lane lane) {
        stdc_impl_t;
        impl.laneButtons->button(lane)->setChecked(true);
        if (lane == impl.lane) {
            return;
        }
        impl.lane = lane;
        // Room for the margins beyond the values
        const auto range = Impl::rangeOf(lane);
        const double span = range.maximum - range.minimum;
        impl.parameters->setKeyRange(int(std::floor(range.minimum - span / 10)),
                                     int(std::ceil(range.maximum + span / 10)));
        impl.fitParameters();
        impl.parameters->viewport()->update();
    }

    TimelineRuler *PianoRoll::ruler() const {
        stdc_impl_t;
        return impl.ruler;
    }

    PianoKeyboard *PianoRoll::keyboard() const {
        stdc_impl_t;
        return impl.keyboard;
    }

    kit::TrackTimeline *PianoRoll::timeline() const {
        stdc_impl_t;
        return impl.timeline;
    }

    void PianoRoll::showNote(int index) {
        stdc_impl_t;
        if (index >= 0 && index < impl.timeline->noteCount()) {
            impl.ensureVisible(index);
        }
    }

    void PianoRoll::scrollToNotes() {
        stdc_impl_t;
        const auto timeline = impl.timeline;
        int lowest = 127;
        int highest = 0;
        for (int i = 0; i < timeline->noteCount(); ++i) {
            const auto &note = timeline->note(i);
            if (!note.rest) {
                lowest = std::min(lowest, note.key);
                highest = std::max(highest, note.key);
            }
        }
        // C4 when the track has no sung note
        const double middle = lowest <= highest ? (lowest + highest + 1) / 2.0 : 60.5;

        auto keys = impl.view->keyAxis();
        keys.top = middle + impl.view->viewport()->height() / keys.pixelsPerKey / 2;
        impl.view->setKeyAxis(keys);
        auto time = impl.view->timeAxis();
        time.left = 0;
        impl.view->setTimeAxis(time);
    }

    std::shared_ptr<const kit::VoiceBank> PianoRoll::voiceBank() const {
        stdc_impl_t;
        return impl.voiceBank;
    }

    void PianoRoll::setVoiceBank(std::shared_ptr<const kit::VoiceBank> bank) {
        stdc_impl_t;
        impl.voiceBank = std::move(bank);
        QPixmap image;
        if (impl.voiceBank) {
            if (const auto path = kit::VoiceBank::imagePathOf(impl.voiceBank->root(),
                                                              impl.voiceBank->character().image)) {
                image.load(QString::fromStdU16String(path->u16string()));
            }
        }
        auto icon = image.isNull() ? style()->standardIcon(QStyle::SP_FileIcon) : QIcon(image);
        const bool leftOut = impl.voiceBank && std::any_of(
                                               impl.voiceBank->directories().cbegin(),
                                               impl.voiceBank->directories().cend(),
                                               [](const auto &directory) { return directory.leftOut; });
        if (leftOut) {
            const auto size = impl.voiceBankButton->iconSize();
            auto pixmap = icon.pixmap(size);
            QPainter painter(&pixmap);
            const auto warningSize = QSize(size.width() / 2, size.height() / 2);
            painter.drawPixmap(size.width() - warningSize.width(),
                               size.height() - warningSize.height(),
                               style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(
                                   warningSize));
            icon = QIcon(pixmap);
        }
        impl.voiceBankButton->setIcon(icon);
        impl.timingsStale = true;
        impl.view->viewport()->update();
        impl.parameters->viewport()->update();
    }

    bool PianoRoll::lacksSample(int index) const {
        stdc_impl_t;
        const auto &bank = impl.voiceBank;
        if (!bank) {
            return false;
        }
        const auto &note = impl.timeline->note(index);
        return !note.rest && !bank->find(note.key, note.lyric);
    }

    PianoRoll::Tool PianoRoll::tool() const {
        stdc_impl_t;
        return impl.tool;
    }

    void PianoRoll::setTool(Tool tool) {
        stdc_impl_t;
        impl.tool = tool;
    }

    int PianoRoll::quantization() const {
        stdc_impl_t;
        return impl.quantization;
    }

    void PianoRoll::setQuantization(int ticks) {
        stdc_impl_t;
        if (impl.quantization == ticks) {
            return;
        }
        impl.quantization = ticks;
        Q_EMIT quantizationChanged(ticks);
    }

    QString PianoRoll::quantizationName(int ticks) {
        return ticks > 0 ? QStringLiteral("1/%1").arg(PianoRollState::WholeNoteTicks / ticks)
                         : tr("Off");
    }

    QList<int> PianoRoll::quantizations() {
        return {PianoRollState::WholeNoteTicks / 4,  PianoRollState::WholeNoteTicks / 8,
                PianoRollState::WholeNoteTicks / 16, PianoRollState::WholeNoteTicks / 32,
                PianoRollState::WholeNoteTicks / 64, 0};
    }

    void PianoRoll::setTimeSignature(int numerator, int denominator) {
        stdc_impl_t;
        impl.beatTicks = PianoRollState::WholeNoteTicks / std::max(1, denominator);
        impl.barTicks = impl.beatTicks * std::max(1, numerator);
        impl.ruler->setTicksPerBeat(impl.beatTicks);
        impl.ruler->setBeatsPerBar(std::max(1, numerator));
        impl.scheduleRefresh();
        impl.view->viewport()->update();
    }

    int PianoRoll::quantizedLength() const {
        stdc_impl_t;
        return impl.quantization > 0 ? impl.quantization : kit::ticksPerQuarter;
    }

    std::optional<std::pair<int, int>> PianoRoll::selectedRange() const {
        const auto indices = selectedIndices();
        if (indices.isEmpty() || indices.last() - indices.first() + 1 != indices.size()) {
            return std::nullopt;
        }
        return std::pair{indices.first(), indices.last()};
    }

    QList<kit::Region> PianoRoll::regions() const {
        stdc_impl_t;
        // The notes of the timeline, which a refresh after an edit updates, so that a region
        // never refers to a note that the timeline does not have yet.
        const int count = impl.timeline->noteCount();
        auto result = impl.notes().regions();
        result.removeIf([count](const kit::Region &region) { return region.last >= count; });
        return result;
    }

    std::optional<kit::Region> PianoRoll::regionAt(int index) const {
        std::optional<kit::Region> found;
        for (const auto &region : regions()) {
            if (region.first <= index && index <= region.last) {
                found = region;
            }
        }
        return found;
    }

    bool PianoRoll::removeLabels(const QList<int> &indices, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto refs = impl.notes();
        auto transaction = impl.session->transaction(tr("Remove Label"));
        for (const int index : indices) {
            if (index >= 0 && index < refs.size() && !refs.at(index).label().isEmpty()) {
                kit::ProjectEdits::setLabel(refs.at(index), QString(), diagnostics);
            }
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::removeRegion(int index, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto region = regionAt(index);
        if (!region) {
            return true;
        }
        return kit::ProjectEdits::removeRegion(impl.notes(), *region, diagnostics);
    }

    void PianoRoll::loadRegion(int first, int last) {
        stdc_impl_t;
        const int count = impl.timeline->noteCount();
        if (first < 0 || last < first || last >= count) {
            return;
        }
        QList<int> indices;
        for (int i = first; i <= last; ++i) {
            indices.push_back(i);
        }
        setSelectedIndices(indices);
        auto time = impl.view->timeAxis();
        const double margin = impl.view->viewport()->width() / 10.0 / time.pixelsPerTick;
        time.left = std::max(0.0, double(impl.timeline->note(first).start) - margin);
        impl.view->setTimeAxis(time);
    }

    void PianoRoll::fillRegionMenu(QMenu *menu) {
        menu->clear();
        const auto all = regions();
        if (all.isEmpty()) {
            menu->addAction(tr("No Regions"))->setEnabled(false);
            return;
        }
        for (const auto &region : all) {
            connect(menu->addAction(region.name), &QAction::triggered, this,
                    [this, region] { loadRegion(region.first, region.last); });
        }
    }

    QList<int> PianoRoll::selectedIndices() const {
        stdc_impl_t;
        QList<int> indices;
        if (impl.selection.isEmpty()) {
            return indices;
        }
        for (int i = 0; i < impl.timeline->noteCount(); ++i) {
            if (impl.isSelected(i)) {
                indices.push_back(i);
            }
        }
        return indices;
    }

    void PianoRoll::setSelectedIndices(const QList<int> &indices) {
        stdc_impl_t;
        QSet<kit::edit::NodeId> ids;
        for (const int index : indices) {
            ids.insert(impl.timeline->note(index).id);
        }
        if (!indices.isEmpty()) {
            impl.anchor = impl.timeline->note(indices.first()).id;
        }
        impl.setSelection(ids);
    }

    void PianoRoll::selectAll() {
        stdc_impl_t;
        if (impl.timeline->noteCount() > 0) {
            impl.selectRange(0, impl.timeline->noteCount() - 1);
        }
    }

    QList<std::pair<int, int>> PianoRoll::selectedPoints() const {
        stdc_impl_t;
        QList<std::pair<int, int>> result;
        const auto selected = impl.selectedPointIndices();
        for (auto it = selected.begin(); it != selected.end(); ++it) {
            for (const int j : it.value()) {
                result.push_back({it.key(), j});
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void PianoRoll::setSelectedPoints(const QList<std::pair<int, int>> &points) {
        stdc_impl_t;
        QSet<kit::edit::NodeId> ids;
        const auto refs = impl.notes();
        for (const auto &[index, j] : points) {
            if (index < 0 || index >= impl.timeline->noteCount()) {
                continue;
            }
            const auto list = refs.at(index).portamento();
            if (j >= 0 && j < list.size()) {
                ids.insert(list.at(j).id());
            }
        }
        impl.selectPoints(ids);
    }

    bool PianoRoll::removeSelected(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        if (!impl.selectedPoints.isEmpty()) {
            return impl.removePoints(impl.selectedPointIndices(), diagnostics);
        }
        const auto indices = selectedIndices();
        if (indices.isEmpty()) {
            return true;
        }
        return kit::ProjectEdits::removeNotes(impl.notes(), indices, diagnostics);
    }

    std::optional<bool> PianoRoll::selectedPortamento() const {
        stdc_impl_t;
        std::optional<QList<kit::PortamentoPoint>> first;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            const auto points = impl.pointsOf(index);
            if (first && *first != points)
                return std::nullopt;
            first = points;
        }
        if (!first)
            return std::nullopt;
        return !first->isEmpty();
    }

    std::optional<bool> PianoRoll::selectedVibrato() const {
        stdc_impl_t;
        std::optional<kit::Vibrato> first;
        std::optional<bool> present;
        const auto refs = impl.notes();
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            const auto vibrato = refs.at(index).vibrato();
            if (present && *present != vibrato.has_value())
                return std::nullopt;
            present = vibrato.has_value();
            if (!vibrato)
                continue;
            if (!first) {
                first = *vibrato;
                continue;
            }
            if (first->length != vibrato->length || first->period != vibrato->period ||
                first->amplitude != vibrato->amplitude || first->attack != vibrato->attack ||
                first->release != vibrato->release || first->phase != vibrato->phase ||
                first->offset != vibrato->offset || first->intensity != vibrato->intensity)
                return std::nullopt;
        }
        return present ? *present : std::optional<bool>{};
    }

    bool PianoRoll::setPortamentoEnabled(bool enabled, kit::DiagnosticList &diagnostics,
                                         const QList<kit::PortamentoPoint> &defaultPoints) {
        stdc_impl_t;
        QHash<int, QList<kit::PortamentoPoint>> points;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            if (!enabled) {
                points.insert(index, {});
            } else if (!defaultPoints.isEmpty()) {
                points.insert(index, defaultPoints);
            } else if (impl.pointsOf(index).isEmpty()) {
                if (defaultPoints.isEmpty()) {
                    kit::PortamentoPoint before;
                    before.x = -PianoRollState::DefaultPortamento;
                    kit::PortamentoPoint after;
                    after.x = PianoRollState::DefaultPortamento;
                    points.insert(index, {before, after});
                } else {
                    points.insert(index, defaultPoints);
                }
            }
        }
        if (points.isEmpty())
            return true;
        return impl.writePoints(enabled ? tr("Add Portamento") : tr("Remove Portamento"), points,
                                diagnostics);
    }

    bool PianoRoll::setVibratoEnabled(bool enabled, kit::DiagnosticList &diagnostics,
                                      std::optional<kit::Vibrato> defaultVibrato) {
        stdc_impl_t;
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            sung.push_back(refs.at(index));
        }
        if (sung.isEmpty())
            return true;
        auto transaction =
            impl.session->transaction(enabled ? tr("Add Vibrato") : tr("Remove Vibrato"));
        if (enabled) {
            QList<kit::NoteRef> lacking;
            for (const auto &note : sung) {
                if (!note.vibrato())
                    lacking.push_back(note);
            }
            if (!lacking.isEmpty())
                kit::ProjectEdits::setVibrato(
                    lacking, defaultVibrato.value_or(kit::Vibrato::utauDefault()), diagnostics);
        } else {
            kit::ProjectEdits::setVibrato(sung, std::nullopt, diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::togglePortamento(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        bool allPresent = true;
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            allPresent = allPresent && !impl.pointsOf(index).isEmpty();
        }
        return setPortamentoEnabled(!allPresent, diagnostics);
    }

    bool PianoRoll::toggleVibrato(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        bool allPresent = true;
        const auto refs = impl.notes();
        for (const int index : selectedIndices()) {
            if (impl.timeline->note(index).rest)
                continue;
            allPresent = allPresent && refs.at(index).vibrato().has_value();
        }
        return setVibratoEnabled(!allPresent, diagnostics);
    }

    bool PianoRoll::crossfadeEnvelopes(Crossfade crossfade, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto timeline = impl.timeline;
        const auto &timings = impl.sampleTimings();
        const auto refs = impl.notes();
        const int count = timeline->noteCount();
        // To a tenth of a millisecond, as the anchors that a drag moves
        const auto rounded = [](double value) { return std::round(value * 10) / 10; };

        auto transaction = impl.session->transaction(tr("Crossfade Envelopes"));
        for (const int index : selectedIndices()) {
            if (timeline->note(index).rest) {
                continue;
            }
            auto anchors = impl.envelopeOf(index).anchorsInTimeOrder();
            const int last = int(anchors.size()) - 1;
            bool changed = false;
            if (index > 0 && !timeline->note(index - 1).rest && timings[index].voiceOverlap > 0) {
                const double overlap = rounded(timings[index].voiceOverlap);
                if (crossfade == CrossfadeP2P3) {
                    anchors[0].x = 0;
                    anchors[1].x = overlap;
                } else {
                    anchors[0].x = overlap;
                    anchors[0].y = anchors[1].y;
                    anchors[1].x = 5;
                }
                changed = true;
            }
            if (index + 1 < count && !timeline->note(index + 1).rest &&
                timings[index + 1].voiceOverlap > 0) {
                const double overlap = rounded(timings[index + 1].voiceOverlap);
                if (crossfade == CrossfadeP2P3) {
                    anchors[last].x = 0;
                    anchors[last - 1].x = overlap;
                } else {
                    anchors[last].x = overlap;
                    anchors[last].y = anchors[last - 1].y;
                    anchors[last - 1].x = 5;
                }
                changed = true;
            }
            if (!changed) {
                continue;
            }
            if (anchors.size() == 5) {
                anchors.removeAt(2);
            }
            kit::ProjectEdits::setEnvelope({refs.at(index)}, kit::Envelope::fromTimeOrder(anchors),
                                           diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::copySelected() {
        stdc_impl_t;
        const auto indices = selectedIndices();
        if (indices.isEmpty()) {
            return false;
        }
        const auto refs = impl.notes();
        QJsonArray notes;
        for (const int index : indices) {
            notes.append(refs.at(index).toNote().toJson());
        }
        const auto bytes = QJsonDocument(QJsonObject{
                                             {QLatin1String("notes"), notes}
        })
                               .toJson(QJsonDocument::Compact);
        auto data = new QMimeData();
        data->setData(QLatin1String(PianoRollState::NotesMimeType), bytes);
        data->setText(QString::fromUtf8(bytes));
        QGuiApplication::clipboard()->setMimeData(data);
        return true;
    }

    QList<kit::Note> PianoRoll::copiedNotes() {
        const auto data = QGuiApplication::clipboard()->mimeData();
        if (!data || !data->hasFormat(QLatin1String(PianoRollState::NotesMimeType))) {
            return {};
        }
        const auto document =
            QJsonDocument::fromJson(data->data(QLatin1String(PianoRollState::NotesMimeType)));
        QList<kit::Note> notes;
        kit::DiagnosticList diagnostics;
        for (const auto &value : document.object().value(QLatin1String("notes")).toArray()) {
            if (const auto note = kit::Note::fromJson(value.toObject(), diagnostics)) {
                notes.push_back(*note);
            }
        }
        return notes;
    }

    bool PianoRoll::pasteParameters(Parameters parameters, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        QList<kit::Note> sources;
        for (const auto &note : copiedNotes()) {
            if (!note.isRest()) {
                sources.push_back(note);
            }
        }
        QList<int> targets;
        for (const int index : selectedIndices()) {
            if (!impl.timeline->note(index).rest) {
                targets.push_back(index);
            }
        }
        if (sources.isEmpty() || targets.isEmpty() || !parameters) {
            return true;
        }

        const auto refs = impl.notes();
        const bool one = sources.size() == 1;
        const auto count = one ? targets.size() : std::min(sources.size(), targets.size());
        auto transaction = impl.session->transaction(tr("Paste Parameters"));
        for (qsizetype k = 0; k < count; ++k) {
            const auto &source = sources[one ? 0 : k];
            const auto target = refs.at(targets[k]);
            if (parameters & PortamentoParameter) {
                kit::ProjectEdits::setPortamento(target, source.portamento, diagnostics);
            }
            if (parameters & VibratoParameter) {
                kit::ProjectEdits::setVibrato({target}, source.vibrato, diagnostics);
            }
            if (parameters & EnvelopeParameter) {
                kit::ProjectEdits::setEnvelope({target}, source.envelope, diagnostics);
            }
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::resetParameters(Parameters parameters, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        // The selected notes, those of the selected points, or all
        auto indices = selectedIndices();
        if (indices.isEmpty()) {
            indices = impl.selectedPointIndices().keys();
        }
        if (indices.isEmpty()) {
            indices = impl.identityOrder();
        }
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        for (const int index : std::as_const(indices)) {
            if (!impl.timeline->note(index).rest) {
                sung.push_back(refs.at(index));
            }
        }
        if (sung.isEmpty() || !parameters) {
            return true;
        }

        auto transaction = impl.session->transaction(tr("Reset Parameters"));
        if (parameters & PortamentoParameter) {
            for (const auto &note : std::as_const(sung)) {
                kit::ProjectEdits::setPortamento(note, {}, diagnostics);
            }
        }
        if (parameters & VibratoParameter) {
            kit::ProjectEdits::setVibrato(sung, std::nullopt, diagnostics);
        }
        if (parameters & EnvelopeParameter) {
            kit::ProjectEdits::setEnvelope(sung, std::nullopt, diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::scalePitch(double portamento, double vibrato,
                               kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto refs = impl.notes();
        QList<kit::NoteRef> sung;
        for (const int index : selectedIndices()) {
            if (!impl.timeline->note(index).rest) {
                sung.push_back(refs.at(index));
            }
        }
        return kit::ProjectEdits::scalePitch(sung, portamento, vibrato, diagnostics);
    }

    bool PianoRoll::convertPitchToMode1(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto timeline = impl.timeline;
        const int count = timeline->noteCount();
        const auto refs = impl.notes();
        QList<kit::Note> notes;
        for (int i = 0; i < count; ++i) {
            notes.push_back(refs.at(i).toNote());
        }
        const auto &timings = impl.sampleTimings();

        auto transaction = impl.session->transaction(tr("Convert Mode2 Pitch to Mode1"));
        for (const int index : selectedIndices()) {
            if (timeline->note(index).rest) {
                continue;
            }
            kit::PitchCurve::Timing timing;
            timing.preUtterance = timings[index].preUtterance;
            timing.startPoint = timings[index].startPoint;
            if (index + 1 < count) {
                timing.nextPreUtterance = timings[index + 1].preUtterance;
                timing.nextOverlap = timings[index + 1].voiceOverlap;
            }
            const kit::PitchCurve curve(notes, index, timeline->tempoMap().tempo(index));
            kit::ProjectEdits::setPitchBend(refs.at(index), curve.toMode1(timing), diagnostics);
        }
        return transaction.commit(diagnostics);
    }

    bool PianoRoll::transposeSelected(int semitones, kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto notes = impl.notes();
        QList<kit::NoteRef> refs;
        for (const int index : selectedIndices()) {
            refs.push_back(notes.at(index));
        }
        return kit::ProjectEdits::transpose(refs, semitones, diagnostics);
    }

    bool PianoRoll::insertNote(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        return impl.insert(QString::fromLatin1(kit::defaultLyric), diagnostics);
    }

    bool PianoRoll::insertRest(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        return impl.insert(QStringLiteral("R"), diagnostics);
    }

    bool PianoRoll::combineSelected(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto indices = selectedIndices();
        if (indices.size() < 2) {
            return true;
        }
        const auto notes = impl.notes();
        const int first = indices.first();
        if (!kit::ProjectEdits::combineNotes(notes, first, indices.last() - first + 1,
                                             diagnostics)) {
            return false;
        }
        impl.anchor = notes.at(first).id();
        impl.setSelection({impl.anchor});
        return true;
    }

    bool PianoRoll::pasteNotes(kit::DiagnosticList &diagnostics) {
        stdc_impl_t;
        const auto copied = copiedNotes();
        if (copied.isEmpty()) {
            return true;
        }
        const auto indices = selectedIndices();
        const int index = indices.isEmpty() ? impl.timeline->noteCount() : indices.first();
        const auto notes = impl.notes();
        auto transaction = impl.session->transaction(tr("Paste"));
        kit::ProjectEdits::insertNotes(notes, index, copied, diagnostics);
        QSet<kit::edit::NodeId> pasted;
        for (int i = 0; i < copied.size(); ++i) {
            pasted.insert(notes.at(index + i).id());
        }
        const auto first = notes.at(index).id();
        if (!transaction.commit(diagnostics)) {
            return false;
        }
        impl.anchor = first;
        impl.setSelection(pasted);
        return true;
    }

    void PianoRoll::editLyric(int index) {
        stdc_impl_t;
        impl.startEditing(index);
    }

    QLineEdit *PianoRoll::lyricEditor() const {
        stdc_impl_t;
        return impl.editor;
    }

    std::optional<double> PianoRoll::playheadPosition() const {
        stdc_impl_t;
        return impl.playhead;
    }

    void PianoRoll::setPlayheadPosition(std::optional<double> tick) {
        stdc_impl_t;
        if (tick == impl.playhead) {
            return;
        }
        impl.playhead = tick;
        if (tick) {
            // A page at a time, so that the view does not move under the pointer continuously
            auto time = impl.view->timeAxis();
            const double width = impl.view->viewport()->width();
            const double x = time.toX(*tick);
            if (x < 0 || x > width) {
                time.left = *tick - width / time.pixelsPerTick * PianoRollState::FollowMargin;
                impl.view->setTimeAxis(time);
            }
        }
        impl.view->viewport()->update();
    }

    double PianoRoll::cursorPosition() const {
        stdc_impl_t;
        return impl.cursor;
    }

    void PianoRoll::setCursorPosition(double tick) {
        stdc_impl_t;
        tick = std::max(0.0, tick);
        if (tick == impl.cursor) {
            return;
        }
        impl.cursor = tick;
        impl.view->viewport()->update();
    }

    QList<PianoRoll::RenderState> PianoRoll::renderStates() const {
        stdc_impl_t;
        return impl.renderStates;
    }

    void PianoRoll::setRenderStates(const QList<RenderState> &states) {
        stdc_impl_t;
        if (states == impl.renderStates) {
            return;
        }
        impl.renderStates = states;
        impl.updateRenderSpans();
    }

    bool PianoRoll::isCursorEnabled() const {
        stdc_impl_t;
        return impl.cursorEnabled;
    }

    void PianoRoll::setCursorEnabled(bool enabled) {
        stdc_impl_t;
        impl.cursorEnabled = enabled;
        impl.view->viewport()->update();
    }

    bool PianoRoll::isPitchVisible() const {
        stdc_impl_t;
        return impl.pitchVisible;
    }

    void PianoRoll::setPitchVisible(bool visible) {
        stdc_impl_t;
        impl.pitchVisible = visible;
        impl.view->viewport()->update();
    }

    bool PianoRoll::isRenderedPitchVisible() const {
        stdc_impl_t;
        return impl.renderedPitchVisible;
    }

    void PianoRoll::setRenderedPitchVisible(bool visible) {
        stdc_impl_t;
        impl.renderedPitchVisible = visible;
        impl.view->viewport()->update();
    }

    bool PianoRoll::areEnvelopesVisible() const {
        stdc_impl_t;
        return impl.envelopesVisible;
    }

    void PianoRoll::setEnvelopesVisible(bool visible) {
        stdc_impl_t;
        impl.envelopesVisible = visible;
        impl.hoveredEnvelope = -1;
        impl.view->viewport()->update();
    }

    bool PianoRoll::areParametersVisible() const {
        stdc_impl_t;
        return impl.parametersVisible;
    }

    void PianoRoll::setParametersVisible(bool visible) {
        stdc_impl_t;
        impl.parametersVisible = visible;
        impl.view->viewport()->update();
    }

    double PianoRoll::pointGrip() const {
        stdc_impl_t;
        return impl.pointGrip;
    }

    void PianoRoll::setPointGrip(double pixels) {
        stdc_impl_t;
        impl.pointGrip = std::max(0.0, pixels);
    }

    double PianoRoll::curveGrip() const {
        stdc_impl_t;
        return impl.curveGrip;
    }

    void PianoRoll::setCurveGrip(double pixels) {
        stdc_impl_t;
        impl.curveGrip = std::max(0.0, pixels);
    }

    void PianoRoll::keyPressEvent(QKeyEvent *event) {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            event->modifiers() == Qt::NoModifier) {
            const auto indices = selectedIndices();
            if (!indices.isEmpty()) {
                editLyric(indices.first());
                event->accept();
                return;
            }
        }
        QWidget::keyPressEvent(event);
    }

    QColor PianoRoll::noteColor() const {
        stdc_impl_t;
        return impl.noteColor.isValid() ? impl.noteColor
                                        : palette().color(QPalette::Active, QPalette::Highlight);
    }

    void PianoRoll::setNoteColor(const QColor &color) {
        stdc_impl_t;
        impl.noteColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::restColor() const {
        stdc_impl_t;
        if (impl.restColor.isValid()) {
            return impl.restColor;
        }
        auto color = palette().color(QPalette::Active, QPalette::Mid);
        color.setAlphaF(0.4f);
        return color;
    }

    void PianoRoll::setRestColor(const QColor &color) {
        stdc_impl_t;
        impl.restColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::lyricColor() const {
        stdc_impl_t;
        return impl.lyricColor.isValid()
                   ? impl.lyricColor
                   : palette().color(QPalette::Active, QPalette::HighlightedText);
    }

    void PianoRoll::setLyricColor(const QColor &color) {
        stdc_impl_t;
        impl.lyricColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::unsampledColor() const {
        stdc_impl_t;
        return impl.unsampledColor.isValid()
                   ? impl.unsampledColor
                   : palette().color(QPalette::Active, QPalette::Highlight);
    }

    void PianoRoll::setUnsampledColor(const QColor &color) {
        stdc_impl_t;
        impl.unsampledColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::unsampledLyricColor() const {
        stdc_impl_t;
        return impl.unsampledLyricColor.isValid()
                   ? impl.unsampledLyricColor
                   : palette().color(QPalette::Active, QPalette::Text);
    }

    void PianoRoll::setUnsampledLyricColor(const QColor &color) {
        stdc_impl_t;
        impl.unsampledLyricColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::selectionColor() const {
        stdc_impl_t;
        return impl.selectionColor.isValid()
                   ? impl.selectionColor
                   : palette().color(QPalette::Active, QPalette::WindowText);
    }

    void PianoRoll::setSelectionColor(const QColor &color) {
        stdc_impl_t;
        impl.selectionColor = color;
        impl.view->viewport()->update();
    }

    kit::TextSearch PianoRoll::lyricSearch() const {
        stdc_impl_t;
        return impl.lyricSearch;
    }

    void PianoRoll::setLyricSearch(const kit::TextSearch &search) {
        stdc_impl_t;
        impl.lyricSearch = search;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::findMatchColor() const {
        stdc_impl_t;
        return impl.findMatchColor.isValid() ? impl.findMatchColor : QColor(0xff, 0xd8, 0x00);
    }

    void PianoRoll::setFindMatchColor(const QColor &color) {
        stdc_impl_t;
        impl.findMatchColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::findMatchTextColor() const {
        stdc_impl_t;
        return impl.findMatchTextColor.isValid() ? impl.findMatchTextColor : QColor(Qt::black);
    }

    void PianoRoll::setFindMatchTextColor(const QColor &color) {
        stdc_impl_t;
        impl.findMatchTextColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::pitchColor() const {
        stdc_impl_t;
        return impl.pitchColor.isValid() ? impl.pitchColor
                                         : palette().color(QPalette::Active, QPalette::Text);
    }

    void PianoRoll::setPitchColor(const QColor &color) {
        stdc_impl_t;
        impl.pitchColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::renderedPitchColor() const {
        stdc_impl_t;
        if (impl.renderedPitchColor.isValid()) {
            return impl.renderedPitchColor;
        }
        auto color = pitchColor();
        color.setAlphaF(color.alphaF() * 0.5f);
        return color;
    }

    void PianoRoll::setRenderedPitchColor(const QColor &color) {
        stdc_impl_t;
        impl.renderedPitchColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::vibratoColor() const {
        stdc_impl_t;
        if (impl.vibratoColor.isValid()) {
            return impl.vibratoColor;
        }
        auto color = palette().color(QPalette::Active, QPalette::Text);
        color.setAlphaF(0.5f);
        return color;
    }

    void PianoRoll::setVibratoColor(const QColor &color) {
        stdc_impl_t;
        impl.vibratoColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::envelopeColor() const {
        stdc_impl_t;
        return impl.envelopeColor.isValid()
                   ? impl.envelopeColor
                   : palette().color(QPalette::Active, QPalette::Highlight);
    }

    void PianoRoll::setEnvelopeColor(const QColor &color) {
        stdc_impl_t;
        impl.envelopeColor = color;
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::intensityBackgroundColor() const {
        stdc_impl_t;
        return impl.intensityBackgroundColor.isValid() ? impl.intensityBackgroundColor
                                                       : whiteRowColor();
    }

    void PianoRoll::setIntensityBackgroundColor(const QColor &color) {
        stdc_impl_t;
        impl.intensityBackgroundColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::intensityBorderColor() const {
        stdc_impl_t;
        return impl.intensityBorderColor.isValid()
                   ? impl.intensityBorderColor
                   : palette().color(QPalette::Active, QPalette::Dark);
    }

    void PianoRoll::setIntensityBorderColor(const QColor &color) {
        stdc_impl_t;
        impl.intensityBorderColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::faintPointColor() const {
        stdc_impl_t;
        return impl.faintPointColor.isValid() ? impl.faintPointColor
                                              : palette().color(QPalette::Active, QPalette::Mid);
    }

    void PianoRoll::setFaintPointColor(const QColor &color) {
        stdc_impl_t;
        impl.faintPointColor = color;
        impl.view->viewport()->update();
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::parameterColor() const {
        stdc_impl_t;
        return impl.parameterColor.isValid()
                   ? impl.parameterColor
                   : palette().color(QPalette::Active, QPalette::Highlight);
    }

    void PianoRoll::setParameterColor(const QColor &color) {
        stdc_impl_t;
        impl.parameterColor = color;
        impl.parameters->viewport()->update();
    }

    QColor PianoRoll::playheadColor() const {
        stdc_impl_t;
        return impl.playheadColor.isValid() ? impl.playheadColor
                                            : palette().color(QPalette::Active, QPalette::Link);
    }

    void PianoRoll::setPlayheadColor(const QColor &color) {
        stdc_impl_t;
        impl.playheadColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::renderWaitingColor() const {
        stdc_impl_t;
        const auto &color = impl.renderColors[0];
        return color.isValid() ? color : palette().color(QPalette::Active, QPalette::Mid);
    }

    void PianoRoll::setRenderWaitingColor(const QColor &color) {
        stdc_impl_t;
        impl.renderColorOf(RenderWaiting) = color;
        impl.updateRenderSpans();
    }

    QColor PianoRoll::renderRunningColor() const {
        stdc_impl_t;
        const auto &color = impl.renderColors[1];
        return color.isValid() ? color : QColor(0xE0, 0xA0, 0x30);
    }

    void PianoRoll::setRenderRunningColor(const QColor &color) {
        stdc_impl_t;
        impl.renderColorOf(RenderRunning) = color;
        impl.updateRenderSpans();
    }

    QColor PianoRoll::renderReadyColor() const {
        stdc_impl_t;
        const auto &color = impl.renderColors[2];
        return color.isValid() ? color : QColor(0x4C, 0xAF, 0x50);
    }

    void PianoRoll::setRenderReadyColor(const QColor &color) {
        stdc_impl_t;
        impl.renderColorOf(RenderReady) = color;
        impl.updateRenderSpans();
    }

    QColor PianoRoll::renderFailedColor() const {
        stdc_impl_t;
        const auto &color = impl.renderColors[3];
        return color.isValid() ? color : QColor(0xD0, 0x40, 0x40);
    }

    void PianoRoll::setRenderFailedColor(const QColor &color) {
        stdc_impl_t;
        impl.renderColorOf(RenderFailed) = color;
        impl.updateRenderSpans();
    }

    QColor PianoRoll::whiteRowColor() const {
        stdc_impl_t;
        return impl.whiteRowColor.isValid() ? impl.whiteRowColor
                                            : palette().color(QPalette::Active, QPalette::Base);
    }

    void PianoRoll::setWhiteRowColor(const QColor &color) {
        stdc_impl_t;
        impl.whiteRowColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::blackRowColor() const {
        stdc_impl_t;
        return impl.blackRowColor.isValid()
                   ? impl.blackRowColor
                   : palette().color(QPalette::Active, QPalette::AlternateBase);
    }

    void PianoRoll::setBlackRowColor(const QColor &color) {
        stdc_impl_t;
        impl.blackRowColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::lineColor() const {
        stdc_impl_t;
        if (impl.lineColor.isValid()) {
            return impl.lineColor;
        }
        auto color = palette().color(QPalette::Active, QPalette::Mid);
        color.setAlphaF(0.35f);
        return color;
    }

    void PianoRoll::setLineColor(const QColor &color) {
        stdc_impl_t;
        impl.lineColor = color;
        impl.view->viewport()->update();
    }

    QColor PianoRoll::barLineColor() const {
        stdc_impl_t;
        return impl.barLineColor.isValid() ? impl.barLineColor
                                           : palette().color(QPalette::Active, QPalette::Mid);
    }

    void PianoRoll::setBarLineColor(const QColor &color) {
        stdc_impl_t;
        impl.barLineColor = color;
        impl.view->viewport()->update();
    }

}
