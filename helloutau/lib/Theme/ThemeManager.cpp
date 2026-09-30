#include "ThemeManager.h"

#include <algorithm>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtWidgets/QWidget>

#include <stdcorelib/pimpl.h>

#include "ThemeIcon.h"
#include "ThemeLogging_p.h"
#include "ThemeStyleSheet.h"

namespace hello::daw {

    namespace {

        constexpr char16_t CommonTheme[] = u"_common";
        constexpr char16_t BaseVariable[] = u"_base";

        // The keys of the values specific to the current operating system
        const QStringList &platformKeys() {
            static const QStringList keys{
#if defined(Q_OS_WINDOWS)
                QStringLiteral("win"),
                QStringLiteral("windows"),
#elif defined(Q_OS_MACOS)
                QStringLiteral("mac"),
                QStringLiteral("macos"),
#else
                QStringLiteral("linux"),
#endif
            };
            return keys;
        }

        // Returns \a value, or its entry for the current operating system if \a value is
        // specified per system as a nonempty object whose keys are all operating system names.
        // Returns an undefined value if such an object has no entry for the current system.
        QJsonValue forPlatform(const QJsonValue &value) {
            static const QStringList systems{QStringLiteral("win"), QStringLiteral("windows"),
                                             QStringLiteral("mac"), QStringLiteral("macos"),
                                             QStringLiteral("linux")};
            const auto object = value.toObject();
            if (!value.isObject() || object.isEmpty()) {
                return value;
            }
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (!systems.contains(it.key())) {
                    return value;
                }
            }
            for (const auto &key : platformKeys()) {
                if (object.contains(key)) {
                    return object.value(key);
                }
            }
            return QJsonValue::Undefined;
        }

        // A style sheet of a namespace
        struct Sheet {
            double priority = 1;
            double ratio = 1;
            QString text;
            QString directory;
        };

