#include <filesystem>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>

#include <zip.h>

#include <hellokit/Support/TextCodec.h>
#include <hellokit/VoiceBank/InstallArchive.h>

#include <helloutau/Editor/Dialogs/InstallArchiveDialog.h>

using namespace hello;
using namespace hello::daw;
namespace fs = std::filesystem;

namespace {

    struct TestEntry {
        QByteArray name;
        QByteArray data;
        // ZIP_FL_ENC_CP437 stores the bytes of the name without the UTF-8 flag, as old
        // archivers stored Shift_JIS.
        zip_flags_t encoding = ZIP_FL_ENC_GUESS;
    };

    bool writeArchive(const QString &path, const QList<TestEntry> &entries) {
        int error = 0;
        const auto zip =
            zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (!zip) {
            return false;
        }
        for (const auto &entry : entries) {
            const auto source =
                zip_source_buffer(zip, entry.data.constData(), zip_uint64_t(entry.data.size()), 0);
            if (!source || zip_file_add(zip, entry.name.constData(), source, entry.encoding) < 0) {
                zip_source_free(source);
                zip_discard(zip);
                return false;
            }
        }
        return zip_close(zip) == 0;
    }

    fs::path pathOf(const QString &path) {
        return fs::path(path.toStdU16String());
    }

    // あいｱ in Shift_JIS. GBK reads the last byte as a lead byte without its trail.
    const QByteArray shiftJisKana("\x82\xA0\x82\xA2\xB1");
    // あい in Shift_JIS, which GBK reads as two ideographs
    const QByteArray shiftJisValidInGbk("\x82\xA0\x82\xA2");

    const QByteArray voiceManifest("type=voiceset\r\nfolder=bank\r\ncontentsdir=bank\r\n"
                                   "description=A test bank\r\n");

}

class test_InstallArchiveDialog : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    QString archivePath(const QString &name = QStringLiteral("bank.uar")) const {
        return m_root.filePath(name);
    }

