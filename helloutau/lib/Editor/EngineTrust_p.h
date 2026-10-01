#ifndef HELLOUTAU_EDITOR_ENGINETRUST_P_H
#define HELLOUTAU_EDITOR_ENGINETRUST_P_H

#include <filesystem>

#include <QtCore/QString>

class QWidget;

namespace hello::daw {

    class AppSettings;

    namespace EngineTrust {
        enum class Kind { Wavtool, Resampler };
        std::filesystem::path resolved(const QString &value, const std::filesystem::path &utau);
        bool exists(const QString &value, const std::filesystem::path &utau);
        bool samePath(const QString &first, const QString &second,
                      const std::filesystem::path &utau);
        bool isTrusted(const AppSettings &settings, const QString &value,
                       const std::filesystem::path &utau, Kind kind);
        bool ask(QWidget *parent, AppSettings &settings, const QString &value,
                 const std::filesystem::path &utau, Kind kind);
        void trust(AppSettings &settings, const QString &value, const std::filesystem::path &utau,
                   Kind kind);
    }
}

#endif
