# A plugin that only the tests load, built into a directory of plugins of its own, which adds the
# command Hello to the project windows
set(_plugins_dir ${CMAKE_CURRENT_BINARY_DIR}/plugins)

qak_add_action_extension(_actions_src TestActions.xml
    FUNCTION testActions
    NAMESPACE hello::daw::test
)

qm_find_qt(Core Widgets)

# The libraries it links are loaded already, see helloutau/plugins.
set(VCPKG_APPLOCAL_DEPS OFF)
helloutau_add_native_plugin(TestActionPlugin
    ID org.helloutau.test.action
    DISPLAY_NAME TestAction
    CATEGORY TestAction
    DEPENDENCIES org.helloutau.core
    DIRECTORY ${_plugins_dir}
    SOURCES TestActionPlugin.cpp ${_actions_src}
    LINKS HelloUtauEditor QActionKit::Widgets
    QT_LINKS Core Widgets
)
unset(VCPKG_APPLOCAL_DEPS)
target_include_directories(TestActionPlugin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

add_auto_test(test_TestActionPlugin.cpp HelloUtauEditor QActionKit::Widgets)
add_dependencies(test_TestActionPlugin TestActionPlugin CorePlugin)
target_compile_definitions(test_TestActionPlugin PRIVATE
    TEST_ACTION_PLUGINS_DIR="${_plugins_dir}"
)
