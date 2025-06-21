-include .config

export ROOT_PATH := ../../..

CC := gcc
LD := ld
CFLAGS := -Wall -O3

SUFFIX = a
dir-y := $(ROOT_PATH)/src
obj_build = -f obj.mk path=
lib_output_dir = output
lib_target = $(lib_output_dir)/libweb_server.$(SUFFIX)
demo_output_dir = release


all : $(lib_target)


.PHONY: $(dir-y)


$(dir-y).a:
	@find $(dir-y) -name *.o -delete
	@make $(obj_build)$(dir-y)


$(dir-y).so:
	@find $(dir-y) -name *.o -delete
	@make $(obj_build)$(dir-y) SUFFIX=so


$(lib_output_dir)/libweb_server.a: $(dir-y).a
	@mkdir -p $(lib_output_dir)
	@find $(dir-y) -name *.o -exec $(AR) cr $@ {} +
	@ranlib $@


$(lib_output_dir)/libweb_server.so: $(dir-y).so
	@mkdir -p $(lib_output_dir)
	@find $(dir-y) -name *.o -exec $(CC) -shared -o $(lib_output_dir)/libweb_server.so {} +


release: $(lib_output_dir)/libweb_server.a $(lib_output_dir)/libweb_server.so
	@mkdir -p $(demo_output_dir)
	@cp -a $(ROOT_PATH)/demo/* $(demo_output_dir)
	@cp -a $(lib_output_dir) $(demo_output_dir)/lib
	@cp -a $(ROOT_PATH)/include $(demo_output_dir)
	@cp Makefile.demo $(demo_output_dir)/Makefile


clean:
	@for dir in $(dir-y); do \
		make $(obj_build)$$dir clean; \
	done
	rm -vf *.o *.a
	rm -rf $(lib_output_dir)
	rm -rf $(demo_output_dir)

	
distclean: clean
	@rm -vf .config include/autoconfig.h
	@echo
