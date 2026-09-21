#include "Probe.h"

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
            QStringLiteral("Mode2=True"),
            QStringLiteral("Flags=%1").arg(settings.flags),
        };

        int index = 0;
        for (const ProbeNote &note : m_notes) {
            lines += QStringLiteral("[#%1]").arg(index++, 4, 10, QLatin1Char('0'));
            lines += entry("Length", note.length);
            lines += entry("Lyric", note.lyric);
            lines += entry("NoteNum", TONE);
            // Every note carries one, because a note UTAU made itself does. A note asking what
            // a value of its own does carries that instead, in the same place.
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
        // Which of the project's flags and the note's own comes first, and what happens to a key
        // that both of them set. The project carries B0 throughout.
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
        // What UTAU passes where the entry is absent, which is what a note it made itself looks
        // like.
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
        // Which is what decides realLength.
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
        // Notes alike in everything, in alike surroundings. If the six characters at the end of
        // the cache name stand for the arguments, these have to come out the same.
        probe.rest();
        for (int i = 0; i < 6; ++i) {
            ask(QStringLiteral("cacheIdentical"), QStringLiteral("alike"));
        }
        probe.rest();
        // The same note with one thing changed at a time, to see what the name follows.
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

        // Every note stands between rests, so that a note's curve is its own. On the first probe
        // the readings before a note starts carry the note before it, which is what made the
        // short vibratos so hard to read.
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

        // -------------------------------------------------------------- how much it covers
        // 480 ticks is 500 ms at 120 bpm, so a share is also its length in milliseconds times
        // five.
        for (int share = 2; share <= 100; share += 2) {
            alone(QStringLiteral("share"), QStringLiteral("%1% = %2 ms").arg(share).arg(5 * share),
                  {entry("VBR", vbr(share))});
        }

        // -------------------------------------------------------------- the same share, longer
        // notes
        for (const int length : {120, 180, 240, 360, 480, 720, 960, 1440, 1920, 2880}) {
            alone(QStringLiteral("ticks"),
                  QStringLiteral("%1 ticks, vibrato over %2").arg(length).arg(length / 5),
                  {entry("VBR", vbr(20))}, length);
        }

        // -------------------------------------------------------------- the same note, three
        // tempos The ticks the vibrato covers do not move; the milliseconds do, by a factor of
        // four. If the boundary is in ticks these agree, and if it is in milliseconds they do not.
        for (const int tempo : {60, 120, 240}) {
            for (const int share : {20, 40, 65}) {
                alone(QStringLiteral("tempo"), QStringLiteral("%1 bpm, %2%").arg(tempo).arg(share),
                      {entry("Tempo", tempo), entry("VBR", vbr(share))});
            }
        }
        // A tempo set on one note runs on into every note after it, so it has to be set back or
        // every block below this one asks its question at 240 bpm instead of 120. The first run
        // of this probe did exactly that, and two of its blocks answered nothing.
        alone(QStringLiteral("tempo"), QStringLiteral("back to 120 bpm"),
              {entry("Tempo", 120), entry("VBR", vbr(100))});

        // -------------------------------------------------------------- where exactly it stops
        // The first run put the boundary between 50 and 60 ms. These pin it to the millisecond,
        // twice over: a share of a short note and a share of a longer one, arranged so that one
        // step of the note's length is one millisecond of vibrato. If the two sweeps break in
        // the same place, the vibrato's own length is what decides and nothing else is.
        //
        // At 120 bpm a tick is 25/24 ms, so 24% of a note moves by a millisecond every four
        // ticks, and 12% of one every eight.
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

        // -------------------------------------------------------------- the fade, on both sides
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

        // -------------------------------------------------------------- the period, at a short
        // share
        for (const int period : {20, 60, 120, 180, 300, 500, 1000}) {
            alone(QStringLiteral("period"), QStringLiteral("20%, period %1 ms").arg(period),
                  {entry("VBR", vbr(20, period))});
        }

        // -------------------------------------------------------------- the handover
        // A note's tail is bent by the pitch line the next note starts with. The first note of
        // each pair carries no pitch line of its own, so its tail is nothing but the handover,
        // and the second starts far enough away to make the difference legible.
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

        probe.rest();
        return probe;
    }

}
