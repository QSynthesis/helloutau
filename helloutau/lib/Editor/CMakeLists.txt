project(HelloUtauEditor
    VERSION ${HELLO_VERSION}
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp" "*_p.h")
file(GLOB_RECURSE _hdr "${CMAKE_CURRENT_SOURCE_DIR}/../../include/helloutau/Editor/*.h")

qm_find_qt(Core Gui Widgets)

# The menus, the tool bars and the commands of each kind of window, compiled by AEC into
# ProjectActions.qak.h, VoiceBankActions.qak.h and their sources. BuiltinActions registers each
# with the action registry of its kind of window.
qak_add_action_extension(_project_actions_src ProjectActions.xml
    FUNCTION projectActions
    NAMESPACE hello::daw
)
qak_add_action_extension(_actions_src VoiceBankActions.xml
    FUNCTION voiceBankActions
    NAMESPACE hello::daw
)
list(APPEND _actions_src ${_project_actions_src})

# The built-in theme, under :/helloutau/themes, and the icons of the commands, under
# :/helloutau/icons
set(_themes_qrc themes/Themes.qrc)
set(_icons_qrc icons/Icons.qrc)

helloutau_add_library(${PROJECT_NAME} SHARED
    QT_AUTOGEN
    SOURCES ${_src} ${_hdr} ${_actions_src} ${_themes_qrc} ${_icons_qrc}
    LINKS HelloUtauWidgets HelloUtauTheme HelloUtauAudio HelloKitEdit HelloKitDocument
          HelloKitVoiceBank HelloKitSynth
    LINKS_PRIVATE stdcorelib::stdcorelib stdcorelib::plugin QActionKit::Widgets
    QT_LINKS Core Gui Widgets
    INCLUDE_PRIVATE ../../include/** **
    MACRO_PREFIX HELLOUTAU_EDITOR
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Where AppLoader finds the plugins that come with the application, relative to the directory of
# the program: ../lib/plugins/helloutau, or ../Plugins in a macOS bundle. The build and the
# installation lay them out alike.
file(RELATIVE_PATH _builtin_plugins ${HELLOUTAU_BUILD_RUNTIME_DIR} ${HELLOUTAU_BUILD_PLUGINS_DIR})
file(RELATIVE_PATH _installed_plugins
    /${HELLOUTAU_INSTALL_RUNTIME_DIR} /${HELLOUTAU_INSTALL_PLUGINS_DIR})
if(NOT _builtin_plugins STREQUAL _installed_plugins)
    message(FATAL_ERROR "The plugins are at ${_builtin_plugins} from the program in the build "
        "but at ${_installed_plugins} in the installation.")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE
    HELLOUTAU_BUILTIN_PLUGINS_PATH="${_builtin_plugins}"
)
