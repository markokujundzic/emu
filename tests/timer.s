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

.equ data_in,    0xFF02
.equ data_out,   0xFF00
.equ timer_cfg,  0xFF10
.equ new_line,   10 # ascii value for newline character

.section entries:
ivt_0:
	mov $_start, %pc
ivt_1:
	halt
ivt_2: 			
	cmp $0, %r4
	jeq exit_1
	
	cmp $0, %r0
	jeq timer
	
	cmp $0, %r0
	jne inc
	
timer:
	mov %r3, timer_cfg
	mov $1, %r0
	jmp label_t
	
inc: 
	add $1, timer_cfg	
label_t:		
	mov $116, data_out
	mov $105, data_out
	mov $109, data_out
	mov $101, data_out
	mov $114, data_out
	mov $32,  data_out
	iret
ivt_3: 		    
	cmp $new_line, data_in	# check if enter key is pressed
	jeq start
	
	jmp exit_2
start:
	mov $1, %r4	
exit_1:
	iret
exit_2:
	mov $0, data_in
	iret
ivt_4: iret
ivt_5: iret
ivt_6: iret
ivt_7: iret

.end