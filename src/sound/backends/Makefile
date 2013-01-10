library_name := sound_backends
path_step := ../../..
source_files = backend_impl enumerator file_backend scope volume_control

text_files := text/backends
po_files := sound_backends

include $(path_step)/make/default.mak
include $(path_step)/make/features.mak

define declare_native_backend
ifdef support_$(1)
generated_headers += $(2)_api
generated_sources += $(2)_api_dynamic
source_files += $(2)_backend
else
source_files += $(2)_stub
endif
endef

define declare_thirdparty_backend
ifdef support_$(1)
$$(platform)_include_dirs += $$(path_step)/3rdparty
generated_headers += $(1)_api
generated_sources += $(1)_api_dynamic
source_files += $(1)_backend
else
source_files += $(1)_stub
endif
endef

$(eval $(call declare_native_backend,waveout,win32))
$(eval $(call declare_native_backend,directsound,dsound))
$(eval $(call declare_native_backend,alsa,alsa))
$(eval $(call declare_native_backend,sdl,sdl))

ifdef support_oss
source_files += oss_backend
else
source_files += oss_stub
endif

$(eval $(call declare_thirdparty_backend,flac))
$(eval $(call declare_thirdparty_backend,mp3))
$(eval $(call declare_thirdparty_backend,ogg))

ifdef support_ogg
generated_headers += vorbis_api vorbisenc_api
source_files += vorbis_api_dynamic vorbisenc_api_dynamic
endif

source_files += null_backend wav_backend

include $(path_step)/makefile.mak

ifndef NO_PERL
%_api_dynamic$(src_suffix): templates/%_api_dynamic$(src_suffix).templ functions/%.dat
	perl $(path_step)/make/build_shgate_dynamic.pl $^ > $@

%_api.h: templates/%_api.h.templ functions/%.dat
	perl $(path_step)/make/build_shgate_dynamic.pl $^ > $@
endif