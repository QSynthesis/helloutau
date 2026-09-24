#include <QtTest/QTest>

#include <hellokit/Edit/EditSession.h>

#include "ProjectSamples.h"

using namespace hello::kit;

class test_EditSession : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Acceptance criterion 1 of docs/Editing.md. The comparison uses the .usth serialization,
    // which covers every field and distinguishes an absent optional field from zero.
    void every_field_survives_the_round_trip_through_the_tree() {
        const auto project = richProject();
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }

    void a_random_project_survives_the_round_trip_through_the_tree() {
        for (quint32 seed = 1; seed <= 20; ++seed) {
            const auto project = randomProject(seed);
            const EditSession session(project);
            QVERIFY2(session.snapshot().toJson() == project.toJson(),
                     qPrintable(QStringLiteral("seed %1").arg(seed)));
        }
    }

    void an_empty_track_survives_the_round_trip_through_the_tree() {
        Project project;
        project.tracks.push_back(Track());
        const EditSession session(project);
        QCOMPARE(session.snapshot().toJson(), project.toJson());
    }
};

QTEST_APPLESS_MAIN(test_EditSession)

#include "test_EditSession.moc"
