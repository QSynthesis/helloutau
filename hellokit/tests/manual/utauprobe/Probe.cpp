#include "Probe.h"

#include <functional>
#include <utility>

#include <QtCore/QPair>

namespace utauprobe {

    namespace {

        constexpr const char LYRIC[] = "a";
        constexpr int TONE = 60; // C4
        constexpr int LENGTH = 240;

        QString entry(const char *key, const QString &value) {
            return QStringLiteral("%1=%2").arg(QLatin1String(key), value);
        }

        QString entry(const char *key, int value) {
            return QStringLiteral("%1=%2").arg(QLatin1String(key)).arg(value);
        }

    }

    void Probe::note(ProbeNote note) {
        m_notes += std::move(note);
    }

    void Probe::rest(int length) {
        ProbeNote note;
        note.length = length;
        note.lyric = QStringLiteral("R");
        note.asks = QStringLiteral("separator");
        m_notes += note;
    }

    QString Probe::toUst(const Settings &settings) const {
        QStringList lines = {
            QStringLiteral("[#VERSION]"),
            QStringLiteral("UST Version1.2"),
            QStringLiteral("[#SETTING]"),
            QStringLiteral("Tempo=%1").arg(settings.tempo),
            QStringLiteral("Tracks=1"),
            QStringLiteral("ProjectName=%1").arg(settings.name),
            QStringLiteral("VoiceDir=%1").arg(settings.voiceDir),
            QStringLiteral("OutFile=%1").arg(settings.outFile),
            QStringLiteral("CacheDir=%1").arg(settings.cacheDir),
            QStringLiteral("Tool1=wavtool.exe"),
            QStringLiteral("Tool2=resampler.exe"),
            settings.mode2 ? QStringLiteral("Mode2=True") : QStringLiteral("Mode2=False"),
            QStringLiteral("Flags=%1").arg(settings.flags),
        };

        int index = 0;
        for (const ProbeNote &note : m_notes) {
            lines += QStringLiteral("[#%1]").arg(index++, 4, 10, QLatin1Char('0'));
            lines += entry("Length", note.length);
            lines += entry("Lyric", note.lyric);
            lines += entry("NoteNum", TONE);
            // Every note carries this entry, as notes created by UTAU do. A note testing a
            // specific value carries that value instead, in the same position.
            const bool carriesItsOwn =
                !note.entries.isEmpty() &&
                note.entries.first().startsWith(QStringLiteral("PreUtterance="));
            if (!carriesItsOwn) {
                lines += QStringLiteral("PreUtterance=");
            }
            lines += note.entries;
        }
        lines += QStringLiteral("[#TRACKEND]");

        return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
    }

    QString Probe::toManifest() const {
        QStringList lines = {QStringLiteral("index\tlyric\tlength\tasks\tdetail")};
        int index = 0;
        for (const ProbeNote &note : m_notes) {
            lines += QStringLiteral("%1\t%2\t%3\t%4\t%5")
                         .arg(index++)
                         .arg(note.lyric)
                         .arg(note.length)
                         .arg(note.asks, note.detail);
        }
        return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
    }

    // ---------------------------------------------------------------------- the argument probe

