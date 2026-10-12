# Хостовые тесты: те же модули движка, собранные gcc для ПК (-DHOST)
ROOT=..
CFLAGS=-std=gnu2x -O1 -g -Wall -Wextra -Wno-unused-parameter -DHOST -I$(ROOT) -I$(ROOT)/test -I$(ROOT)/test/host
CC=gcc
OUT=out
ENGINE=$(ROOT)/resource.c $(ROOT)/state.c $(ROOT)/gfx_v1.c
VM=$(ROOT)/script.c $(ROOT)/script_v2.c $(ROOT)/object.c $(ROOT)/room.c $(ROOT)/actor.c $(ROOT)/costume.c $(ROOT)/costume_r.c $(ROOT)/limb.c \
	$(ROOT)/camera.c $(ROOT)/text.c $(ROOT)/string.c $(ROOT)/verbs.c $(ROOT)/scumm.c $(ROOT)/gfx.c $(ROOT)/font6.c $(ROOT)/sound.c host/host_stubs.c
HOST=host/host.c host/png.c host/host_gfx.c

all: lfl res gfx strips vm

$(OUT):
	mkdir -p $(OUT)

$(OUT)/restest: host/restest.c $(ENGINE) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/restest.c $(ENGINE) $(HOST)

# M1: ресурсы через кэш страниц совпадают со справочником по LFL
res: $(OUT)/restest
	$(OUT)/restest $(ROOT) > $(OUT)/res.txt
	python3 $(ROOT)/tools/lfl_stats.py $(ROOT) > $(OUT)/res_ref.txt
	diff $(OUT)/res_ref.txt $(OUT)/res.txt && echo "res: OK"

$(OUT)/gfxtest: host/gfxtest.c $(ENGINE) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/gfxtest.c $(ENGINE) $(HOST)

# M2: фон и маска всех комнат совпадают с независимым эталоном tools/v1gfx.py
gfx: $(OUT)/gfxtest
	$(OUT)/gfxtest $(ROOT) > $(OUT)/gfx.txt
	python3 $(ROOT)/tools/v1gfx.py $(ROOT) all > $(OUT)/gfx_ref.txt
	diff $(OUT)/gfx_ref.txt $(OUT)/gfx.txt && echo "gfx: OK"

$(OUT)/lfltest: host/lfltest.c m0sum.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/lfltest.c

# M0: файлы игры — MM v1 DOS с правками порта (test/m0sum.h, tools/lfl_sums.py)
lfl: $(OUT)/lfltest
	$(OUT)/lfltest $(ROOT)

$(OUT)/vmtest: host/vmtest.c $(ENGINE) $(VM) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/vmtest.c $(ENGINE) $(VM) $(HOST)

# M3: движок на ПК — 300 кадров заставки без падения (трасса опкодов — out/vm.txt)
vm: $(OUT)/vmtest
	$(OUT)/vmtest $(ROOT) 300 $(OUT)/vm.txt
	grep -q "^--- frame 299 " $(OUT)/vm.txt && echo "vm: OK"

$(OUT)/striptest: host/striptest.c $(ENGINE) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/striptest.c $(ENGINE) $(HOST)

# StripTable: перерисовка полос совпадает с полным декодированием
strips: $(OUT)/striptest
	$(OUT)/striptest $(ROOT)

.PHONY: all lfl res gfx strips vm snd

$(OUT)/sndtest: host/sndtest.c $(ENGINE) $(VM) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/sndtest.c $(ENGINE) $(VM) $(HOST)

# M6: секвенсор звука по всем звукам
snd: $(OUT)/sndtest
	$(OUT)/sndtest $(ROOT)
