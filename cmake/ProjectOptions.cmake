include(CheckIPOSupported)

if(QTROCKET_ENABLE_IPO)
    check_ipo_supported(RESULT QTROCKET_IPO_SUPPORTED OUTPUT ipo_output)
    if(NOT QTROCKET_IPO_SUPPORTED)
        message(WARNING "QTROCKET_ENABLE_IPO is ON, but this toolchain cannot do IPO:\n${ipo_output}")
    endif()
endif()

if("thread" IN_LIST QTROCKET_SANITIZERS AND "address" IN_LIST QTROCKET_SANITIZERS)
    message(FATAL_ERROR "QTROCKET_SANITIZERS: 'thread' cannot be combined with 'address'")
endif()

if(QTROCKET_SANITIZERS AND MSVC)
    message(FATAL_ERROR "QTROCKET_SANITIZERS needs GCC or Clang (asan/tsan presets: Linux, macOS)")
endif()

if(QTROCKET_ENABLE_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "QTROCKET_ENABLE_COVERAGE uses llvm-cov and needs Clang. Use the coverage preset.")
endif()

if(QTROCKET_ENABLE_CLANG_TIDY)
    # Xcode has no clang-tidy; on macOS it comes from Homebrew's llvm, which is not on PATH.
    find_program(CLANG_TIDY_PROGRAM clang-tidy
                 HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin REQUIRED)
    # GCC-only -W flags are unknown to clang-tidy's Clang frontend; don't report them.
    set(QTROCKET_CLANG_TIDY_COMMAND ${CLANG_TIDY_PROGRAM} --extra-arg=-Wno-unknown-warning-option)
    if(QTROCKET_WARNINGS_AS_ERRORS)
        list(APPEND QTROCKET_CLANG_TIDY_COMMAND --warnings-as-errors=*)
    endif()
endif()

