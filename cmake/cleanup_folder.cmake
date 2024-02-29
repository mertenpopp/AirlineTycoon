cmake_minimum_required(VERSION 3.5)

# Usage: cleanup_folder.cmake <dir> [match] - destructive
set(target_directory "${CMAKE_ARGV3}")
set(match_regex "${CMAKE_ARGV4}")

file(GLOB FILES_TO_REMOVE "${target_directory}/*")

if (match_regex)
    list(FILTER FILES_TO_REMOVE INCLUDE REGEX "${match_regex}")
endif()

foreach(FILE IN LISTS FILES_TO_REMOVE)
    if(NOT IS_DIRECTORY ${FILE})
        file(REMOVE ${FILE})
    endif()
endforeach()
