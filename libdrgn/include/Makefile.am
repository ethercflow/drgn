# "Install" the elfutils headers to the build directory.

BUILT_SOURCES = dwarf.h \
		elf.h \
		elfutils/elf-knowledge.h \
		elfutils/known-dwarf.h \
		elfutils/libasm.h \
		elfutils/libdw.h \
		elfutils/libdwelf.h \
		elfutils/libdwfl.h \
		elfutils/version.h \
		gelf.h \
		libelf.h \
		nlist.h

dwarf.h: ../elfutils/libdw/dwarf.h
elf.h: ../elfutils/libelf/elf.h
elfutils/elf-knowledge.h: ../elfutils/libelf/elf-knowledge.h
elfutils/known-dwarf.h: ../elfutils/libdw/known-dwarf.h
elfutils/libasm.h: ../elfutils/libasm/libasm.h
elfutils/libdw.h: ../elfutils/libdw/libdw.h
elfutils/libdwelf.h: ../elfutils/libdwelf/libdwelf.h
elfutils/libdwfl.h: ../elfutils/libdwfl/libdwfl.h
elfutils/version.h: ../elfutils/version.h
gelf.h: ../elfutils/libelf/gelf.h
libelf.h: ../elfutils/libelf/libelf.h
nlist.h: ../elfutils/libelf/nlist.h

$(BUILT_SOURCES):
	mkdir -p $$(dirname $@)
	cp $< $@
