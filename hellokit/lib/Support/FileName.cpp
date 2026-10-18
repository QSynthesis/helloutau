#include "FileName.h"

#include <QtCore/QString>

namespace hello::kit {

    std::filesystem::path FileName::listed(const std::filesystem::path &name) {
#ifdef Q_OS_MACOS
        // The same conversion as QFile::decodeName() on macOS
        return QString::fromStdU16String(name.u16string())
            .normalized(QString::NormalizationForm_C)
            .toStdU16String();
#else
        return name;
#endif
    }

}
