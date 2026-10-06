# Хостовые тесты: те же модули движка, собранные gcc для ПК (-DHOST)
ROOT=..
CFLAGS=-std=gnu2x -O1 -g -Wall -Wextra -Wno-unused-parameter -DHOST -I$(ROOT) -I$(ROOT)/test/host
CC=gcc
OUT=out
ENGINE=$(ROOT)/resource.c $(ROOT)/state.c
HOST=host/host.c

all: res

$(OUT):
	mkdir -p $(OUT)

$(OUT)/restest: host/restest.c $(ENGINE) $(HOST) $(ROOT)/*.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ host/restest.c $(ENGINE) $(HOST)

# M1: ресурсы через кэш страниц совпадают со справочником по LFL
res: $(OUT)/restest
	$(OUT)/restest $(ROOT) > $(OUT)/res.txt
	python3 $(ROOT)/tools/lfl_stats.py $(ROOT) > $(OUT)/res_ref.txt
	diff $(OUT)/res_ref.txt $(OUT)/res.txt && echo "res: OK"

.PHONY: all res
