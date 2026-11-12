cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()
if(NOT DEFINED PACKAGE_ROOT OR NOT DEFINED DEPENDENCY_ROOT)
    message(FATAL_ERROR "PACKAGE_ROOT and DEPENDENCY_ROOT are required")
endif()
file(GLOB_RECURSE package_executables "${PACKAGE_ROOT}/*.exe")
file(GLOB_RECURSE package_libraries "${PACKAGE_ROOT}/*.dll")
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${package_executables}
    LIBRARIES ${package_libraries}
    DIRECTORIES "${PACKAGE_ROOT}/bin" "${DEPENDENCY_ROOT}/bin"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    CONFLICTING_DEPENDENCIES_PREFIX conflicts
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES "^[Cc]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
)
if(unresolved)
    message(FATAL_ERROR "Unresolved runtime dependencies: ${unresolved}")
endif()
# windeployqt patches Qt6Core's install prefix. Prefer the deployed copy,
# while requiring every other conflicting external library to be identical.
foreach(name IN LISTS conflicts_FILENAMES)
    if(EXISTS "${PACKAGE_ROOT}/bin/${name}")
        file(SHA256 "${PACKAGE_ROOT}/bin/${name}" deployed_hash)
        foreach(candidate IN LISTS "conflicts_${name}")
            file(SHA256 "${candidate}" candidate_hash)
            if(NOT name STREQUAL "Qt6Core.dll" AND NOT candidate_hash STREQUAL deployed_hash)
                message(FATAL_ERROR "Deployed library differs from external library named ${name}: ${candidate}")
            endif()
        endforeach()
        list(APPEND resolved "${PACKAGE_ROOT}/bin/${name}")
    else()
        set(previous_hash "")
        foreach(candidate IN LISTS "conflicts_${name}")
            file(SHA256 "${candidate}" candidate_hash)
            if(previous_hash AND NOT candidate_hash STREQUAL previous_hash)
                message(FATAL_ERROR "Different external libraries named ${name}: ${conflicts_${name}}")
            endif()
            set(previous_hash "${candidate_hash}")
        endforeach()
        list(GET "conflicts_${name}" 0 candidate)
        list(APPEND resolved "${candidate}")
    endif()
endforeach()
set(provenance "")
foreach(library IN LISTS resolved)
    get_filename_component(name "${library}" NAME)
    if(NOT EXISTS "${PACKAGE_ROOT}/bin/${name}")
        file(COPY "${library}" DESTINATION "${PACKAGE_ROOT}/bin")
    endif()
    string(APPEND provenance "${name}\t${library}\n")
endforeach()
file(WRITE "${PACKAGE_ROOT}/runtime-dependencies.tsv" "${provenance}")
list(LENGTH resolved count)
message(STATUS "Resolved ${count} non-system runtime libraries")
