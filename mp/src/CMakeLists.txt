cmake_minimum_required(VERSION 3.21)

project(invasion_all)

include_directories(public/)

set(LIBPUBLIC lib/public/2012/)
set(LIBCOMMON lib/common/)

add_subdirectory(tier1)
#add_subdirectory(fgdlib)
#add_subdirectory(game/client)
#add_subdirectory(game/server)