    Probe argumentProbe() {
        Probe probe;

        const auto ask = [&probe](const QString &asks, const QString &detail,
                                  const QStringList &entries = {}, int length = LENGTH,
                                  const char *lyric = LYRIC) {
            ProbeNote note;
            note.length = length;
            note.lyric = QLatin1String(lyric);
            note.entries = entries;
            note.asks = asks;
            note.detail = detail;
            probe.note(note);
        };

        // ------------------------------------------------------------------ flags
        // The order of project flags and note flags, and the result for a key set by both. The
        // project uses B0 throughout.
        probe.rest();
        QStringList flags = {QString()};
        for (const char *letter : {"g", "B", "b", "t", "Y", "H", "P", "N", "A", "O", "S", "u",
                                   "e", "E", "M", "W", "C", "D", "F", "Z", "G", "c", "d", "p"}) {
            for (const char *value : {"", "0", "5", "-5", "50", "100"}) {
                flags += QLatin1String(letter) + QLatin1String(value);
            }
        }
        flags += {
            QStringLiteral("ge5"),     QStringLiteral("gE5"),    QStringLiteral("g5e10"),
            QStringLiteral("e10g5"),   QStringLiteral("B50g5"),  QStringLiteral("g5B50"),
            QStringLiteral("g5Y0H0"),  QStringLiteral("B0"),     QStringLiteral("B0g5"),
            QStringLiteral("g5B0"),    QStringLiteral("ee"),     QStringLiteral("eE"),
            QStringLiteral("Ee"),      QStringLiteral("EE"),     QStringLiteral("g5e"),
            QStringLiteral("5e5"),     QStringLiteral("e5e5"),   QStringLiteral("g-5Y0H0B50"),
            QStringLiteral("t10P0N0"), QStringLiteral("u10O10"),
        };
        for (const QString &value : std::as_const(flags)) {
            ask(QStringLiteral("flags"), value,
                value.isEmpty() ? QStringList() : QStringList{entry("Flags", value)});
        }

        // ------------------------------------------------------------------ levels
        // The values UTAU passes if the entry is absent, as in a note created by UTAU.
        probe.rest();
        const auto sweep = [&ask](const char *key, const QString &asks, const QList<int> &values,
                                  bool withAbsent) {
            if (withAbsent) {
                ask(asks, QStringLiteral("absent"));
            }
            for (const int value : values) {
                ask(asks, QString::number(value), {entry(key, value)});
            }
        };
        sweep("Intensity", QStringLiteral("intensity"), {0, 1, 50, 100, 150, 200}, true);
        sweep("Modulation", QStringLiteral("modulation"), {0, 1, 50, 100, 150, 200, -100}, true);
        sweep("Velocity", QStringLiteral("velocity"), {0, 50, 100, 150, 200}, true);

        // ------------------------------------------------------------------ envelope
        probe.rest();
        ask(QStringLiteral("envelope"), QStringLiteral("absent"));
        for (const char *envelope :
             {"0,5,35,0,100,100,0", "0,5,35,0,100,100,0,0", "0,5,35,0,100,100,0,10",
              "0,5,35,0,100,100,0,%,20,30", "0,5,35,0,100,100,0,10,%,20,30", "1,2,3,4,5,6,7",
              "0,0,0,0,0,0,0", "10,20,30,40,50,60,70,80"}) {
            ask(QStringLiteral("envelope"), QLatin1String(envelope),
                {entry("Envelope", QLatin1String(envelope))});
        }

        // ------------------------------------------------------------------ timing overrides
        probe.rest();
        sweep("PreUtterance", QStringLiteral("preUtterance"), {0, 10, 50, 100, 200}, true);
        sweep("VoiceOverlap", QStringLiteral("voiceOverlap"), {0, 5, 20, 50, 100}, true);
        sweep("StartPoint", QStringLiteral("startPoint"), {0, 10, 50, 100, 300}, true);

        // ------------------------------------------------------------------ length
        // The length determines realLength.
        probe.rest();
        QList<int> lengths;
        for (int ticks = 15; ticks <= 480; ticks += 15) {
            lengths += ticks;
        }
        lengths +=
            {520, 560, 600, 660, 720, 800, 840, 900, 960, 1080, 1200, 1320, 1440, 1680, 1920};
        for (const int ticks : std::as_const(lengths)) {
            ask(QStringLiteral("length"), QString::number(ticks), {}, ticks);
        }

        // ------------------------------------------------------------------ pitch curves
        probe.rest();
        ask(QStringLiteral("pitch"), QStringLiteral("absent"));
        for (const char *start :
             {"-40", "-40;0", "-40;20", "-40;-20", "0", "-100", "-10", "-200"}) {
            for (const char *width : {"40", "80", "160", "20,20", "40,40", "80,80"}) {
                ask(QStringLiteral("pitch"),
                    QStringLiteral("PBS=%1 PBW=%2").arg(QLatin1String(start), QLatin1String(width)),
                    {entry("PBS", QLatin1String(start)), entry("PBW", QLatin1String(width))});
            }
        }
        for (const char *y : {"10", "-10", "50", "-50", "0", "100"}) {
            ask(QStringLiteral("pitch"), QStringLiteral("PBY=%1").arg(QLatin1String(y)),
                {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("40,40")),
                 entry("PBY", QLatin1String(y))});
        }
        for (const char *mode : {",s", ",r", ",j", "s,s", "r,r", "j,j", ",", "s,"}) {
            ask(QStringLiteral("pitch"), QStringLiteral("PBM=%1").arg(QLatin1String(mode)),
                {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("40,40")),
                 entry("PBY", QStringLiteral("10")), entry("PBM", QLatin1String(mode))});
        }
        for (const char *mode : {",s,r", ",j,s", ",,", "s,r,j"}) {
            ask(QStringLiteral("pitch"),
                QStringLiteral("three segments PBM=%1").arg(QLatin1String(mode)),
                {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("30,30,30")),
                 entry("PBY", QStringLiteral("10,-10")), entry("PBM", QLatin1String(mode))});
        }
        ask(QStringLiteral("pitch"), QStringLiteral("four segments"),
            {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("20,20,20,20")),
             entry("PBY", QStringLiteral("5,-5,5"))});
        ask(QStringLiteral("pitch"), QStringLiteral("six segments"),
            {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("10,10,10,10,10,10")),
             entry("PBY", QStringLiteral("5,-5,5,-5,5"))});

        // ------------------------------------------------------------------ vibrato
        probe.rest();
        ask(QStringLiteral("vibrato"), QStringLiteral("absent"));
        for (const int share : {20, 65, 100}) {
            for (const int period : {60, 180, 500}) {
                for (const int amplitude : {10, 35, 100}) {
                    for (const auto &shape : QList<QList<int>>{
                             {20, 20, 0,  0 },
                             {0,  0,  0,  0 },
                             {50, 50, 50, 50}
                    }) {
                        const QString value = QStringLiteral("%1,%2,%3,%4,%5,%6,%7,0")
                                                  .arg(share)
                                                  .arg(period)
                                                  .arg(amplitude)
                                                  .arg(shape.at(0))
                                                  .arg(shape.at(1))
                                                  .arg(shape.at(2))
                                                  .arg(shape.at(3));
                        ask(QStringLiteral("vibrato"), value, {entry("VBR", value)});
                    }
                }
            }
        }

        // ------------------------------------------------------------------ the cache name
        // Identical notes in identical context. If the six characters at the end of the cache
        // name encode the arguments, these notes must yield the same characters.
        probe.rest();
        for (int i = 0; i < 6; ++i) {
            ask(QStringLiteral("cacheIdentical"), QStringLiteral("alike"));
        }
        probe.rest();
        // The same note with one property changed at a time, to determine what the name depends
        // on.
        ask(QStringLiteral("cacheVaried"), QStringLiteral("nothing"));
        ask(QStringLiteral("cacheVaried"), QStringLiteral("intensity"), {entry("Intensity", 90)});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("modulation"), {entry("Modulation", 10)});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("velocity"), {entry("Velocity", 90)});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("flags"),
            {entry("Flags", QStringLiteral("g5"))});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("envelope"),
            {entry("Envelope", QStringLiteral("0,5,36,0,100,100,0"))});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("pitch"),
            {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("80"))});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("vibrato"),
            {entry("VBR", QStringLiteral("65,180,35,20,20,0,0,0"))});
        ask(QStringLiteral("cacheVaried"), QStringLiteral("startPoint"), {entry("StartPoint", 10)});

        // ------------------------------------------------------------------ tempo changes
        probe.rest();
        ask(QStringLiteral("tempo"), QStringLiteral("absent"));
        for (const int tempo : {60, 90, 134, 180, 240, 120}) {
            ask(QStringLiteral("tempo"), QString::number(tempo), {entry("Tempo", tempo)});
        }

        // ------------------------------------------------------------------ rests
        probe.rest();
        for (const int ticks : {60, 120, 240, 480, 960}) {
            ask(QStringLiteral("rest"), QString::number(ticks), {}, ticks, "R");
            ask(QStringLiteral("afterRest"), QString::number(ticks));
        }

        probe.rest();
        return probe;
    }

    // ----------------------------------------------------------------------- the vibrato probe

    Probe vibratoProbe() {
        Probe probe;

        const auto vbr = [](int share, int period = 180, int amplitude = 100, int easeIn = 0,
                            int easeOut = 0, int phase = 0, int offset = 0) {
            return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,0")
                .arg(share)
                .arg(period)
                .arg(amplitude)
                .arg(easeIn)
                .arg(easeOut)
                .arg(phase)
                .arg(offset);
        };

        // Every note is surrounded by rests, so that its curve is unaffected by neighbors. In the
        // argument probe the values before the start of a note were influenced by the preceding
        // note, which made short vibratos difficult to evaluate.
        const auto alone = [&probe](const QString &asks, const QString &detail,
                                    const QStringList &entries, int length = 480) {
            probe.rest();
            ProbeNote note;
            note.length = length;
            note.lyric = QLatin1String(LYRIC);
            note.entries = entries;
            note.asks = asks;
            note.detail = detail;
            probe.note(note);
        };

        // -------------------------------------------------------------- coverage
        // 480 ticks are 500 ms at 120 bpm, so a percentage of the note corresponds to five times
        // that many milliseconds.
        for (int share = 2; share <= 100; share += 2) {
            alone(QStringLiteral("share"), QStringLiteral("%1% = %2 ms").arg(share).arg(5 * share),
                  {entry("VBR", vbr(share))});
        }

        // -------------------------------------------------------------- same share, longer notes
        for (const int length : {120, 180, 240, 360, 480, 720, 960, 1440, 1920, 2880}) {
            alone(QStringLiteral("ticks"),
                  QStringLiteral("%1 ticks, vibrato over %2").arg(length).arg(length / 5),
                  {entry("VBR", vbr(20))}, length);
        }

        // -------------------------------------------------------------- same note, three tempos
        // The vibrato covers the same ticks at every tempo, while the milliseconds vary by a
        // factor of four. If the threshold is defined in ticks these notes agree, and if it is
        // defined in milliseconds they do not.
        for (const int tempo : {60, 120, 240}) {
            for (const int share : {20, 40, 65}) {
                alone(QStringLiteral("tempo"), QStringLiteral("%1 bpm, %2%").arg(tempo).arg(share),
                      {entry("Tempo", tempo), entry("VBR", vbr(share))});
            }
        }
        // A tempo set on one note applies to every following note, so it must be reset.
        // Otherwise every subsequent block would test its question at 240 bpm instead of 120.
        alone(QStringLiteral("tempo"), QStringLiteral("back to 120 bpm"),
              {entry("Tempo", 120), entry("VBR", vbr(100))});

        // -------------------------------------------------------------- exact threshold
        // The threshold lies between 50 and 60 ms. These notes determine it to the millisecond
        // with two sweeps: a share of a short note and a share of a longer one, arranged so
        // that one step of note length equals one millisecond of vibrato. If both sweeps change
        // at the same point, the threshold depends only on the vibrato length.
        //
        // At 120 bpm a tick is 25/24 ms, so 24% of a note changes by one millisecond every four
        // ticks, and 12% of a note every eight ticks.
        for (int ticks = 180; ticks <= 260; ticks += 4) {
            alone(QStringLiteral("boundary"),
                  QStringLiteral("24% of %1 ticks = %2 ms").arg(ticks).arg(ticks / 4),
                  {entry("VBR", vbr(24))}, ticks);
        }
        for (int ticks = 360; ticks <= 520; ticks += 8) {
            alone(QStringLiteral("boundary"),
                  QStringLiteral("12% of %1 ticks = %2 ms").arg(ticks).arg(ticks / 8),
                  {entry("VBR", vbr(12))}, ticks);
        }

        // -------------------------------------------------------------- fade in and fade out
        for (const int share : {20, 30, 40, 50, 65, 100}) {
            for (const auto &fade : QList<QPair<int, int>>{
                     {0,  0 },
                     {20, 20},
                     {50, 50},
                     {80, 80}
            }) {
                alone(QStringLiteral("fade"),
                      QStringLiteral("%1%, fade %2/%3").arg(share).arg(fade.first).arg(fade.second),
                      {entry("VBR", vbr(share, 180, 100, fade.first, fade.second))});
            }
        }

        // -------------------------------------------------------------- period at a short share
        for (const int period : {20, 60, 120, 180, 300, 500, 1000}) {
            alone(QStringLiteral("period"), QStringLiteral("20%, period %1 ms").arg(period),
                  {entry("VBR", vbr(20, period))});
        }

        // -------------------------------------------------------------- transition
        // The tail of a note is bent by the initial pitch line of the next note. The first note
        // of each pair has no pitch line, so its tail consists only of the transition, and the
        // second note starts far enough away to make the difference measurable.
        for (const int start : {10, 20, 40, 80, 160}) {
            for (const int width : {20, 40, 80}) {
                const QString detail = QStringLiteral("PBS=-%1 PBW=%2").arg(start).arg(width);
                probe.rest();
                ProbeNote before;
                before.lyric = QLatin1String(LYRIC);
                before.asks = QStringLiteral("handover");
                before.detail = detail + QStringLiteral(", the note before");
                probe.note(before);

                ProbeNote after;
                after.lyric = QLatin1String(LYRIC);
                after.entries = {entry("PBS", QStringLiteral("-%1").arg(start)),
                                 entry("PBW", QStringLiteral("%1,%1").arg(width)),
                                 entry("PBY", QStringLiteral("100"))};
                after.asks = QStringLiteral("handover");
                after.detail = detail + QStringLiteral(", the note after");
                probe.note(after);
            }
        }

        // -------------------------------------------------------------- scope of the omission
        // A vibrato below the threshold is not drawn. The question is whether the note loses
        // only its vibrato or its entire pitch curve: if the pitch line remains, the rule
        // applies to the vibrato, and if it disappears as well, the rule applies to the note.
        for (const int ticks : {180, 400}) {
            const QString how =
                ticks == 180 ? QStringLiteral("too short") : QStringLiteral("long enough");
            alone(QStringLiteral("dropped"), QStringLiteral("%1, vibrato alone").arg(how),
                  {entry("VBR", vbr(24))}, ticks);
            alone(QStringLiteral("dropped"), QStringLiteral("%1, line alone").arg(how),
                  {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("40,40")),
                   entry("PBY", QStringLiteral("100"))},
                  ticks);
            alone(QStringLiteral("dropped"), QStringLiteral("%1, both").arg(how),
                  {entry("PBS", QStringLiteral("-40")), entry("PBW", QStringLiteral("40,40")),
                   entry("PBY", QStringLiteral("100")), entry("VBR", vbr(24))},
                  ticks);
        }

        // -------------------------------------------------------------- carry-over
        // In the 455-note probe, a vibrato below the threshold still appeared, without fade, in
        // the first two values of the *next* note. Every note there had its own vibrato, so here
        // a plain note follows each one: any deviation at its start originates from the
        // preceding note alone.
        for (const auto &shape : QList<QPair<QString, QString>>{
                 {QStringLiteral("too short"), vbr(24, 180, 100)},
                 {QStringLiteral("too short, faded"), vbr(24, 180, 100, 50, 50, 50, 50)},
                 {QStringLiteral("long enough"), vbr(100, 180, 100)},
                 {QStringLiteral("long enough, faded"), vbr(100, 180, 100, 50, 50, 50, 50)},
        }) {
            probe.rest();
            ProbeNote before;
            before.length = 180;
            before.lyric = QLatin1String(LYRIC);
            before.entries = {entry("VBR", shape.second)};
            before.asks = QStringLiteral("leak");
            before.detail = shape.first + QStringLiteral(", the note with the vibrato");
            probe.note(before);

            ProbeNote after;
            after.length = 480;
            after.lyric = QLatin1String(LYRIC);
            after.asks = QStringLiteral("leak");
            after.detail = shape.first + QStringLiteral(", the plain note after it");
            probe.note(after);
        }

        // -------------------------------------------------------------- offset dependency
        // The transition is delayed by a third of a millisecond, identically at 120 and 240 bpm.
        // These notes vary the remaining candidate causes: the length of the preceding note and
        // its tempo.
        for (const int length : {180, 240, 360, 480, 960}) {
            probe.rest();
            ProbeNote before;
            before.length = length;
            before.lyric = QLatin1String(LYRIC);
            before.asks = QStringLiteral("offset");
            before.detail = QStringLiteral("%1 ticks at 120 bpm, the note before").arg(length);
            probe.note(before);

            ProbeNote after;
            after.lyric = QLatin1String(LYRIC);
            after.entries = {entry("PBS", QStringLiteral("-20")),
                             entry("PBW", QStringLiteral("20,20")),
                             entry("PBY", QStringLiteral("100"))};
            after.asks = QStringLiteral("offset");
            after.detail = QStringLiteral("%1 ticks at 120 bpm, the note after").arg(length);
            probe.note(after);
        }
        for (const int tempo : {60, 240, 120}) {
            probe.rest();
            ProbeNote before;
            before.lyric = QLatin1String(LYRIC);
            before.entries = {entry("Tempo", tempo)};
            before.asks = QStringLiteral("offset");
            before.detail = QStringLiteral("480 ticks at %1 bpm, the note before").arg(tempo);
            probe.note(before);

            ProbeNote after;
            after.lyric = QLatin1String(LYRIC);
            after.entries = {entry("PBS", QStringLiteral("-20")),
                             entry("PBW", QStringLiteral("20,20")),
                             entry("PBY", QStringLiteral("100"))};
            after.asks = QStringLiteral("offset");
            after.detail = QStringLiteral("480 ticks at %1 bpm, the note after").arg(tempo);
            probe.note(after);
        }

        // -------------------------------------------------------------- omitted curve
        // For a few notes UTAU writes three values instead of the usual four: no "!tempo" and no
        // curve, only "0Q" concatenated with the tempo. Two unrelated hypotheses fit the
        // observations, so both are tested.
        //
        // First hypothesis: the pitch line of the note ends before the curve starts. Fifteen
        // transition pairs already support it, and these notes test both sides of the boundary.
        for (const auto &line : QList<QPair<int, int>>{
                 {40,  20},
                 {80,  20},
                 {80,  30},
                 {80,  40},
                 {160, 60},
                 {160, 80},
        }) {
            alone(QStringLiteral("nocurve"),
                  QStringLiteral("line from -%1 by %2, ends at %3")
                      .arg(line.first)
                      .arg(line.second)
                      .arg(2 * line.second - line.first),
                  {entry("PBS", QStringLiteral("-%1").arg(line.first)),
                   entry("PBW", QStringLiteral("%1,%1").arg(line.second)),
                   entry("PBY", QStringLiteral("100"))});
        }

        // Second hypothesis: the curve starts less than one value interval before the note. An
        // interval is five ticks, and the three affected notes of the 455-note probe were all
        // below it: velocity 200 and tempo 60 both yield 4.29 ticks, and a pre-utterance of zero
        // yields none. Velocity 150 and a pre-utterance of 10 exceed it and behaved normally.
        for (const int velocity : {150, 175, 190, 200, 250}) {
            alone(QStringLiteral("nocurve"), QStringLiteral("velocity %1").arg(velocity),
                  {entry("Velocity", velocity)});
        }
        for (const int pre : {0, 2, 4, 5, 6, 8, 10}) {
            alone(QStringLiteral("nocurve"), QStringLiteral("pre-utterance %1 ms").arg(pre),
                  {entry("PreUtterance", pre)});
        }

        probe.rest();
        return probe;
    }

    // ------------------------------------------------------------------------ the bounds probe

    Probe boundsProbe() {
        Probe probe;
        // Each value alone between rests, as in the vibrato probe
        const auto alone = [&probe](const char *key, const QString &asks, int value) {
            probe.rest();
            ProbeNote note;
            note.length = LENGTH;
            note.lyric = QLatin1String(LYRIC);
            note.entries = {entry(key, value)};
            note.asks = asks;
            note.detail = QString::number(value);
            probe.note(note);
        };
        // Below 0 the editor offers velocity to -100, as QSynthesis does. Velocity=250 is known
        // to fall back to 100.
        for (const int value : {-200, -100, -50, -1, 0, 200, 201, 210, 225, 250, 300, 1000}) {
            alone("Velocity", QStringLiteral("velocity"), value);
        }
        for (const int value : {-100, -1, 0, 200, 201, 250, 500}) {
            alone("Intensity", QStringLiteral("intensity"), value);
        }
        for (const int value : {-300, -201, -200, 200, 201, 300}) {
            alone("Modulation", QStringLiteral("modulation"), value);
        }
        probe.rest();
        return probe;
    }

    // ------------------------------------------------------------------------- the Mode1 probe

    Probe mode1Probe() {
        Probe probe;
        const auto values = [](int count, const std::function<QString(int)> &at) {
            QStringList out;
            for (int i = 0; i < count; ++i) {
                out += at(i);
            }
            return out.join(QLatin1Char(','));
        };
        // A note of 480 ticks holds 96 values at one per 5 ticks.
        const auto alone = [&probe](const QString &asks, const QString &detail,
                                    const QStringList &entries) {
            probe.rest();
            ProbeNote note;
            note.length = 480;
            note.lyric = QLatin1String(LYRIC);
            note.entries = entries;
            note.asks = asks;
            note.detail = detail;
            probe.note(note);
        };
        const auto bend = [](const QString &start, const QString &pitches) {
            return QStringList{QStringLiteral("PBType=5"), entry("PBStart", start),
                               entry("PitchBend", pitches)};
        };
        const auto ramp = values(96, [](int i) { return QString::number(i * 2); });

        alone(QStringLiteral("mode1"), QStringLiteral("absent"), {});
        alone(QStringLiteral("mode1"), QStringLiteral("96 values of 50, PBStart 0"),
              bend(QStringLiteral("0"), values(96, [](int) { return QStringLiteral("50"); })));
        alone(QStringLiteral("mode1"), QStringLiteral("ramp 0 to 190 by 2, PBStart 0"),
              bend(QStringLiteral("0"), ramp));
        // The unit and the sign of PBStart
        for (const char *start : {"-50", "-20.5", "20", "50"}) {
            alone(QStringLiteral("pbstart"),
                  QStringLiteral("ramp, PBStart %1").arg(QLatin1String(start)),
                  bend(QLatin1String(start), ramp));
        }
        // What follows the end of the values, and what the values beyond the note do
        alone(QStringLiteral("extent"), QStringLiteral("10 values of 100, then none"),
              bend(QStringLiteral("0"), values(10, [](int) { return QStringLiteral("100"); })));
        alone(QStringLiteral("extent"), QStringLiteral("150 values of 100, beyond the note"),
              bend(QStringLiteral("0"), values(150, [](int) { return QStringLiteral("100"); })));
        alone(QStringLiteral("precision"), QStringLiteral("96 values of 10.5"),
              bend(QStringLiteral("0"), values(96, [](int) { return QStringLiteral("10.5"); })));
        alone(QStringLiteral("precision"), QStringLiteral("alternating 300 and -300"),
              bend(QStringLiteral("0"),
                   values(96, [](int i) { return QString::number(i % 2 ? -300 : 300); })));
        // With a vibrato
        const auto vibrato = QStringLiteral("VBR=65,180,35,20,20,0,0,0");
        alone(QStringLiteral("vibrato"), QStringLiteral("vibrato alone"), {vibrato});
        alone(QStringLiteral("vibrato"), QStringLiteral("values of 0 and a vibrato"),
              bend(QStringLiteral("0"), values(96, [](int) { return QStringLiteral("0"); })) +
                  QStringList{vibrato});
        alone(QStringLiteral("vibrato"), QStringLiteral("ramp and a vibrato"),
              bend(QStringLiteral("0"), ramp) + QStringList{vibrato});
        // Data of both modes, and of Mode2 alone, which the setting of the project selects
        const QStringList points = {QStringLiteral("PBS=-40;0"), QStringLiteral("PBW=40,40"),
                                    QStringLiteral("PBY=50")};
        alone(QStringLiteral("both"), QStringLiteral("Mode2 points alone"), points);
        alone(QStringLiteral("both"), QStringLiteral("ramp and Mode2 points"),
              bend(QStringLiteral("0"), ramp) + points);
        // A note after a note with values, which may carry into it as a Mode2 curve does
        alone(QStringLiteral("carry"), QStringLiteral("ramp"), bend(QStringLiteral("0"), ramp));
        ProbeNote next;
        next.length = 480;
        next.lyric = QLatin1String(LYRIC);
        next.asks = QStringLiteral("carry");
        next.detail = QStringLiteral("the note after, absent");
        probe.note(next);
        probe.rest();
        return probe;
    }

    // -------------------------------------------------------------------------- the save probe

    Probe saveProbe() {
        Probe probe;
        const auto alone = [&probe](const QString &asks, const QString &detail,
                                    const QStringList &entries) {
            probe.rest();
            ProbeNote note;
            note.length = LENGTH;
            note.lyric = QLatin1String(LYRIC);
            note.entries = entries;
            note.asks = asks;
            note.detail = detail;
            probe.note(note);
        };
        alone(QStringLiteral("envelope"), QStringLiteral("absent"), {});
        for (const char *envelope :
             {"0,5,35,0,100,100,0", "0,5,35,0,100,100,0,0", "0,5,35,0,100,100,0,0,0,100",
              "0,5,35,0,100,100", "0,5,35,0,100,100,0,10"}) {
            alone(QStringLiteral("envelope"), QLatin1String(envelope),
                  {entry("Envelope", QLatin1String(envelope))});
        }
        // Points whose last one does not end at the pitch of the note, with each shape
        for (const char *mode : {"", "s", "r", "j"}) {
            QStringList entries = {QStringLiteral("PBS=-40;0"), QStringLiteral("PBW=40,40"),
                                   QStringLiteral("PBY=20,30")};
            if (*mode) {
                entries += entry("PBM", QStringLiteral("%1,%1").arg(QLatin1String(mode)));
            }
            alone(QStringLiteral("points"),
                  QStringLiteral("last point at 30, PBM %1")
                      .arg(*mode ? QLatin1String(mode) : QLatin1String("absent")),
                  entries);
        }
        probe.rest();
        return probe;
    }

}
