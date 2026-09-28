#include "TrackTimeline.h"

#include <algorithm>

#include <stdcorelib/pimpl.h>

#include "ProjectRefs.h"
#include "ProjectSession.h"

namespace hello::kit {

    class TrackTimeline::Impl {
    public:
        using Decl = TrackTimeline;

        Impl(ProjectSession *session, int trackIndex) : session(session), trackIndex(trackIndex) {
        }

        ProjectSession *session;
        int trackIndex;

        // Computed on demand, and marked stale by any change of the session
        mutable bool stale = true;
        mutable QList<Note> notes;
        mutable TempoMap tempoMap;

        void compute() const {
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
            stdc_impl_t;
            if (!impl.stale) {
                impl.stale = true;
                Q_EMIT invalidated();
            }
        });
    }

    TrackTimeline::~TrackTimeline() = default;

    int TrackTimeline::noteCount() const {
        stdc_impl_t;
        impl.compute();
        return int(impl.notes.size());
    }

    const TrackTimeline::Note &TrackTimeline::note(int index) const {
        stdc_impl_t;
        impl.compute();
        return impl.notes.at(index);
    }

    const TempoMap &TrackTimeline::tempoMap() const {
        stdc_impl_t;
        impl.compute();
        return impl.tempoMap;
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
