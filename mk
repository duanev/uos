# common make rules for all arch/*/mach/*/mk files
# must be used with a current working dir one above uos
# (ie. a build-xxx directory)

vpath %.s   ../arch/$(ARCH)/mach/$(MACH) ../arch/$(ARCH)
vpath %.c   ../arch/$(ARCH)/mach/$(MACH) ../arch/$(ARCH) ../lib ../apps/$(APP)

INC = -I../inc -I../arch/$(ARCH) -I../arch/$(ARCH)/mach/$(MACH)

.c.o:
	$(TOOLCHAIN_PFX)gcc $(DFLAGS) $(CFLAGS) $(INC) -c $< -o $(@F)
	$(TOOLCHAIN_PFX)objdump -S $(@F) > $(notdir $*).lst

# using gcc to assemble so .ifdef is understood
.s.o:
	$(TOOLCHAIN_PFX)gcc -x assembler-with-cpp $(DFLAGS) $(AFLAGS) -c $< -o $(@F)
	$(TOOLCHAIN_PFX)objdump -d $(@F) > $(notdir $*).lst

$(APP).elf: ../arch/$(ARCH)/mach/$(MACH)/$(LDSCRIPT).ld $(SRCS:=.o)
	@echo srcs $(SRCS)
	$(TOOLCHAIN_PFX)ld -T $^ -Map $(APP).map -o $(APP).elf

$(APP).elf: $(SRCS:=.o)

clean:
	rm -f *.o *.elf

distclean:
	rm -f *.o *.elf *.lst *.bin *.map


# maybe need more of these ... like one per lib*.c
libarch.o: ../arch/$(ARCH)/arch.h

uos.h: ../inc/_bitmap.h  ../inc/_c.h  ../inc/_gate.h  ../inc/_mem.h  ../inc/_smp.h  ../inc/_types.h

