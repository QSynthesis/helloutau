hellokit_add_auto_test(test_SoftwareRelease.cpp HelloKitUpdate)
hellokit_add_auto_test(test_SoftwareReleaseDownload.cpp HelloKitUpdate)
hellokit_add_auto_test(test_Sha256Sums.cpp HelloKitUpdate)

if(WIN32)
    hellokit_add_auto_test(test_InnoSetup.cpp HelloKitUpdate)
endif()
