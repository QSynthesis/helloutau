#include "TrackTimeline.h"

#include <algorithm>

#include "ProjectRefs.h"
#include "ProjectSession.h"

namespace hello::kit {

    class TrackTimeline::Impl {
    public:
        Impl(ProjectSession *session, int trackIndex)
            : session(session), trackIndex(trackIndex) {
        }

        ProjectSession *session;
        int trackIndex;

        // Computed on demand, and marked stale by any change of the session
        bool stale = true;
        QList<Note> notes;
        TempoMap tempoMap;

        void compute() {
            if (!stale) {
                return;
            }
            stale = false;

            const ProjectRef project(session);
            tempoMap = TempoMap(project.settings().tempo());
            notes.clear();

            const auto tracks = project.tracks();
            if (trackIndex >= tracks.size()) {
                return;
            }
            const auto list = tracks.at(trackIndex).notes();
            notes.reserve(list.size());
            for (int i = 0; i < list.size(); ++i) {
                const auto ref = list.at(i);
                Note note;
                note.id = ref.id();
                note.start = tempoMap.startTick(i);
                note.length = ref.length();
                note.key = ref.noteNum();
                note.lyric = ref.lyric();
                note.rest = kit::Note::isRestLyric(note.lyric);
                note.tempo = ref.tempo();
                tempoMap.append(note.length, note.tempo);
                notes.push_back(std::move(note));
            }
        }
    };

    TrackTimeline::TrackTimeline(ProjectSession *session, int trackIndex, QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(session, trackIndex)) {
        connect(session, &ProjectSession::changed, this, [this] {
            if (!_impl->stale) {
                _impl->stale = true;
                Q_EMIT invalidated();
            }
        });
    }

    TrackTimeline::~TrackTimeline() = default;

    int TrackTimeline::noteCount() const {
        _impl->compute();
        return int(_impl->notes.size());
    }

    const TrackTimeline::Note &TrackTimeline::note(int index) const {
        _impl->compute();
        return _impl->notes.at(index);
    }

    const TempoMap &TrackTimeline::tempoMap() const {
        _impl->compute();
        return _impl->tempoMap;
    }

    qint64 TrackTimeline::length() const {
        const auto &map = tempoMap();
        return map.startTick(map.noteCount());
    }

    int TrackTimeline::noteAt(double tick) const {
        return tempoMap().noteAt(tick);
    }

    std::pair<int, int> TrackTimeline::notesBetween(double first, double last) const {
        const auto &map = tempoMap();
        const int count = map.noteCount();
        const int begin = std::clamp(map.noteAt(first), 0, count);
        const int end = std::clamp(map.noteAt(last) + 1, begin, count);
        return {begin, end};
    }

}
