ui_dir = $(objects_dir)/.ui
ui_headers = $(addprefix $(ui_dir)/,$(notdir $(ui_files:=.ui)))
moc_sources = $(ui_files:=.moc) $(moc_files:=.moc)

source_files += $(ui_files) $(moc_files)
generated_headers += $(ui_headers)
generated_sources += $(moc_sources)

include_dirs += $(ui_dir)

defines += QT_LARGEFILE_SUPPORT QT_GUI_LIB QT_CORE_LIB QT_THREAD_SUPPORT

qrc_sources = $(qrc_files:=.qrc)
generated_sources += $(qrc_sources)

ifneq (,$(findstring Core,$(qt_libraries)))
windows_libraries += kernel32 user32 shell32 uuid ole32 advapi32 ws2_32 oldnames
mingw_libraries += kernel32 user32 shell32 uuid ole32 advapi32 ws2_32 z
dingux_libraries += z
endif
ifneq (,$(findstring Gui,$(qt_libraries)))
windows_libraries += gdi32 comdlg32 oleaut32 imm32 winmm winspool ws2_32 ole32 user32 advapi32 oldnames
mingw_libraries += gdi32 comdlg32 oleaut32 imm32 winmm winspool ws2_32 ole32 uuid user32 advapi32 png z
dingux_libraries += png z
endif

vpath %.qrc $(sort $(dir $(qrc_files)))
vpath %.ui $(sort $(dir $(ui_files)))
vpath %.h $(sort $(dir $(moc_files) $(ui_files)))

$(ui_dir):
	$(call makedir_cmd,$@)

UIC := uic
MOC := moc
RCC := rcc

$(ui_dir)/%.ui.h: %.ui | $(ui_dir)
	$(UIC) $< -o $@

%.moc$(src_suffix): %.h
	$(MOC) $(addprefix -D,$(DEFINITIONS)) $< -o $@

%.qrc$(src_suffix): %.qrc
	$(RCC) -name $(basename $(notdir $<)) -o $@ $<

#disable dependencies generation for generated files
$(objects_dir)/%$(call makeobj_name,.moc): %.moc$(src_suffix)
	$(call build_obj_cmd_nodeps,$(CURDIR)/$<,$@)

$(objects_dir)/%$(call makeobj_name,.qrc): %.qrc$(src_suffix)
	$(call build_obj_cmd_nodeps,$(CURDIR)/$<,$@)