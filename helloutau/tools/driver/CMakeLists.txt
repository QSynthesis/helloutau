file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

# The executable, named after the product rather than the module. It contains only the entry
# point, so that all application logic remains testable.
helloutau_add_application(HelloUTAU
    SOURCES ${_src}
    LINKS HelloUtauWidgets
    QT_LINKS Core Widgets
)
