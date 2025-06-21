all:

-include .config

CFLAGS := -Wall -O3 -I../../../include
LDFLAGS := -r

ifeq ($(SUFFIX), so)
CFLAGS_APPEND = -fPIC
endif

path := $(patsubst %/,%,%(path))
obj_build=-f obj.mk path=

include $(path)/Makefile


%.o: %.c
	@$(CC) $(CFLAGS) $(SRC_INCLUDE) $(CFLAGS_APPEND) -o $@ -c $<

PHONY := $(foreach n, $(PHONY), $(path)/$(n))
obj-y := $(foreach n, $(obj-y), $(path)/$(n))



all: $(obj-y) $(dir-y)


PHONY += $(dir-y)
$(dir-y):
	@make $(obj_build)$(path)/$@


clean:
	@for dir in $(dir-y); do \
		make $(obj_build)$(path)/$$dir clean; \
	done
	rm -vf $(path)/*.o
	rm -vf $(path)/*.a

.PHONY: $(PHONY) FORCE