        struct Variable {
            double priority = 1;
            QString value;
        };

    }

    class ThemeManager::Impl {
    public:
        using Decl = ThemeManager;

        explicit Impl(Decl *decl) : _decl(decl) {
        }

        Decl *_decl;
        QStringList searchPaths;
        QString current;
        double scale = 1;
        double fontScale = 1;

        // Maps from identifier to namespaces, from theme to namespace to sheets, and from theme
        // to variables
        QHash<QString, QStringList> namespaces;
        QHash<QString, QHash<QString, QList<Sheet>>> sheets;
        QHash<QString, QHash<QString, Variable>> variables;

        QList<std::pair<QPointer<QWidget>, QStringList>> widgets;
        bool pending = false;

        void read() {
            namespaces.clear();
            sheets.clear();
            variables.clear();
            for (const auto &path : std::as_const(searchPaths)) {
                QStringList files;
                QDirIterator it(path, {QStringLiteral("*.res.json")}, QDir::Files,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    files.push_back(it.next());
                }
                // Sorted, so that sheets of equal priority have a stable order
                files.sort();
                for (const auto &file : std::as_const(files)) {
                    readFile(file);
                }
            }
        }

        void readFile(const QString &path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                qCWarning(lcTheme) << "The theme file" << path << "cannot be read.";
                return;
            }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            if (!document.isObject()) {
                qCWarning(lcTheme).noquote()
                    << QStringLiteral("The theme file %1 is not a JSON object: %2")
                           .arg(path, error.errorString());
                return;
            }
            const auto root = document.object();
            const auto directory = QFileInfo(path).absolutePath();

            double ratio = 1;
            double priority = 1;
            const auto config = root.value(QStringLiteral("config")).toObject();
            ratio = forPlatform(config.value(QStringLiteral("ratio"))).toDouble(ratio);
            priority = forPlatform(config.value(QStringLiteral("priority"))).toDouble(priority);

            const auto widgets = root.value(QStringLiteral("widgets")).toObject();
            for (auto it = widgets.begin(); it != widgets.end(); ++it) {
                auto &list = namespaces[it.key()];
                if (it->isArray()) {
                    for (const auto &item : it->toArray()) {
                        list.push_back(item.toString());
                    }
                } else {
                    list.push_back(it->toString());
                }
            }

            const auto themes = root.value(QStringLiteral("variables")).toObject();
            for (auto theme = themes.begin(); theme != themes.end(); ++theme) {
                const auto object = theme->toObject();
                auto &map = variables[theme.key()];
                for (auto it = object.begin(); it != object.end(); ++it) {
                    Variable variable{priority, {}};
                    auto value = forPlatform(*it);
                    if (value.isObject()) {
                        const auto described = value.toObject();
                        variable.priority = forPlatform(described.value(QStringLiteral("priority")))
                                                .toDouble(priority);
                        value = forPlatform(described.value(QStringLiteral("value")));
                    }
                    if (!value.isString()) {
                        continue;
                    }
                    variable.value = value.toString();
                    const auto existing = map.find(it.key());
                    if (existing == map.end() || existing->priority <= variable.priority) {
                        map.insert(it.key(), variable);
                    }
                }
            }

            const auto styles = root.value(QStringLiteral("stylesheets")).toObject();
            for (auto theme = styles.begin(); theme != styles.end(); ++theme) {
                const auto object = theme->toObject();
                for (auto it = object.begin(); it != object.end(); ++it) {
                    const auto items = it->isArray() ? it->toArray() : QJsonArray{*it};
                    for (const auto &item : items) {
                        addSheet(theme.key(), it.key(), item.toObject(), directory, ratio,
                                 priority);
                    }
                }
            }
        }

        void addSheet(const QString &theme, const QString &space, const QJsonObject &item,
                      const QString &directory, double ratio, double priority) {
            Sheet sheet;
            sheet.ratio = forPlatform(item.value(QStringLiteral("ratio"))).toDouble(ratio);
            sheet.priority = forPlatform(item.value(QStringLiteral("priority"))).toDouble(priority);
            sheet.directory = directory;
            const auto fileName = forPlatform(item.value(QStringLiteral("file"))).toString();
            if (!fileName.isEmpty()) {
                const auto path = QDir(directory).absoluteFilePath(fileName);
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) {
                    qCWarning(lcTheme) << "The style sheet" << path << "cannot be read.";
                    return;
                }
                sheet.text = QString::fromUtf8(file.readAll());
                sheet.directory = QFileInfo(path).absolutePath();
            } else {
                sheet.text = forPlatform(item.value(QStringLiteral("content"))).toString();
            }
            if (!sheet.text.isEmpty()) {
                sheets[theme][space].push_back(sheet);
            }
        }

        // Returns the chain of themes along _base, from the most basic theme to \a theme
        QStringList chain(const QString &theme) const {
            QStringList result;
            QString at = theme;
            while (!at.isEmpty()) {
                if (result.contains(at)) {
                    qCWarning(lcTheme) << "The themes extend each other in a cycle at" << at;
                    break;
                }
                result.prepend(at);
                at = variables.value(at).value(QString::fromUtf16(BaseVariable)).value;
            }
            return result;
        }

        // Returns the variables of a chain, in which later themes override earlier themes
        QHash<QString, QString> variablesOf(const QStringList &themes) const {
            QHash<QString, QString> result;
            for (const auto &theme : themes) {
                const auto map = variables.value(theme);
                for (auto it = map.begin(); it != map.end(); ++it) {
                    result.insert(it.key(), it->value);
                }
            }
            return result;
        }

        static QString substituted(const QString &text, const QHash<QString, QString> &values) {
            QString result;
            qsizetype at = 0;
            while (true) {
                const auto start = text.indexOf(QStringLiteral("${"), at);
                const auto end = start < 0 ? -1 : text.indexOf(u'}', start + 2);
                if (end < 0) {
                    result += QStringView(text).sliced(at);
                    return result;
                }
                result += QStringView(text).sliced(at, start - at);
                const auto name = text.mid(start + 2, end - start - 2);
                const auto found = values.find(name);
                if (found == values.end()) {
                    qCWarning(lcTheme) << "The theme defines no variable" << name;
                    result += QStringView(text).sliced(start, end + 1 - start);
                } else {
                    result += *found;
                }
                at = end + 1;
            }
        }

        QString assemble(const QStringList &ids) const {
            QStringList spaces;
            for (const auto &id : ids) {
                for (const auto &space : namespaces.value(id)) {
                    if (!spaces.contains(space)) {
                        spaces.push_back(space);
                    }
                }
            }
            // The chain of _common first, followed by the chain of the current theme. Every sheet
            // uses the variables of both chains.
            const auto common = chain(QString::fromUtf16(CommonTheme));
            auto themes = common;
            if (!current.isEmpty() && current != QString::fromUtf16(CommonTheme)) {
                for (const auto &theme : chain(current)) {
                    themes.push_back(theme);
                }
            }
            const auto values = variablesOf(themes);

            QStringList parts;
            for (const auto &theme : std::as_const(themes)) {
                QList<Sheet> selected;
                for (const auto &space : std::as_const(spaces)) {
                    selected += sheets.value(theme).value(space);
                }
                // Sheets of equal priority remain in the order of the namespaces of the widget,
                // and within a namespace in the order of the files and of the arrays in them
                std::stable_sort(
                    selected.begin(), selected.end(),
                    [](const Sheet &a, const Sheet &b) { return a.priority < b.priority; });
                for (const auto &sheet : std::as_const(selected)) {
                    ThemeStyleSheet::Options options;
                    options.directory = sheet.directory;
                    options.scale = scale * sheet.ratio;
                    options.fontScale = fontScale * sheet.ratio;
                    parts.push_back(
                        ThemeStyleSheet::preprocess(substituted(sheet.text, values), options));
                }
            }
            return parts.join(QStringLiteral("\n\n"));
        }

        void schedule() {
            stdc_decl_t;
            if (pending) {
                return;
            }
            pending = true;
            QTimer::singleShot(0, &decl, [this] { refresh(); });
        }

        void refresh() {
            pending = false;
            widgets.removeIf([](const auto &entry) { return entry.first.isNull(); });
            for (const auto &[widget, ids] : std::as_const(widgets)) {
                widget->setStyleSheet(assemble(ids));
            }
        }
    };

    ThemeManager::ThemeManager(QObject *parent)
        : QObject(parent), _impl(std::make_unique<Impl>(this)) {
    }

    ThemeManager::~ThemeManager() = default;

    void ThemeManager::addSearchPath(const QString &directory) {
        stdc_impl_t;
        impl.searchPaths.push_back(directory);
        reload();
    }

    void ThemeManager::reload() {
        stdc_impl_t;
        // Clears the icon cache as well, so that icons are drawn from the current files.
        ThemeIcon::clearCache();
        impl.read();
        impl.schedule();
    }

    QStringList ThemeManager::themes() const {
        stdc_impl_t;
        QStringList result;
        for (const auto &theme : impl.sheets.keys() + impl.variables.keys()) {
            if (theme != QString::fromUtf16(CommonTheme) && !result.contains(theme)) {
                result.push_back(theme);
            }
        }
        result.sort();
        return result;
    }

    QString ThemeManager::currentTheme() const {
        stdc_impl_t;
        return impl.current;
    }

    void ThemeManager::setCurrentTheme(const QString &theme) {
        stdc_impl_t;
        if (theme == impl.current) {
            return;
        }
        impl.current = theme;
        impl.schedule();
        Q_EMIT currentThemeChanged();
    }

    double ThemeManager::scale() const {
        stdc_impl_t;
        return impl.scale;
    }

    void ThemeManager::setScale(double scale) {
        stdc_impl_t;
        if (scale > 0 && scale != impl.scale) {
            impl.scale = scale;
            impl.schedule();
        }
    }

    double ThemeManager::fontScale() const {
        stdc_impl_t;
        return impl.fontScale;
    }

    void ThemeManager::setFontScale(double scale) {
        stdc_impl_t;
        if (scale > 0 && scale != impl.fontScale) {
            impl.fontScale = scale;
            impl.schedule();
        }
    }

    void ThemeManager::install(QWidget *widget, const QStringList &ids) {
        stdc_impl_t;
        impl.widgets.push_back({widget, ids});
        widget->setStyleSheet(impl.assemble(ids));
    }

    QString ThemeManager::styleSheet(const QStringList &ids) const {
        stdc_impl_t;
        return impl.assemble(ids);
    }

}
