/ Модуль AZБК: страница СМК без смены _cur_page (общая копия кода: mapr0, back)
	.globl	_az_smk, _az_back, mapr0, back
	.text

/ void az_smk(uint16_t page): страница page (0..15) в окнах 0100000..; _cur_page — прежний
_az_smk:
	mov	2(sp), r0
	jmp	mapr0

/ void az_back(void): снова страница вызывающего (_cur_page)
_az_back:
	jmp	back
