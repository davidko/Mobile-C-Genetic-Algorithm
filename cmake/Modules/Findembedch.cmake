# - Try to find Embedded-Ch libraries
# Once this is done, it should define:
# EMBEDCH_FOUND - System has found mobile-c
# EMBEDCH_INCLUDE_DIRS - The Mobile-C include directories
# EMBEDCH_LIBRARIES - The libraries required to use Mobile-C

find_path(EMBEDCH_INCLUDE_DIR embedch.h HINTS /usr/local/ch/extern/include C:/Ch/extern/include)
find_library(EMBEDCH_LIBRARY NAMES embedch libembedch HINTS /usr/local/ch/extern/lib C:/Ch/extern/lib)
set(EMBEDCH_LIBRARIES ${EMBEDCH_LIBRARY})
set(EMBEDCH_INCLUDE_DIRS ${EMBEDCH_INCLUDE_DIR})

mark_as_advanced(EMBEDCH_INCLUDE_DIR EMBEDCH_LIBRARY)
