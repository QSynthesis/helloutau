add_auto_test(test_SoftwareRelease.cpp HelloKitUpdate)
add_auto_test(test_Sha256Sums.cpp HelloKitUpdate)

if(WIN32)
    add_auto_test(test_InnoSetup.cpp HelloKitUpdate)
endif()
