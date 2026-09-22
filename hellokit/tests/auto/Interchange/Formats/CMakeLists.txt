# Links wolf-midi directly, which the library links privately. The test cases generate the MIDI
# files they read, so that the input of each case is defined next to its assertions.
add_auto_test(test_MidiConvert.cpp HelloKitInterchange wolf-midi::wolf-midi)
