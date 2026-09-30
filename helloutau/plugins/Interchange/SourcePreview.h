#ifndef HELLOUTAU_INTERCHANGE_SOURCEPREVIEW_H
#define HELLOUTAU_INTERCHANGE_SOURCEPREVIEW_H

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <hellokit/Interchange/InterchangeSource.h>
#include <hellokit/Support/TextCodec.h>

#include <Interchange/InterchangePluginGlobal.h>

namespace hello::daw {

    /// Decoding of the undecoded text of an inspected file for the encoding selection of the
    /// import wizard. See the encoding page in docs/ImportExport.md.
    class INTERCHANGEPLUGIN_EXPORT SourcePreview {
        Q_DECLARE_TR_FUNCTIONS(hello::daw::SourcePreview)
    public:
        /// Returns the undecoded text of \a source: the entry names, the lyrics and the labels.
        static QList<QByteArray> textsOf(const kit::InterchangeSource &source);

        /// Returns whether \a encoding decodes each of \a texts without an invalid byte
        /// sequence.
        static bool decodes(const QList<QByteArray> &texts, const QString &encoding);

        /// Returns the default selection among \a candidates for \a texts: UTF-8 if it decodes
        /// every text, else the system encoding \a system if it is a candidate and decodes
        /// every text, else the first candidate of \c TextCodec::ranked(). The default is a
        /// preselection only, and the user confirms or changes it.
        static QString defaultEncoding(const QList<QByteArray> &texts,
                                       const QStringList &candidates,
                                       const QString &system = kit::TextCodec::systemName());

        /// Returns the text of \a source decoded with \a encoding for display: the names and the
        /// lyrics of the entries, and the labels. A text that is invalid in \a encoding is
        /// replaced by a marker instead of being shown garbled.
        static QString preview(const kit::InterchangeSource &source, const QString &encoding);

        /// Returns \a bytes decoded with \a encoding, or a marker if \a bytes is invalid in
        /// \a encoding.
        static QString decodedOrMarker(const QByteArray &bytes, const QString &encoding);
    };

}

#endif // HELLOUTAU_INTERCHANGE_SOURCEPREVIEW_H
