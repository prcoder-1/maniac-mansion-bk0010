# Maniac Mansion (SCUMM v2) для БК-0010 + СМК-512: ядро в базовом ОЗУ, общая копия
# кода (REPL) во всех страницах ДОЗУ, банки кода A/B в своих страницах
VERSION=0.1
IMG=MANIAC_V$(VERSION).IMG
BUILD_DATE=$(shell date +%d.%m.%Y)
XGCC=/home/prcoder/xgcc
CFLAGS=-std=gnu23 -fomit-frame-pointer -msoft-float -nostartfiles -nodefaultlibs -nostdlib -m10 -Os \
	-fno-if-conversion -fno-reorder-blocks -fno-caller-saves -mlra -I. -I$(XGCC)/include
ASFLAGS=-mno-fpu -mlimited-eis
CC=pdp11-aout-gcc
AS=pdp11-aout-as
LD=pdp11-aout-ld
OBJCOPY=pdp11-aout-objcopy
OUT=bin
PG_A=2
PG_B=3
PG_C=7

LIBGCC=$(shell $(CC) -m10 -msoft-float -print-libgcc-file-name)

KERNEL_OBJ=crt0.o loader.o helpers.o divmulmod.o memory.o font_pc.o con.o state.o platform.o main.o
REPL_OBJ=farcall.o smk.o blit.o gfxv2.o div5.o snd.o proc3.o mathsi.o far_stubs.o
# банк A: интерпретатор; B: комната, актёры, цикл; C: ресурсы, экран, звук
A_OBJ=script.o script_v2.o object.o string.o verbs.o scumm.o
B_OBJ=room.o actor.o costume.o costume_r.o camera.o
C_OBJ=resource.o gfx_v2.o gfx.o font6.o sound.o saveload.o
BANKA_OBJ=tab_A.o $(A_OBJ)
BANKB_OBJ=tab_B.o $(B_OBJ)
BANKC_OBJ=tab_C.o $(C_OBJ) sndseq.o

LFL=$(sort $(wildcard [0-5][0-9].LFL))

