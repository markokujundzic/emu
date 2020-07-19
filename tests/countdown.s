.global _start

.section .text:
print:
	push %r3
	push %r5
	
	mov %r0, %r3		
	jeq return
	mov %r3, %r5
	mod $10, %r5
	div $10, %r3	
	mov %r3, %r0
	call print
	
	add $zero, %r5
	mov %r5, data_out
	
return:
	pop %r5
	pop %r3
	ret

_start:
	jmp _start
	halt

.section iv_table:
.word ivt_0
.word ivt_1
.word ivt_2
.word ivt_3
.word ivt_4
.word ivt_5
.word ivt_6
.word ivt_7

.equ data_in,    0xFF02
.equ data_out,   0xFF00
.equ timer_cfg,  0xFF10
.equ zero,       48 # ascii value for zero character
.equ space,      32 # ascii value for space character
.equ new_line,   10 # ascii value for newline character

.section entries:
ivt_0:
	mov $1, timer_cfg
	mov $_start, %pc
ivt_1:
	halt
ivt_2:
	cmp $0, %r4
	jeq exit_1
	
	cmp $0, %r5
	jeq stop
	
	mov %r5, %r0
	call print
	mov $space, data_out
	
	sub $1, %r5
	jmp *exit_1(%pc)
stop:
	mov $0, %r4
	mov $zero, data_out
	mov $new_line, data_out
exit_1:
	iret
ivt_3:
	cmp $new_line, data_in	# check if enter key is pressed
	jeq start
	
	mul $10, %r5
	mov data_in, %r3
	sub $zero, %r3
	add %r3, %r5
	
	jmp exit_2
start:
	mov $1, %r4
exit_2:
	mov $0, data_in
	iret
ivt_4: iret
ivt_5: iret
ivt_6: iret
ivt_7: iret

.end