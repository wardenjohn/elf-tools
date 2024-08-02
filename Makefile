include Makefile.inc

SUBDIRS = main

.PHONY: all $(SUBDIRS) 

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

