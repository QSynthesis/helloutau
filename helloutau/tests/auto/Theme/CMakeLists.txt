qm_find_qt(Widgets)

helloutau_add_auto_test(test_ThemeSyntax.cpp HelloUtauTheme)
helloutau_add_auto_test(test_ThemeTypes.cpp HelloUtauTheme Qt${QT_VERSION_MAJOR}::Widgets)
helloutau_add_auto_test(test_ThemeManager.cpp HelloUtauTheme Qt${QT_VERSION_MAJOR}::Widgets
    RESOURCES themes)
helloutau_add_auto_test(test_ThemeIcon.cpp HelloUtauTheme Qt${QT_VERSION_MAJOR}::Widgets
    RESOURCES icons)
