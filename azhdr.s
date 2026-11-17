/ Модуль AZБК (azmod.ld): таблица входов с 040000 — порядок az.h (AZ_*), вызов — azc
/ (farcall.s: jmp @040000(r0) с r0 = номер * 2)
	.text
	.word	_az_init, _az_flip, _az_view, _az_strips, _az_char, _az_clear
	.word	_az_recolor, _az_cursor, _az_clearall, _az_label, _az_lights, _az_actors
	.word	_az_actreset
