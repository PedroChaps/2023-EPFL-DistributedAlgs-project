# DO NOT CHANGE THE SYMBOLIC VARIABLE `SOURCES`.
# You can, however, change the list of files that comprise this variable.

include_directories(include)
set(SOURCES src/main.cpp src/hello.c src/Process/Process.cpp src/Process/Process.h src/Process/Receiver.cpp src/Process/Receiver.h src/Process/Sender.cpp src/Process/Sender.h src/Link/Link.cpp src/Link/Link.h src/Link/PerfectLink.cpp src/Link/PerfectLink.h)

# DO NOT EDIT THE FOLLOWING LINES
find_package(Threads)
add_executable(da_proc ${SOURCES})
target_link_libraries(da_proc ${CMAKE_THREAD_LIBS_INIT})
