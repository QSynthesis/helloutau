#ifndef HELLOKIT_TESTS_EDIT_VOICEBANKSAMPLES_H
#define HELLOKIT_TESTS_EDIT_VOICEBANKSAMPLES_H

#include <filesystem>
#include <map>
#include <optional>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtTest/QTest>

#include <hellokit/VoiceBank/VoiceBankDiskState.h>

namespace hello::kit {

    // Voice banks for the tests of the editing layer.

    /// 葛平 in GBK.
    inline const QByteArray kGbkGePing = QByteArray("\xb8\xf0\xc6\xbd", 4);

    /// Selects an encoding per directory, and declines a directory that has none.
    class DirectorySelector : public VoiceBankCharsetSelector {
    public:
        explicit DirectorySelector(std::map<std::filesystem::path, QString> charsets)
            : m_charsets(std::move(charsets)) {
        }

        std::optional<QString> selectCharset(const VoiceBankDirectorySource &directory,
                                             DiagnosticList &) override {
            const auto it = m_charsets.find(directory.path);
            return it == m_charsets.end() ? std::nullopt : std::optional<QString>(it->second);
        }

    private:
        std::map<std::filesystem::path, QString> m_charsets;
    };

    inline bool writeSampleFile(const QString &root, const QString &relative,
                                const QByteArray &bytes) {
        const QString path = root + QLatin1Char('/') + relative;
        QFile file(path);
        return QDir().mkpath(QFileInfo(path).path()) && file.open(QIODevice::WriteOnly) &&
               file.write(bytes) == bytes.size();
    }

    /// Writes into \a root a voice bank with every kind of content that the tree holds, and
    /// opens it: the files of the root, an \c oto.ini that declares its encoding, spellings of
    /// numbers including empty ones, an entry whose audio file is missing, audio files without
    /// an entry, a nested directory \c sub/inner , and the directories \c left and \c sub/deep ,
    /// which are left out because no encoding is selected for them.
    ///
    /// The root holds four entries in this order: \c a.wav twice, \c b.wav with empty numbers
    /// and \c missing.wav , and \c c.wav without an entry.
    inline std::optional<VoiceBankDiskState::Opened> openRichBank(const QString &root) {
        const std::pair<const char *, QByteArray> files[] = {
            {"oto.ini",           "a.wav=" + kGbkGePing +
                            ",41.0,87.688,97.316,8.938,4.457\r\n"
                            "a.wav=- " +
                            kGbkGePing +
                            ",41,87.6880,-143.414,8.938,04.457\r\n"
                            "b.wav=,,,,,\r\n"
                            "missing.wav=m,1,2,3,4,5\r\n"             },
            {"character.txt",
             "name=" + kGbkGePing +
                 "\r\nimage=icon.bmp\r\nauthor=a\r\nweb=w\r\nsample=s.wav\r\nVersion:1.0\r\n"},
            {"prefix.map",        "C4\tp\ts\r\nD4\t\t" + kGbkGePing + "\r\n"                 },
            {"readme.txt",        kGbkGePing                                                 },
            {"a.wav",             "RIFF"                                                     },
            {"b.wav",             "RIFF"                                                     },
            {"c.wav",             "RIFF"                                                     },
            {"sub/oto.ini",       "#Charset:UTF-8\r\nx.wav=\xe8\x91\x9b,1,2,3,4,5\r\n"       },
            {"sub/x.wav",         "RIFF"                                                     },
            {"sub/deep/oto.ini",  "y.wav=\xff,1,2,3,4,5\r\n"                                 },
            {"sub/deep/y.wav",    "RIFF"                                                     },
            {"sub/inner/oto.ini", "w.wav=w,1,2,3,4,5\r\n"                                    },
            {"sub/inner/w.wav",   "RIFF"                                                     },
            {"left/oto.ini",      "z.wav=" + kGbkGePing + ",1,2,3,4,5\r\n"                   },
            {"left/z.wav",        "RIFF"                                                     },
        };
        for (const auto &[name, bytes] : files) {
            if (!writeSampleFile(root, QString::fromLatin1(name), bytes)) {
                return std::nullopt;
            }
        }

        DirectorySelector selector({
            {std::filesystem::path(),                QStringLiteral("GBK")},
            {std::filesystem::path("sub") / "inner", QStringLiteral("GBK")},
        });
        DiagnosticList diagnostics;
        return VoiceBankDiskState::open(std::filesystem::path(root.toStdU16String()), &selector,
                                        diagnostics);
    }

    /// Returns \a bank without the directories that were not read, which a session takes as
    /// absent.
    inline VoiceBank editablePart(const VoiceBank &bank) {
        QList<VoiceBankDirectory> directories;
        QMap<int, int> indices;
        for (int i = 0; i < bank.directories().size(); ++i) {
            const auto &directory = bank.directories().at(i);
            if (!directory.leftOut) {
                indices.insert(i, int(directories.size()));
                directories.push_back(directory);
            }
        }
        QList<VoiceSample> samples;
        for (auto sample : bank.samples()) {
            if (indices.contains(sample.directory)) {
                sample.directory = indices.value(sample.directory);
                samples.push_back(sample);
            }
        }
        return VoiceBank(bank.root(), directories, samples);
    }

    /// Compares every field of two voice banks, including the order of the samples, which
    /// determines the precedence between duplicate aliases.
    inline void verifyEqual(const VoiceBank &actual, const VoiceBank &expected) {
        QCOMPARE(actual.root(), expected.root());
        QCOMPARE(actual.directories().size(), expected.directories().size());
        for (int i = 0; i < expected.directories().size(); ++i) {
            const auto &a = actual.directories().at(i);
            const auto &e = expected.directories().at(i);
            QCOMPARE(a.path, e.path);
            QCOMPARE(a.path.native(), e.path.native());
            QCOMPARE(a.charset, e.charset);
            QCOMPARE(a.leftOut, e.leftOut);
            QVERIFY(a.character == e.character);
            QCOMPARE(a.prefixMap, e.prefixMap);
            QCOMPARE(a.readme, e.readme);
        }
        QCOMPARE(actual.samples().size(), expected.samples().size());
        for (int i = 0; i < expected.samples().size(); ++i) {
            const auto &a = actual.samples().at(i);
            const auto &e = expected.samples().at(i);
            QCOMPARE(a.path, e.path);
            QCOMPARE(a.path.native(), e.path.native());
            QCOMPARE(a.directory, e.directory);
            QCOMPARE(a.fileName, e.fileName);
            QCOMPARE(a.alias, e.alias);
            QCOMPARE(a.offset, e.offset);
            QCOMPARE(a.consonant, e.consonant);
            QCOMPARE(a.cutoff, e.cutoff);
            QCOMPARE(a.preUtterance, e.preUtterance);
            QCOMPARE(a.voiceOverlap, e.voiceOverlap);
            QCOMPARE(a.hasEntry, e.hasEntry);
            QVERIFY(a.spellings == e.spellings);
        }
        QCOMPARE(actual.character().name, expected.character().name);
    }

}

#endif // HELLOKIT_TESTS_EDIT_VOICEBANKSAMPLES_H
