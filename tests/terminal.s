.global _start

.section .text:
_start:
	jmp _start

.section iv_table:
.word ivt_0
.word ivt_1
.word ivt_2
.word ivt_3
.word ivt_4
.word ivt_5
.word ivt_6
.word ivt_7

.equ data_in,  0xFF02
.equ data_out, 0xFF00

.section entries:
ivt_0:
	mov $_start, %pc
ivt_1:
	halt
ivt_2:
	iret
ivt_3:
	mov data_in, data_out
	mov $0, data_in
	iret
ivt_4: iret
ivt_5: iret
ivt_6: iret
ivt_7: iret

.end