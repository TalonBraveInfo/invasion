project(fgdlib)

add_library(fgdlib STATIC
	gamedata.cpp
	gdclass.cpp
	gdvar.cpp
	inputoutput.cpp
	wckeyvalues.cpp)

target_include_directories(fgdlib PRIVATE
		../utils/common/
		../public/fgdlib/
		../public/tier0/
		../public/tier1/)
