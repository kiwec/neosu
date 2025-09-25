# can't just use sed to add a single line :)
file(READ "${FILE_TO_PATCH}" FILE_CONTENT)

# add #include <chrono> after #include <condition_variable>
string(REPLACE
    "#include <condition_variable>"
    "#include <condition_variable>\n#include <chrono>"
    FILE_CONTENT
    "${FILE_CONTENT}"
)

file(WRITE "${FILE_TO_PATCH}" "${FILE_CONTENT}")
message(STATUS "patched ${FILE_TO_PATCH} to include <chrono>")
