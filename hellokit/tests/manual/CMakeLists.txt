# Programs for manual execution, not run by ctest. They provide integration testing of the
# libraries, which the automatic tests cannot provide, because each of those covers one class
# with synthetic inputs. They are development tools rather than part of the product, and
# therefore not installed.
add_subdirectory(ustconv)
add_subdirectory(ustedit)
add_subdirectory(ustrender)
add_subdirectory(utaucompare)
add_subdirectory(utauprobe)
