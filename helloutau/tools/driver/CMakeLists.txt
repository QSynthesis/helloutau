project(helloutau
    VERSION ${HELLO_VERSION}
    DESCRIPTION "HelloUtau Editor"
    LANGUAGES CXX
)

file(GLOB_RECURSE _src "*.cpp")

qm_find_qt(Core Widgets)

set(HELLOUTAU_WINDOWS_APPLICATION ON)
set(HELLOUTAU_CONSOLE_APPLICATION OFF)

helloutau_add_application(${PROJECT_NAME}
    SOURCES ${_src}
    LINKS HelloUtauEditor
    QT_LINKS Core Widgets
)

# The icons of the application and of the file types that the installer of Windows and the bundle
# of macOS associate with it, written from the drawings in Resources/icons, one directory for each
# icon and named after it.
if(WIN32 OR APPLE)
    include(AddIcon)
    set(_suffix .ico)
    if(APPLE)
        set(_suffix .icns)
    endif()
    set(_app_icon "${CMAKE_CURRENT_BINARY_DIR}/app${_suffix}")
    add_icon("${_app_icon}" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/app/app.svg"
        SMALL_DRAWINGS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/app/app"
    )
    set(_file_icons)
    foreach(_type IN ITEMS usth ust uar)
        set(_icon "${CMAKE_CURRENT_BINARY_DIR}/icons/${_type}${_suffix}")
        add_icon("${_icon}" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/${_type}/${_type}.svg"
            SMALL_DRAWINGS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/${_type}/${_type}"
        )
        list(APPEND _file_icons "${_icon}")
    endforeach()
    target_sources(${PROJECT_NAME} PRIVATE ${_file_icons})
endif()

# The icon of the application is also in the resources of the program, and is the icon of the
# installer. The installer registers the icons of the file types as their default icons, see
# setup.iss.
if(WIN32)
    install(FILES "${_app_icon}" ${_file_icons} DESTINATION "${HELLOUTAU_INSTALL_DATA_DIR}/icons")
endif()

# The desktop entry, the icons and the file types on Linux, in share as the specifications of
# freedesktop.org lay them out, so that an installation into /usr works as it is. The scripts of
# the package copy them into the data directory of the user, see dist/installer/linux. The drawings
# are installed as they are, because an icon theme accepts SVG in the directory of each size.
# hicolor has no directory of 20 px.
if(UNIX AND NOT APPLE)
    set(_share "${HELLOUTAU_INSTALL_SHARE_DIR}")
    install(FILES "Resources/${HELLOUTAU_APPLICATION_ID}.desktop"
        DESTINATION "${_share}/applications"
    )
    foreach(_type IN ITEMS app usth ust uar)
        if(_type STREQUAL "app")
            set(_context apps)
            set(_name "${HELLOUTAU_APPLICATION_ID}")
        else()
            set(_context mimetypes)
            set(_name "application-x-${_type}")
            install(FILES "Resources/mime/${HELLOUTAU_APPLICATION_ID}.${_type}.xml"
                DESTINATION "${_share}/mime/packages"
            )
        endif()
        set(_drawings "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icons/${_type}/${_type}")
        install(FILES "${_drawings}.svg"
            DESTINATION "${_share}/icons/hicolor/scalable/${_context}" RENAME "${_name}.svg"
        )
        foreach(_size IN ITEMS 16 24 32 48)
            install(FILES "${_drawings}-${_size}.svg"
                DESTINATION "${_share}/icons/hicolor/${_size}x${_size}/${_context}"
                RENAME "${_name}.svg"
            )
        endforeach()
    endforeach()
endif()

# The resource with the icon and the product name, which conf.cmake leaves to an application
qm_add_win_rc(${PROJECT_NAME}
    NAME "HelloUtau"
    DESCRIPTION "${PROJECT_DESCRIPTION}"
    COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
    ICON "${_app_icon}"
)

# The manifest of qmsetup, without the UTF-8 code page, so that GetACP() reports the ANSI one
qm_add_win_manifest(${PROJECT_NAME} NAME "HelloUtau")

# HelloUtau.app on macOS, whose name the root CMakeLists.txt gives as HELLO_MACOSX_BUNDLE_NAME.
# qm_add_mac_bundle names the executable after the bundle.
if(APPLE)
    # The icons of the file types, which Info.plist.in names, in Contents/Resources
    set_source_files_properties(${_file_icons} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # The localizations of the bundle, English and the languages of the translations. macOS gives
    # the application the current locale in the preferred language of the user only if the
    # bundle lists that language, and gives a bundle that lists English alone an English locale
    # such as en_CN, in which the translations are not loaded and the panels of the system are in
    # English.
    set(HELLOUTAU_BUNDLE_LOCALIZATIONS "<string>en</string>")
    foreach(_language IN LISTS HELLOUTAU_LANGUAGES)
        string(APPEND HELLOUTAU_BUNDLE_LOCALIZATIONS "\n\t\t<string>${_language}</string>")
    endforeach()

    qm_add_mac_bundle(${PROJECT_NAME}
        NAME "${HELLO_MACOSX_BUNDLE_NAME}"
        DESCRIPTION "${PROJECT_DESCRIPTION}"
        COPYRIGHT "${HELLOUTAU_RC_COPYRIGHT}"
        ICON "${_app_icon}"
        INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist.in"
    )
    # The identifier is the AppId of the installer of Windows, and the short version has the
    # three parts that macOS shows, where qm_add_mac_bundle sets two.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "${HELLOUTAU_APPLICATION_ID}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING
            "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
    )
endif()
