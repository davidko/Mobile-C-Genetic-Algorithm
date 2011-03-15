# - Try to find Mobile-C libraries
# Once this is done, it should define:
# MOBILEC_FOUND - System has found mobile-c
# MOBILEC_INCLUDE_DIRS - The Mobile-C include directories
# MOBILEC_LIBRARIES - The libraries required to use Mobile-C
# MOBILEC_DEFINITIONS - Compiler switches required for Mobile-C

find_package(PkgConfig)
pkg_check_modules(PC_MOBILEC QUIET mobilec)
set(MOBILEC_DEFINITIONS ${PC_MOBILEC_CFLAGS_OTHER})
find_path(MOBILEC_INCLUDE_DIR libmc.h HINTS ${PC_MOBILEC_INCLUDEDIR} ${PC_MOBILEC_INCLUDE_DIRS})
find_library(MOBILEC_LIBRARY NAMES mc libmc HINTS ${PC_MOBILEC_LIBDIR} ${PC_MOBILEC_LIBRARY_DIRS})
set(MOBILEC_LIBRARIES ${MOBILEC_LIBRARY})
set(MOBILEC_INCLUDE_DIRS ${MOBILEC_INCLUDE_DIR})

# find_package_handle_standard_args(MobileC DEFAULT_MSG MOBILEC_LIBRARY MOBILEC_INCLUDE_DIR)
mark_as_advanced(MOBILEC_INCLUDE_DIR MOBILEC_LIBRARY)