all: $(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC $(OUT)/$(IMG)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# as молча обрезает смещение дальнего BR/SOB: проверка по листингу (tools/chkbr.py)
%.o: %.s
	$(AS) $(ASFLAGS) -a=$*.lst -o $@ $<
	@python3 tools/chkbr.py $*.lst || (rm -f $@; false)

version.h: FORCE
	@echo '#define VERSION_STR "VERSION $(VERSION) ($(BUILD_DATE))"' > version.h.tmp
	@cmp -s version.h.tmp version.h && rm version.h.tmp || mv version.h.tmp version.h
FORCE:

opcodes_v2.h: tools/gen_opcodes.py
	python3 tools/gen_opcodes.py /home/prcoder/scummvm/engines/scumm/script_v2.cpp > $@
script.o script_v2.o: opcodes_v2.h

font6.s: tools/mkfont.py
	python3 tools/mkfont.py /home/prcoder/scummvm/engines/scumm/charset-fontdata.cpp $@

# дальние вызовы: far.def по объектникам (что вызывается из чужого банка),
# по нему — заглушки (в REPL) и таблицы банков
far.def: $(A_OBJ) $(B_OBJ) $(C_OBJ) $(filter-out main.o,$(KERNEL_OBJ)) main.o tools/far_auto.py
	python3 tools/far_auto.py $@ --bank A $(PG_A) $(A_OBJ) --bank B $(PG_B) $(B_OBJ) --bank C $(PG_C) $(C_OBJ) --kernel $(KERNEL_OBJ)
far_stubs.s tab_A.s tab_B.s tab_C.s far_A.strip far_B.strip far_C.strip: far.def tools/gen_far.py
	python3 tools/gen_far.py far.def .

# общая копия кода: 0160000..0176777 в каждой странице 2..15
REPL.out: $(REPL_OBJ) repl.ld sys.ld
	$(LD) -T repl.ld -Map REPL.map -o $@ $(REPL_OBJ)
	@python3 mkbin.py --check $@ 0o160000 0o177000

kernel.out: $(KERNEL_OBJ) kernel.ld sys.ld REPL.out
	$(LD) -T kernel.ld -R REPL.out -Map kernel.map -o $@ $(KERNEL_OBJ) $(LIBGCC)
	@python3 mkbin.py --check $@ 0o1000 0o23400

# банк: символы ядра и общей копии без заглушек его собственных функций
define bank
REPL_$(1).out: REPL.out far_$(1).strip
	$(OBJCOPY) $$$$(cat far_$(1).strip) REPL.out $$@
KERN_$(1).out: kernel.out far_$(1).strip
	$(OBJCOPY) $$$$(cat far_$(1).strip) kernel.out $$@
BANK$(1).out: $(2) bank.ld sys.ld KERN_$(1).out REPL_$(1).out
	$(LD) -T bank.ld -R KERN_$(1).out -R REPL_$(1).out -Map BANK$(1).map -o $$@ $(2) $(LIBGCC)
	@python3 mkbin.py --check $$@ 0o100000 0o160000
$(OUT)/BANK$(1): BANK$(1).out | $(OUT)
	python3 mkbin.py $$< $$@ 0o100000
endef
$(eval $(call bank,A,$(BANKA_OBJ)))
$(eval $(call bank,B,$(BANKB_OBJ)))
$(eval $(call bank,C,$(BANKC_OBJ)))

$(OUT):
	mkdir -p $(OUT)

$(OUT)/MANIAC.BIN: kernel.out | $(OUT)
	python3 mkbin.py $< $@

$(OUT)/REPL: REPL.out | $(OUT)
	python3 mkbin.py $< $@ 0o160000

# дискета ANDOS: ядро с автостартом, код и файлы игры как есть
GAME_FILES=$(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC
$(OUT)/$(IMG): ANDOS.IMG mkandos.py $(GAME_FILES) $(LFL) | $(OUT)
	python3 mkandos.py ANDOS.IMG $@ +$(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC $(addprefix =,$(LFL))

andos: $(OUT)/$(IMG)

# эмулятор: дискета, СМК-512; FRAMES кадров и снимок экрана в run.png
EMU=$(HOME)/claude-experiments/bk0010-emulator/build/bk0010-emulator
FRAMES=1500
run: $(OUT)/$(IMG)
	$(EMU) --smk $(OUT)/$(IMG)
shot: $(OUT)/$(IMG)
	QT_QPA_PLATFORM=offscreen $(EMU) --smk --frames $(FRAMES) --shot run.png $(KEYS) $(OUT)/$(IMG)

GMPI_API_URL=http://10.0.0.55/api
GMPI_UPLOAD_DIR=/BK_Uploads
GMPI_IMG=$(OUT)/$(IMG)
g-mpi: $(GMPI_IMG)
	curl -i -o /dev/null -X POST -H "Content-Type: multipart/form-data" -F "storeas=$(GMPI_UPLOAD_DIR)/$(IMG)" -F "size=$$(stat -c%s $(GMPI_IMG))" -F "file=@$(GMPI_IMG)" "$(GMPI_API_URL)/upload"
	curl -i -s -o /dev/null "$(GMPI_API_URL)/setdrv?drv=F0&path=$(GMPI_UPLOAD_DIR)/$(IMG)"
	curl -i -s -o /dev/null "$(GMPI_API_URL)/run?dev=smk&emu10=no"

sizes: all
	@pdp11-aout-size kernel.out REPL.out BANKA.out BANKB.out BANKC.out

asm-files:
	for f in *.c; do $(CC) $(CFLAGS) -S -fverbose-asm $$f; done

clean:
	rm -f *.o *.lst *.out *.map version.h far.def far_stubs.s tab_*.s far_*.strip $(OUT)/*

.PHONY: all clean asm-files sizes andos g-mpi run shot FORCE
.SECONDARY:
