# Maniac Mansion (SCUMM v2) для БК-0011М + СМК-512 + AY: ядро в базовом ОЗУ, общая
# копия кода (REPL) во всех страницах ДОЗУ, банки кода A/B/C в своих страницах
VERSION=0.4
IMG=MANIAC_$(VERSION).IMG
BUILD_DATE=$(shell date +%d.%m.%Y)
XGCC=/home/prcoder/xgcc
CFLAGS=-std=gnu23 -fomit-frame-pointer -msoft-float -nostartfiles -nodefaultlibs -nostdlib -m10 -m1801vm1 -Os \
	-fno-reorder-blocks -fno-caller-saves -I. -I$(XGCC)/include
ASFLAGS=-mno-fpu -mlimited-eis
# make clean; make TIMING=1: замеры частей шага (scumm.c TMARK, dbg_acc)
ifdef TIMING
CFLAGS+=-DTIMING
endif
CC=pdp11-aout-gcc
AS=pdp11-aout-as
LD=pdp11-aout-ld
OBJCOPY=pdp11-aout-objcopy
OUT=bin
PG_A=2
PG_B=3
PG_C=7
PG_D=15

LIBGCC=$(shell $(CC) -m10 -msoft-float -print-libgcc-file-name)

KERNEL_OBJ=crt0.o loader.o helpers.o divmulmod.o memory.o font_pc.o con.o state.o platform.o irq.o main.o
REPL_OBJ=farcall.o smk.o blit.o gfxv2.o div5.o proc3.o far_stubs.o
# банк A: интерпретатор, цикл; B: актёры; C: ресурсы, экран, текст, звук; D: комната и боксы
A_OBJ=script.o script_v2.o object.o string.o verbs.o scumm.o
B_OBJ=actor.o costume.o costume_r.o camera.o pal11a.o
D_OBJ=room.o
C_OBJ=resource.o gfx_v2.o gfx.o textb.o font6.o sound.o saveload.o romio.o pal11.o
BANKA_OBJ=tab_A.o $(A_OBJ)
BANKB_OBJ=tab_B.o $(B_OBJ)
BANKC_OBJ=tab_C.o $(C_OBJ)
BANKD_OBJ=tab_D.o $(D_OBJ)
# звуковой драйвер AY: страница 1 ОЗУ 11М с 040000 (snddrv.h)
DRV_OBJ=drvhdr.o snddrv.o

LFL=$(sort $(wildcard [0-5][0-9].LFL))

