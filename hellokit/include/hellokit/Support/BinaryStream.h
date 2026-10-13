#ifndef HELLOKIT_SUPPORT_BINARYSTREAM_H
#define HELLOKIT_SUPPORT_BINARYSTREAM_H

#include <optional>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>
#include <QtCore/QtEndian>

#include <hellokit/Support/HelloKitSupportGlobal.h>

namespace hello::kit {

    /// Reads and writes the numbers of a binary file in its byte order, at a position in its
    /// bytes, as the binary files of the synth tools store them.
    ///
    /// \c QDataStream does the same in principle, and is not used for these reasons:
    /// - **Each value reports whether the bytes hold it.** A read past the end returns
    ///   \c std::nullopt for that value. \c QDataStream returns zero and sets a status that
    ///   concerns every read since the last check, which makes it easy to take a zero for a value
    ///   of a truncated or crafted file, the input being untrusted.
    /// - **The size of a floating-point value is that of its type.** \c QDataStream reads and
    ///   writes \c float and \c double alike with the precision of the stream, whose default
    ///   reads 8 bytes for a \c float, so that a format that mixes the two types needs the
    ///   precision switched for every value, and a forgotten switch reads the wrong bytes
    ///   without an error.
    /// - **The position is free.** A format may refer to its own positions, as \c desc.mrq of
    ///   moresampler refers to the data of each entry, and a change may overwrite bytes in place,
    ///   such as a count in a header. \c QDataStream reads and writes through a \c QIODevice,
    ///   whose position is that of the device, and writing into the middle of a \c QBuffer
    ///   requires reopening it.
    /// - Nothing of the serialization of Qt types, such as the length prefix of a \c QString, is
    ///   needed.
    ///
    /// The stream refers to the bytes, which outlive it.
    class HELLOKIT_SUPPORT_EXPORT BinaryStream {
    public:
        /// The byte order of the numbers.
        enum ByteOrder {
            LittleEndian, ///< The least significant byte first, as on x86
            BigEndian,    ///< The most significant byte first
        };

        /// Reads \a bytes from the position \a pos. A stream of this constructor cannot write.
        explicit BinaryStream(const QByteArray &bytes, qsizetype pos = 0,
                              ByteOrder order = LittleEndian);

        /// Reads and writes \a bytes from the position \a pos.
        explicit BinaryStream(QByteArray *bytes, qsizetype pos = 0, ByteOrder order = LittleEndian);

        ByteOrder byteOrder() const {
            return m_order;
        }

        /// Returns whether the stream writes, as constructed from a pointer.
        bool isWritable() const {
            return m_target != nullptr;
        }

        /// Returns the position of the next value.
        qsizetype pos() const {
            return m_pos;
        }

        /// Moves to the position \a pos, and returns whether the bytes hold it. A position equal
        /// to the size is the end, where a write appends.
        bool seek(qsizetype pos);

        /// Returns the number of bytes after the position.
        qsizetype bytesAvailable() const {
            return m_bytes->size() - m_pos;
        }

        /// Skips \a count bytes, and returns whether the bytes hold them.
        bool skip(qsizetype count);

        /// Returns the next number, or \c std::nullopt if the bytes do not hold it, in which case
        /// the position is kept.
        template <class T>
        std::optional<T> read() {
            static_assert(std::is_arithmetic_v<T>, "BinaryStream reads numbers only");
            if (bytesAvailable() < qsizetype(sizeof(T))) {
                return std::nullopt;
            }
            const auto data = m_bytes->constData() + m_pos;
            m_pos += qsizetype(sizeof(T));
            return m_order == LittleEndian ? qFromLittleEndian<T>(data) : qFromBigEndian<T>(data);
        }

        /// Returns the next \a count bytes, or \c std::nullopt if the bytes do not hold them.
        std::optional<QByteArray> readBytes(qsizetype count);

        /// Writes \a value at the position and moves past it, overwriting the bytes there and
        /// appending past the end.
        ///
        /// \pre isWritable()
        template <class T>
        void write(T value) {
            static_assert(std::is_arithmetic_v<T>, "BinaryStream writes numbers only");
            char data[sizeof(T)];
            if (m_order == LittleEndian) {
                qToLittleEndian(value, data);
            } else {
                qToBigEndian(value, data);
            }
            writeBytes(QByteArrayView(data, qsizetype(sizeof(T))));
        }

        /// Writes \a bytes at the position and moves past them, overwriting the bytes there and
        /// appending past the end.
        ///
        /// \pre isWritable()
        void writeBytes(QByteArrayView bytes);

    private:
        const QByteArray *m_bytes;
        QByteArray *m_target;
        qsizetype m_pos;
        ByteOrder m_order;
    };

}

#endif // HELLOKIT_SUPPORT_BINARYSTREAM_H