private Q_SLOTS:
    void cleanup() {
        kit::TextCodec::setSystemName({});
    }

    // A .uar is an installation archive whatever it holds, and is refused at installation if it
    // has no install.txt. A .zip is one only with an install.txt at its root.
    void an_installation_archive_is_a_uar_or_a_zip_with_install_txt() {
        QVERIFY(writeArchive(archivePath(QStringLiteral("with.zip")),
                             {
                                 {"install.txt",        voiceManifest},
                                 {"bank/character.txt", {}           }
        }));
        QVERIFY(writeArchive(archivePath(QStringLiteral("without.zip")),
                             {
                                 {"bank/install.txt", voiceManifest}
        }));
        QVERIFY(writeArchive(archivePath(QStringLiteral("without.uar")), {
                                                                             {"a.txt", {}}
        }));
        QVERIFY(InstallArchiveDialog::isInstallArchive(pathOf(archivePath("with.zip"))));
        QVERIFY(!InstallArchiveDialog::isInstallArchive(pathOf(archivePath("without.zip"))));
        QVERIFY(InstallArchiveDialog::isInstallArchive(pathOf(archivePath("without.uar"))));
        QVERIFY(InstallArchiveDialog::isInstallArchive(pathOf(archivePath("absent.UAR"))));
        QVERIFY(!InstallArchiveDialog::isInstallArchive(pathOf(archivePath("absent.zip"))));
        QVERIFY(!InstallArchiveDialog::isInstallArchive(pathOf(archivePath("song.ust"))));
    }

    // Names in ASCII need no encoding. The text files read best in Shift_JIS, which is selected
    // and names the voice bank, and the encodings in which they are invalid are gray.
    void a_voice_bank_with_ascii_names_asks_for_the_text_encoding_only() {
        QVERIFY(writeArchive(archivePath(),
                             {
                                 {"install.txt",        voiceManifest                             },
                                 {"bank/character.txt", "name=" + shiftJisKana + "\r\n"           },
                                 {"bank/oto.ini",       "a.wav=" + shiftJisKana + ",0,0,0,0,0\r\n"},
                                 {"bank/a.wav",         "a"                                       }
        }));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        QVERIFY2(dialog.isInstallable(), qPrintable(dialog.errorText()));
        QCOMPARE(dialog.nameEncoding(), QString());
        QVERIFY(!dialog.isNameModeShown());
        QCOMPARE(dialog.contentPaths(), (QStringList{"character.txt", "oto.ini", "a.wav"}));

        QCOMPARE(dialog.charset(), QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.isCharsetGray(QStringLiteral("GBK")));
        QVERIFY(dialog.isCharsetGray(QStringLiteral("UTF-8")));
        QVERIFY(!dialog.isCharsetGray(QStringLiteral("Shift_JIS")));
        const auto kana = QString::fromUtf8("あいｱ");
        QCOMPARE(dialog.contentName(), kana);
        QVERIFY(dialog.summaryText().contains(kana));
        QVERIFY(dialog.summaryText().contains(QStringLiteral("A test bank")));
        QCOMPARE(dialog.previewTitles(), (QStringList{"Files", "character.txt", "oto.ini"}));
        QVERIFY(dialog.previewText(2).contains(kana));

        // The preview follows the encoding, and marks the invalid byte sequences in the title.
        dialog.setCharset(QStringLiteral("GBK"));
        QVERIFY(dialog.previewTitles().value(1).startsWith(QStringLiteral("character.txt (")));
        QVERIFY(!dialog.previewText(2).contains(kana));
    }

    // Names without the UTF-8 flag need an encoding, the one that reads them best initially.
    // The names cannot be misdecoded in a code page in which they are invalid.
    void names_without_the_utf8_flag_ask_for_their_encoding() {
        kit::TextCodec::setSystemName(QStringLiteral("GBK"));
        QVERIFY(writeArchive(archivePath(),
                             {
                                 {"install.txt", voiceManifest},
                                 {"bank/character.txt", "name=bank\r\n"},
                                 {"bank/" + shiftJisKana + ".wav", "a", ZIP_FL_ENC_CP437}
        }));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        QCOMPARE(dialog.nameEncoding(), QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.isNameEncodingGray(QStringLiteral("GBK")));
        QVERIFY(dialog.isNameEncodingGray(QStringLiteral("UTF-8")));
        QVERIFY2(dialog.isInstallable(), qPrintable(dialog.errorText()));
        QCOMPARE(dialog.contentPaths(),
                 (QStringList{"character.txt", QString::fromUtf8("あいｱ.wav")}));
        QVERIFY(dialog.isNameModeShown());
        QVERIFY(!dialog.isMisdecodedAvailable());
        QCOMPARE(dialog.nameMode(), kit::InstallArchive::Decoded);

        // In an encoding that cannot read the names, the archive is not installable.
        dialog.setNameEncoding(QStringLiteral("UTF-8"));
        QVERIFY(!dialog.isInstallable());
        QVERIFY(!dialog.errorText().isEmpty());
        QVERIFY(dialog.contentPaths().isEmpty());
    }

    // The names can be written as UTAU writes them if the code page of the system reads them,
    // and the choice is not offered if that code page is the encoding of the names.
    void the_names_can_be_misdecoded_in_the_code_page_of_the_system() {
        kit::TextCodec::setSystemName(QStringLiteral("GBK"));
        QVERIFY(writeArchive(archivePath(),
                             {
                                 {"install.txt", voiceManifest},
                                 {"bank/character.txt", "name=bank\r\n"},
                                 {"bank/" + shiftJisValidInGbk + ".wav", "a", ZIP_FL_ENC_CP437}
        }));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        dialog.setNameEncoding(QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.isNameModeShown());
        QVERIFY(dialog.isMisdecodedAvailable());
        dialog.setNameMode(kit::InstallArchive::Misdecoded);
        QCOMPARE(dialog.nameMode(), kit::InstallArchive::Misdecoded);

        dialog.setNameEncoding(QStringLiteral("GBK"));
        QVERIFY(dialog.isInstallable());
        QVERIFY(!dialog.isNameModeShown());
        QCOMPARE(dialog.nameMode(), kit::InstallArchive::Decoded);
    }

    // On a system whose encoding is UTF-8, the names cannot be misdecoded and every name is
    // representable, so that the choice is not offered.
    void a_system_in_utf8_offers_no_choice_of_the_names() {
        kit::TextCodec::setSystemName(QStringLiteral("UTF-8"));
        QVERIFY(writeArchive(archivePath(),
                             {{"install.txt", voiceManifest},
                              {"bank/character.txt", "name=bank\r\n"},
                              {"bank/" + shiftJisKana + ".wav", "a", ZIP_FL_ENC_CP437}}));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        QCOMPARE(dialog.nameEncoding(), QStringLiteral("Shift_JIS"));
        QVERIFY(dialog.isInstallable());
        QVERIFY(!dialog.isNameModeShown());
        QCOMPARE(dialog.nameMode(), kit::InstallArchive::Decoded);
    }

    // A plugin has no encoding of its text files, and its name comes from plugin.txt in the
    // code page of the system, as UTAU reads it.
    void a_plugin_is_named_by_its_plugin_txt() {
        kit::TextCodec::setSystemName(QStringLiteral("Shift_JIS"));
        QVERIFY(writeArchive(
            archivePath(),
            {
                {"install.txt",     "type=editplugin\r\nfolder=tool\r\ncontentsdir=tool\r\n"},
                {"tool/plugin.txt", "name=" + shiftJisKana + "\r\nexecute=tool.exe\r\n"     },
                {"tool/tool.exe",   "x"                                                     }
        }));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        QVERIFY2(dialog.isInstallable(), qPrintable(dialog.errorText()));
        QCOMPARE(archive.kind(), kit::InstallArchive::Plugin);
        QCOMPARE(dialog.charset(), QString());
        QCOMPARE(dialog.contentName(), QString::fromUtf8("あいｱ"));
        QCOMPARE(dialog.previewTitles(), (QStringList{"Files", "plugin.txt"}));
    }

    // The reason of an archive that cannot be installed is shown, and the content is not.
    void an_archive_that_cannot_be_installed_shows_the_reason() {
        QVERIFY(
            writeArchive(archivePath(), {
                                            {"install.txt",  voiceManifest},
                                            {"bank/oto.ini", {}           }
        }));
        kit::InstallArchive archive;
        QVERIFY2(archive.open(pathOf(archivePath())), qPrintable(archive.errorString()));
        InstallArchiveDialog dialog(archive);
        QVERIFY(!dialog.isInstallable());
        QVERIFY(dialog.errorText().contains(QStringLiteral("character.txt")));
        QVERIFY(dialog.contentPaths().isEmpty());
        QCOMPARE(dialog.previewTitles(), QStringList{"Files"});
    }
};

QTEST_MAIN(test_InstallArchiveDialog)

#include "test_InstallArchiveDialog.moc"
