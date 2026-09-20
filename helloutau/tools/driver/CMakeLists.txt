file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

# The executable, named for the product rather than for the module. It holds the entry point and
# nothing else, so that everything it starts stays testable.
helloutau_add_application(helloutau
    SOURCES ${_src}
    LINKS HelloUtauWidgets
    QT_LINKS Core Widgets
)
