cmake_minimum_required(VERSION 3.5)

# Usage: copy_if_different.cmake <source_dir> <destination_dir>
set(source_dir "${CMAKE_ARGV3}")
set(destination_dir "${CMAKE_ARGV4}")

file(GLOB_RECURSE source_files RELATIVE "${source_dir}" "${source_dir}/*")

foreach(file ${source_files})
    set(src "${source_dir}/${file}")
    set(dst "${destination_dir}/${file}")

    if(NOT EXISTS "${dst}" OR "${src}" IS_NEWER_THAN "${dst}")
        # Get the directory part of the destination file path
        get_filename_component(dst_dir "${dst}" DIRECTORY)

        # Create the directory if it doesn't exist
        file(MAKE_DIRECTORY "${dst_dir}")

        file(COPY "${src}" DESTINATION "${dst_dir}")
    endif()
endforeach()