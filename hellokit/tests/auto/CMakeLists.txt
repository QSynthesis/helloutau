qm_find_qt(Core)

find_package(wolf-midi CONFIG REQUIRED)

add_auto_test(test_PayloadCodec.cpp HelloKitDocument)
add_auto_test(test_Interchange.cpp HelloKitInterchange HelloKitDocument HelloKitSupport)

# Links wolf-midi directly, which the library keeps private. The cases build the MIDI files they
# read, so that what each one relies on sits next to what it asserts.
add_auto_test(test_MidiReader.cpp HelloKitInterchange HelloKitDocument HelloKitSupport wolf-midi::wolf-midi)
