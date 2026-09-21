# Links wolf-midi directly, which the library keeps private. The cases build the MIDI files they
# read, so that what each one relies on sits next to what it asserts.
add_auto_test(test_MidiConvert.cpp HelloKitInterchange wolf-midi::wolf-midi)