all: $(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC $(OUT)/BANKD $(OUT)/SNDDRV $(OUT)/$(IMG)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# зависимости от заголовков (.d пишет gcc при сборке .o)
-include $(wildcard *.d)

%.o: %.s
	$(AS) $(ASFLAGS) -a=$*.lst -o $@ $<

version.h: FORCE
	@echo '#define VERSION_STR "VERSION $(VERSION) ($(BUILD_DATE))"' > version.h.tmp
	@echo '#define VERSION_NUM "$(VERSION)"' >> version.h.tmp
	@cmp -s version.h.tmp version.h && rm version.h.tmp || mv version.h.tmp version.h
FORCE:

opcodes_v2.h: tools/gen_opcodes.py
	python3 tools/gen_opcodes.py /home/prcoder/scummvm/engines/scumm/script_v2.cpp > $@
script.o script_v2.o: opcodes_v2.h

# палитры и узоры БК-0011М по комнатам (tools/pal11.py): фон — pal11.c (банк C),
# актёры — pal11a.s (банк B)
pal11.c pal11a.s &: tools/pal11.py tools/v2gfx.py
	python3 tools/pal11.py . pal11.c pal11a.s

font6.s: tools/mkfont.py
	python3 tools/mkfont.py /home/prcoder/scummvm/engines/scumm/charset-fontdata.cpp $@

# дальние вызовы: far.def по объектникам (что вызывается из чужого банка),
# по нему — заглушки (в REPL) и таблицы банков (&: — один запуск на все, иначе при -j их
# пишут несколько gen_far.py сразу и ассемблер читает недописанный файл)
far.def: $(A_OBJ) $(B_OBJ) $(C_OBJ) $(D_OBJ) $(filter-out main.o,$(KERNEL_OBJ)) main.o tools/far_auto.py
	python3 tools/far_auto.py $@ --bank A $(PG_A) $(A_OBJ) --bank B $(PG_B) $(B_OBJ) --bank C $(PG_C) $(C_OBJ) --bank D $(PG_D) $(D_OBJ) --kernel $(KERNEL_OBJ)
far_stubs.s tab_A.s tab_B.s tab_C.s tab_D.s far_A.strip far_B.strip far_C.strip far_D.strip &: far.def tools/gen_far.py
	python3 tools/gen_far.py far.def .

# общая копия кода: 0160000..0176777 в каждой странице 2..15
# 32-битное деление и умножение libgcc — в общую копию: одно на ядро и банки
REPL_LIBGCC=-u ___divsi3 -u ___modsi3 -u ___udivsi3 -u ___umodsi3 -u ___mulsi3
REPL.out: $(REPL_OBJ) repl.ld sys.ld
	$(LD) -T repl.ld -Map REPL.map -o $@ $(REPL_OBJ) $(REPL_LIBGCC) $(LIBGCC)
	@python3 mkbin.py --check $@ 0o160000 0o177000

kernel.out: $(KERNEL_OBJ) kernel.ld sys.ld REPL.out
	$(LD) -T kernel.ld -R REPL.out -Map kernel.map -o $@ $(KERNEL_OBJ) $(LIBGCC)
	@python3 mkbin.py --check $@ 0o1000 0o25000

# банк: символы ядра и общей копии без заглушек его собственных функций
define bank
REPL_$(1).out: REPL.out far_$(1).strip
	$(OBJCOPY) $$$$(cat far_$(1).strip) REPL.out $$@
KERN_$(1).out: kernel.out far_$(1).strip
	$(OBJCOPY) $$$$(cat far_$(1).strip) kernel.out $$@
BANK$(1).out: $(2) bank.ld sys.ld KERN_$(1).out REPL_$(1).out tools/far_ptr.py
	$(LD) -T bank.ld -R KERN_$(1).out -R REPL_$(1).out -Map BANK$(1).map -o $$@ $(2) $(LIBGCC)
	@python3 mkbin.py --check $$@ 0o100000 0o160000
	@python3 tools/far_ptr.py $$@ far.def $(1) REPL.out || (rm -f $$@; false)
$(OUT)/BANK$(1): BANK$(1).out | $(OUT)
	python3 mkbin.py --trim $$< $$@ 0o100000
endef
$(eval $(call bank,A,$(BANKA_OBJ)))
$(eval $(call bank,B,$(BANKB_OBJ)))
$(eval $(call bank,C,$(BANKC_OBJ)))
$(eval $(call bank,D,$(BANKD_OBJ)))

# драйвер: символы ядра (почтовый ящик, деление, _page_code) — по адресам ядра
SNDDRV.out: $(DRV_OBJ) drv.ld sys.ld kernel.out
	$(LD) -T drv.ld -R kernel.out -Map SNDDRV.map -o $@ $(DRV_OBJ) $(LIBGCC)
	@python3 mkbin.py --check $@ 0o40000 0o57400
$(OUT)/SNDDRV: SNDDRV.out | $(OUT)
	python3 mkbin.py --trim $< $@ 0o40000

$(OUT):
	mkdir -p $(OUT)

$(OUT)/MANIAC.BIN: kernel.out | $(OUT)
	python3 mkbin.py $< $@

$(OUT)/REPL: REPL.out | $(OUT)
	python3 mkbin.py $< $@ 0o160000

# дискета ANDOS: ядро с автостартом, код и файлы игры (титры — tools/lfl_patch.py); ftab.py — где они
# лежат (быстрое чтение драйвером ПЗУ, loader.s) и метка сборки сохранений
GAME_FILES=$(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC $(OUT)/BANKD $(OUT)/SNDDRV
# сохранения: заранее по 20 Кбайт нулей — игра пишет их по блокам, минуя ANDOS (saveload.c);
# на дискете места — на два
SAVES=$(OUT)/MMSAVE1 $(OUT)/MMSAVE2
$(SAVES): | $(OUT)
	head -c 20480 /dev/zero > $@
$(OUT)/$(IMG): ANDOS.IMG mkandos.py tools/ftab.py tools/lfl_patch.py $(GAME_FILES) $(SAVES) $(LFL) | $(OUT)
	@test $$(printf %s '$(IMG)' | wc -c) -le 14 || { echo 'имя образа $(IMG) длиннее 14 символов'; false; }
	python3 tools/lfl_patch.py .
	python3 mkandos.py ANDOS.IMG $@ +$(OUT)/MANIAC.BIN $(OUT)/REPL $(OUT)/BANKA $(OUT)/BANKB $(OUT)/BANKC $(OUT)/BANKD $(OUT)/SNDDRV $(addprefix =,$(LFL)) $(addprefix =,$(SAVES))
	python3 tools/ftab.py $@ kernel.map BANKC.map

andos: $(OUT)/$(IMG)

# эмулятор: БК-0011М, СМК-512, AY, дискета с записью (сохранения игры); FRAMES кадров
# и снимок экрана в run.png
EMU=$(HOME)/claude-experiments/bk0010-emulator/build/bk0010-emulator
EMUFLAGS=--machine bk11m --smk --ay --no-2ay
FRAMES=1500
run: $(OUT)/$(IMG)
	$(EMU) $(EMUFLAGS) --disk-rw $(OUT)/$(IMG)
shot: $(OUT)/$(IMG)
	QT_QPA_PLATFORM=offscreen $(EMU) $(EMUFLAGS) --frames $(FRAMES) --shot run.png $(KEYS) $(OUT)/$(IMG)

GMPI_API_URL=http://10.0.0.55/api
GMPI_UPLOAD_DIR=/BK_Uploads
GMPI_IMG=$(OUT)/$(IMG)
g-mpi: $(GMPI_IMG)
	curl -i -o /dev/null -X POST -H "Content-Type: multipart/form-data" -F "storeas=$(GMPI_UPLOAD_DIR)/$(IMG)" -F "size=$$(stat -c%s $(GMPI_IMG))" -F "file=@$(GMPI_IMG)" "$(GMPI_API_URL)/upload"
	curl -i -s -o /dev/null "$(GMPI_API_URL)/setdrv?drv=F0&path=$(GMPI_UPLOAD_DIR)/$(IMG)"
	curl -i -s -o /dev/null "$(GMPI_API_URL)/run?dev=smk&emu10=no"

sizes: all
	@pdp11-aout-size kernel.out REPL.out BANKA.out BANKB.out BANKC.out BANKD.out SNDDRV.out

asm-files:
	for f in *.c; do $(CC) $(CFLAGS) -S -fverbose-asm $$f; done

clean:
	rm -f *.o *.d *.lst *.out *.map version.h far.def far_stubs.s tab_*.s far_*.strip $(OUT)/*

.PHONY: all clean asm-files sizes andos g-mpi run shot FORCE
.SECONDARY:
.DELETE_ON_ERROR:
