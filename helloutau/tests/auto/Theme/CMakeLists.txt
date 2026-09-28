qm_find_qt(Widgets)

add_auto_test(test_ThemeSyntax.cpp HelloUtauTheme)
add_auto_test(test_ThemeTypes.cpp HelloUtauTheme Qt${QT_VERSION_MAJOR}::Widgets)
add_auto_test(test_ThemeManager.cpp HelloUtauTheme Qt${QT_VERSION_MAJOR}::Widgets
    RESOURCES themes)
