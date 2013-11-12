dynamic_name := ogg
path_step := ../zxtune
include $(path_step)/make/default.mak 

$(platform)_definitions += LIBOGG_EXPORTS

include_dirs = include
source_files_cc := src/bitwise src/framing

ld_flags = /DEF:win32\ogg.def

include $(path_step)/makefile.mak
