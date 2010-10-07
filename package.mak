#package generating
#TODO: make proper revision number width
pkg_revision := 0$(shell svnversion $(path_step))
ifneq ($(pkg_revision),$(subst ;,,$(pkg_revision)))
$(error Invalid package revision ($(pkg_revision) - possible mixed))
endif
pkg_subversion := $(if $(subst release,,$(mode)),_dbg,)
pkg_suffix := zip
pkg_lang := en

pkg_dir := $(path_step)/Builds/Revision$(pkg_revision)_$(platform)_$(arch)
pkg_filename := $(binary_name)_r$(pkg_revision)$(pkg_subversion)_$(platform)_$(arch).$(pkg_suffix)
pkg_file := $(pkg_dir)/$(pkg_filename)
pkg_log := $(pkg_dir)/$(binary_name).log
pkg_debug := $(pkg_dir)/$(binary_name)_debug.$(pkg_suffix)
pkg_manual_source := dist/$(binary_name).txt
ifneq ($(wildcard $(pkg_manual_source)),)
pkg_manual := $(pkg_dir)/$(binary_name)_$(pkg_lang).txt
endif
pkg_additional_files := $(wildcard dist/$(platform)/*)

package:
	@$(call makedir_cmd,$(pkg_dir))
	$(info Creating package at $(pkg_dir))
	@$(MAKE) $(pkg_file) > $(pkg_dir)/packaging_$(binary_name).log 2>&1
	$(call rmfiles_cmd,$(pkg_manual) $(pkg_log))

.PHONY: clean_package

clean_package: clean
	-$(call rmdir_cmd,$(pkg_dir))

$(pkg_file): $(pkg_manual) $(pkg_debug)
	$(info Packaging $(binary_name) to $(pkg_filename))
	zip -9Dj $@ $(target) ../zxtune.conf $(pkg_additional_files) $(pkg_manual)

$(pkg_manual): $(pkg_manual_source)
	$(TEXTATOR) --process --keys $(pkg_lang),txt --asm --output $@ $<

$(pkg_debug): $(pkg_log)
	$(info Packaging debug information and build log)
	zip -9Dj $@ $(target).pdb $(pkg_log)

$(pkg_log):
	$(MAKE) defines=ZXTUNE_VERSION=rev$(pkg_revision) > $(pkg_log) 2>&1