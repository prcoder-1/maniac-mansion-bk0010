# Хостовые тесты: те же модули движка, собранные gcc для ПК (-DHOST)
ROOT=..
CFLAGS=-std=gnu2x -O1 -g -Wall -Wextra -Wno-unused-parameter -DHOST -I$(ROOT) -I$(ROOT)/test/host
CC=gcc
OUT=out
ENGINE=$(ROOT)/resource.c $(ROOT)/state.c $(ROOT)/gfx_v2.c
VM=$(ROOT)/script.c $(ROOT)/script_v2.c $(ROOT)/object.c $(ROOT)/room.c $(ROOT)/actor.c $(ROOT)/costume.c $(ROOT)/costume_r.c \
	$(ROOT)/camera.c $(ROOT)/string.c $(ROOT)/verbs.c $(ROOT)/scumm.c $(ROOT)/gfx.c $(ROOT)/font6.c host/host_stubs.c
HOST=host/host.c host/png.c host/host_gfx.c

all: res gfx strips

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

# M2: фон и маска всех комнат совпадают с эталоном tools/v2gfx.py
gfx: $(OUT)/gfxtest
	$(OUT)/gfxtest $(ROOT) > $(OUT)/gfx.txt
	python3 $(ROOT)/tools/v2gfx.py $(ROOT) all > $(OUT)/gfx_ref.txt
	diff $(OUT)/gfx_ref.txt $(OUT)/gfx.txt && echo "gfx: OK"

$(OUT)/vmtest: host/vmtest.c $(ENGINE) $(VM) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/vmtest.c $(ENGINE) $(VM) $(HOST)

$(OUT)/striptest: host/striptest.c $(ENGINE) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/striptest.c $(ENGINE) $(HOST)

# StripTable: перерисовка полос совпадает с полным декодированием
strips: $(OUT)/striptest
	$(OUT)/striptest $(ROOT)

.PHONY: all res gfx strips
