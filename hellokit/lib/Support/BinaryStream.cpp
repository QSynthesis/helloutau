#include "BinaryStream.h"

#include <algorithm>

namespace hello::kit {

    BinaryStream::BinaryStream(const QByteArray &bytes, qsizetype pos, ByteOrder order)
        : m_bytes(&bytes), m_target(nullptr), m_pos(std::clamp<qsizetype>(pos, 0, bytes.size())),
          m_order(order) {
    }

    BinaryStream::BinaryStream(QByteArray *bytes, qsizetype pos, ByteOrder order)
        : m_bytes(bytes), m_target(bytes), m_pos(std::clamp<qsizetype>(pos, 0, bytes->size())),
          m_order(order) {
    }

    bool BinaryStream::seek(qsizetype pos) {
        if (pos < 0 || pos > m_bytes->size()) {
            return false;
        }
        m_pos = pos;
        return true;
    }

    bool BinaryStream::skip(qsizetype count) {
        if (count < 0 || count > bytesAvailable()) {
            return false;
        }
        m_pos += count;
        return true;
    }

    std::optional<QByteArray> BinaryStream::readBytes(qsizetype count) {
        if (count < 0 || count > bytesAvailable()) {
            return std::nullopt;
        }
        const auto bytes = m_bytes->mid(m_pos, count);
        m_pos += count;
        return bytes;
    }

    void BinaryStream::writeBytes(QByteArrayView bytes) {
        Q_ASSERT(m_target);
        if (!m_target) {
            return;
        }
        const auto end = m_pos + bytes.size();
        if (end > m_target->size()) {
            m_target->resize(end);
        }
        std::copy(bytes.begin(), bytes.end(), m_target->begin() + m_pos);
        m_pos = end;
    }

}