# QtRocket_configure_target(<target>)
# Applies warnings, sanitizers, coverage, clang-tidy and IPO to one of *our* targets (never to dependencies).
# Call it for every target you add.
function(QtRocket_configure_target target)
    if(MSVC)   # cl, and clang-cl (which takes the same options)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8 /Zc:__cplusplus $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
            # Off-by-default warnings, enabled at level 1: narrowing conversions (4242, 4254, 4826),
            # a missed override (4263), a non-virtual destructor (4265), always-false comparisons
            # (4287, 4296), pointer truncation (4311), comma/no-effect mistakes (4545-4555),
            # thread-unsafe statics (4640), string literal casts (4905, 4906), copy-init (4928).
            /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549
            /w14555 /w14640 /w14826 /w14905 /w14906 /w14928
            $<$<BOOL:${QTROCKET_WARNINGS_AS_ERRORS}>:/WX>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor
            -Wold-style-cast -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2
            -Wimplicit-fallthrough -Wcast-align
            $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>
            $<$<BOOL:${QTROCKET_WARNINGS_AS_ERRORS}>:-Werror>)
        # Formulas ported from OpenRocket (Java) rely on every product being rounded before it is added:
        # Java forbids fused multiply-add, but GCC and Clang contract a*b+c into one by default, which on
        # an FMA target (Apple Silicon, -march=native) changes the last bit and breaks exact comparisons
        # such as Line2D.relativeCCW's px*y2 - py*x2 == 0. MSVC's /fp:precise never contracts.
        target_compile_options(${target} PRIVATE -ffp-contract=off)
        # Bounds-checked operator[] etc. in the standard library, ABI-compatible (unlike _GLIBCXX_DEBUG):
        # libstdc++ (Linux) and libc++ (macOS) each ignore the other's macro. MSVC's Debug STL checks itself.
        target_compile_definitions(${target} PRIVATE
            $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
            $<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>)

        if(QTROCKET_SANITIZERS)
            list(JOIN QTROCKET_SANITIZERS "," sanitizers)
            set(sanitize -fsanitize=${sanitizers})
            if("undefined" IN_LIST QTROCKET_SANITIZERS)
                list(APPEND sanitize -fno-sanitize-recover=all)   # UB stops the program, so a test fails
            endif()
            target_compile_options(${target} PRIVATE ${sanitize} -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC ${sanitize})
            if("address" IN_LIST QTROCKET_SANITIZERS)
                # libstdc++ annotates std::vector's spare capacity for ASan only on request (libc++ does it by
                # default), so reads past size() but within capacity() are caught. Ignored by libc++ and MSVC.
                target_compile_definitions(${target} PRIVATE _GLIBCXX_SANITIZE_VECTOR)
            endif()
        endif()
    endif()

    if(QTROCKET_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target} PUBLIC -fprofile-instr-generate)
    endif()

    if(QTROCKET_ENABLE_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${QTROCKET_CLANG_TIDY_COMMAND}")
    endif()

    if(QTROCKET_ENABLE_IPO AND QTROCKET_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endfunction()

# QtRocket_configure_qt_target(<target>)
# QtRocket_configure_target() plus what every Qt target of ours (QtRocket_gui, the QtRocket application and
# QtRocket_gui_tests) needs. Call it instead of QtRocket_configure_target() for a target that uses Qt.
# - AUTOMOC on this target only: a global CMAKE_AUTOMOC (qt_standard_project_setup() sets one) would also give the
#   core, its tests and GoogleTest moc steps. AUTOMOC reads only the headers that are sources of the target, so every
#   header is listed in its area's CMakeLists.txt (an unlisted Q_OBJECT header fails to link: "undefined reference
#   to vtable"). No AUTOUIC (no .ui files) and no AUTORCC (resources go through qt_add_resources()).
# - QT_NO_KEYWORDS: no signals/slots/emit/foreach macros (Q_SIGNALS, Q_EMIT); Qt's emit macro would break the core's
#   Signal::emit() (util/Signal.h refuses to compile under it).
# - QT_ENABLE_STRICT_MODE_UP_TO=0x060800: nothing that Qt 6.8 deprecates (QT_DISABLE_DEPRECATED_UP_TO), no
#   contextless connect(), no narrowing in connect(), no implicit QString/QByteArray/QUrl conversions, ...
# - QT_WARN_DEPRECATED_UP_TO=0x060800: Qt warns about every deprecation up to 7.0 by default, which with -Werror
#   breaks the build on a newer Qt for APIs whose replacement Qt 6.8 (the oldest Qt supported, CI's on macOS and
#   Windows) does not have yet.
# - QT_NO_CAST_FROM_ASCII: every string literal is tr() (user-visible text) or QStringLiteral.
function(QtRocket_configure_qt_target target)
    QtRocket_configure_target(${target})
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    target_compile_definitions(${target} PRIVATE
        QT_NO_KEYWORDS
        QT_ENABLE_STRICT_MODE_UP_TO=0x060800
        QT_WARN_DEPRECATED_UP_TO=0x060800
        QT_NO_CAST_FROM_ASCII)
endfunction()

# QtRocket_automoc_follows_sources(<target>)
# Makes AUTOMOC run again whenever the list of sources of <target> changes. AUTOMOC runs again when a CMakeLists.txt
# file of the directory that made its target changes, but not when one of a subdirectory does: a Q_OBJECT header
# added to an area's list (gui/src/<area>/CMakeLists.txt, tests/gui/<area>/CMakeLists.txt) would not be moc'ed in an
# existing build directory, and the build would fail with "undefined reference to vtable" until a clean build. This
# writes a hash of the list into a file of the build directory (only when it changes) and reads it as a list file of
# the calling directory, which AUTOMOC then depends on. Call it in the directory that made <target>, after every
# subdirectory that adds sources to it.
function(QtRocket_automoc_follows_sources target)
    get_target_property(sources ${target} SOURCES)
    string(SHA256 sources_hash "${sources}")
    set(stamp ${CMAKE_CURRENT_BINARY_DIR}/${target}_sources.cmake)
    file(CONFIGURE OUTPUT ${stamp} CONTENT "# The sources of ${target}, for AUTOMOC: ${sources_hash}\n")
    include(${stamp})
endfunction()
