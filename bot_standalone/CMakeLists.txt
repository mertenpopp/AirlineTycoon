cmake_minimum_required (VERSION 3.10)

project (AT_bot)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_BUILD_TYPE Release)
    add_compile_definitions(NDEBUG)
endif()

if(MSVC)
    add_compile_options("/MP")
    add_compile_options("-m")
else()
    add_compile_options("-Wall")
    add_compile_options("-Wextra")
    add_compile_options("-Wno-format-nonliteral")
    add_compile_options("-Wno-sign-compare")
    add_compile_options("-Wno-type-limits")

    add_compile_options("-Wcast-align")
    add_compile_options("-Wcast-qual")
    add_compile_options("-Wctor-dtor-privacy")
    add_compile_options("-Wdisabled-optimization")
    add_compile_options("-Wfloat-equal")
    add_compile_options("-Wformat=2")
    add_compile_options("-Winit-self")
    add_compile_options("-Winline")
    add_compile_options("-Wlogical-op")
    add_compile_options("-Wmissing-declarations")
    add_compile_options("-Wmissing-include-dirs")
    add_compile_options("-Wnoexcept")
    #add_compile_options("-Wold-style-cast")
    add_compile_options("-Woverloaded-virtual")
    add_compile_options("-Wredundant-decls")
    #add_compile_options("-Wshadow")
    #add_compile_options("-Wsign-conversion")
    add_compile_options("-Wsign-promo")
    add_compile_options("-Wstrict-null-sentinel")
    add_compile_options("-Wstrict-overflow=2")
    add_compile_options("-Wswitch-default")
    add_compile_options("-Wundef")
    add_compile_options("-Wunsafe-loop-optimizations")
endif()

include_directories(${PROJECT_SOURCE_DIR})

# Add source to this project's executable.
add_executable (AT_bot
    Auftrag.cpp
    BotHelper.cpp
    BotPlanerAlgo.cpp
    BotPlaner.cpp
    compat_global.cpp
    compat_misc.cpp
    Fracht.cpp
    GameMechanic.cpp
    main.cpp
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    target_compile_features(AT PUBLIC cxx_std_17)
    target_compile_options(AT PUBLIC "/Zc:__cplusplus")
    target_compile_options(AT PUBLIC "/std:c++17")
endif()

