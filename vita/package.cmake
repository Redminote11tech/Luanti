include("${VITASDK}/share/vita.cmake" REQUIRED)
target_sources(luanti PRIVATE "${CMAKE_SOURCE_DIR}/src/porting_vita.cpp")
target_link_libraries(luanti SceNet_stub SceNetCtl_stub SceSysmodule_stub
	SceLibKernel_stub ScePower_stub SceAppMgr_stub bz2)
vita_create_self(eboot.bin luanti UNSAFE STRIPPED MEMSIZE 0x40000)
# Data is read from app0:; writes go to ux0:data/luanti.
set(vita_assets
	FILE "${CMAKE_SOURCE_DIR}/vita/livearea/icon0.png" "sce_sys/icon0.png"
	FILE "${CMAKE_SOURCE_DIR}/vita/livearea/bg.png" "sce_sys/livearea/contents/bg.png"
	FILE "${CMAKE_SOURCE_DIR}/vita/livearea/startup.png" "sce_sys/livearea/contents/startup.png"
	FILE "${CMAKE_SOURCE_DIR}/LICENSE.txt" "LICENSE.txt"
	FILE "${CMAKE_SOURCE_DIR}/vita/livearea/template.xml" "sce_sys/livearea/contents/template.xml")
foreach(resource builtin client/shaders textures/base/pack fonts)
	file(GLOB_RECURSE files RELATIVE "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/${resource}/*")
	foreach(file IN LISTS files)
		if(NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${file}")
			list(APPEND vita_assets FILE "${CMAKE_SOURCE_DIR}/${file}" "${file}")
		endif()
	endforeach()
endforeach()
vita_create_vpk(luanti-vita.vpk "LUANTI001" eboot.bin
	VERSION "01.00" NAME "Luanti Vita" ${vita_assets})
