#include "BinaryStream.h"

#include <algorithm>

namespace hello::kit {

    BinaryStream::BinaryStream(const QByteArray &bytes, qsizetype at, ByteOrder order)
        : m_bytes(&bytes), m_target(nullptr), m_at(std::clamp<qsizetype>(at, 0, bytes.size())),
          m_order(order) {
    }

    BinaryStream::BinaryStream(QByteArray *bytes, qsizetype at, ByteOrder order)
        : m_bytes(bytes), m_target(bytes), m_at(std::clamp<qsizetype>(at, 0, bytes->size())),
          m_order(order) {
    }

    bool BinaryStream::seek(qsizetype at) {
        if (at < 0 || at > m_bytes->size()) {
            return false;
        }
        m_at = at;
        return true;
    }

    bool BinaryStream::skip(qsizetype count) {
        if (count < 0 || count > left()) {
            return false;
        }
        m_at += count;
        return true;
    }

    std::optional<QByteArray> BinaryStream::readBytes(qsizetype count) {
        if (count < 0 || count > left()) {
            return std::nullopt;
        }
        const auto bytes = m_bytes->mid(m_at, count);
        m_at += count;
        return bytes;
    }

    void BinaryStream::writeBytes(QByteArrayView bytes) {
        Q_ASSERT(m_target);
        if (!m_target) {
            return;
        }
        const auto end = m_at + bytes.size();
        if (end > m_target->size()) {
            m_target->resize(end);
        }
        std::copy(bytes.begin(), bytes.end(), m_target->begin() + m_at);
        m_at = end;
    }

}
