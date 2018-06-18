TESTS = \
	helper.py \
	fw-unit \
	fw-req \
	fw-resp \
	fw-fcp \
	snd-unit \
	snd-efw \
	snd-dice \
	snd-dg00x \
	snd-motu

AM_TESTS_ENVIRONMENT = \
	export LD_LIBRARY_PATH="$(abs_top_srcdir)/src/.libs:$(LD_LIBRARY_PATH)"; \
	export GI_TYPELIB_PATH="$(abs_top_srcdir)/src:$(GI_TYPELIB_PATH)";


EXTRA_DIST = \
	$(TESTS) \
	meson.build
