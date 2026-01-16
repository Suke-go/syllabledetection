project(libsyllable_examples)

add_executable(process_wav process_wav.c)
target_link_libraries(process_wav PRIVATE syllable)
