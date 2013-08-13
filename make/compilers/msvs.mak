tools.cxx = $($(platform).$(arch).execprefix)cl
tools.cc = $($(platform).$(arch).execprefix)cl
tools.ld ?= $($(platform).$(arch).execprefix)link
tools.ar ?= $($(platform).$(arch).execprefix)lib
tools.mt ?= $($(platform).$(arch).execprefix)mt

#set options according to mode
ifdef release
CXX_MODE_FLAGS = /Ox /MD /DNDEBUG /D_HAS_ITERATOR_DEBUGGING=0 /D_SECURE_SCL=0 
LD_MODE_FLAGS = /SUBSYSTEM:$(if $(have_gui),WINDOWS,CONSOLE)
else
CXX_MODE_FLAGS = /Od /MDd /D_HAS_ITERATOR_DEBUGGING=1 /D_SECURE_SCL=1
LD_MODE_FLAGS = /SUBSYSTEM:CONSOLE
endif

ifdef pic
CXX_MODE_FLAGS += /LD
LD_MODE_FLAGS += /DLL
endif

#specific
DEFINITIONS = $(defines) $($(platform)_definitions)
INCLUDES = $(include_dirs) $($(platform)_include_dirs)
windows_libraries += kernel32 $(addsuffix $(if $(release),,d), msvcrt msvcprt)

#setup flags
CXXFLAGS = /nologo /c $(CXX_MODE_FLAGS) $(cxx_flags) \
	/W3 \
	$(addprefix /D, $(DEFINITIONS) $($(platform).definitions) $($(platform).$(arch).definitions)) \
	/J /Zc:wchar_t,forScope /Z7 /Zl /EHsc \
	/GA /GF /Gy /Y- /GR \
	$(addprefix /I, $(INCLUDES))

ARFLAGS = /NOLOGO /NODEFAULTLIB

LDFLAGS = /NOLOGO $(LD_MODE_FLAGS) $($(platform).ld.flags) $($(platform).$(arch).ld.flags) $(ld_flags) \
	/INCREMENTAL:NO /DEBUG\
	/IGNORE:4217 /IGNORE:4049\
	/OPT:REF,ICF=5 /NODEFAULTLIB

build_obj_cmd = $(tools.cxx) $(CXXFLAGS) /Fo$2 $1
build_obj_cmd_nodeps = $(build_obj_cmd)
build_obj_cmd_cc = $(build_obj_cmd)
build_lib_cmd = $(tools.ar) $(ARFLAGS) /OUT:$2 $1
#ignore some warnings for Qt
link_cmd = $(tools.ld) $(LDFLAGS) /OUT:$@ $(OBJECTS) $(RESOURCES) \
	$(if $(libraries),/LIBPATH:$(libs_dir) $(addsuffix .lib,$(libraries)),)\
	$(if $(dynamic_libs),/LIBPATH:$(output_dir) $(addprefix /DELAYLOAD:,$(addsuffix .dll,$(dynamic_libs))) $(addsuffix .lib,$(dynamic_libs)),)\
	$(addprefix /LIBPATH:,$($(platform)_libraries_dirs))\
	$(addsuffix .lib,$(sort $($(platform)_libraries)))\
	/PDB:$@.pdb /PDBPATH:none

postlink_cmd = $(tools.mt) -manifest $@.manifest -outputresource:$@ || ECHO No manifest
