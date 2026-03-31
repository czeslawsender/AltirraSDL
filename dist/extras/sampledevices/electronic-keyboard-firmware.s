; Electronic keyboard firmware
hposm0	equ		$d004
sizem	equ		$d00c
grafp0	equ		$d00d
colpm0	equ		$d012
colpf1	equ		$d017
colpf2	equ		$d018
colbk	equ		$d01a
prior	equ		$d01b
vdelay	equ		$d01c
gractl	equ		$d01d
kbcode	equ		$d209
irqen	equ		$d20e
irqst	equ		$d20e
skctl	equ		$d20f
skstat	equ		$d20f

dmactl	equ		$d400
chactl	equ		$d401
dlistl	equ		$d402
pmbase	equ		$d407
chbase	equ		$d409
vcount	equ		$d40b
nmien	equ		$d40e
nmires	equ		$d40f
nmist	equ		$d40f

missiles = $0580

panpos = $80

		org		$0700
		opt		h-f+
		
;disk boot header
disk_start:
		dta		$00
		dta		[disk_end-disk_start]/$80
		dta		a(disk_start)
		dta		a(main)
		rts

dlist:
		dta		$70,$70,$70
		dta		$42,a(pf)
		dta		$70
		dta		$02
		dta		$42,a(pf_keyboard1)
		dta		$02
		dta		$02
		dta		$42,a(pf_keyboard2)
		dta		$02
		dta		$41,a(dlist)
		
pf:
		;		 0123456789012345678901234567890123456789
		dta		"   Elec Keyboard     Pan (,.) "*
		dta		                              "        "
		dta		                                      "  "*
pf_keyboard1:
		dta		"  ",$D9,$80,$00,$80,$00,$80,$FC,$80,$00,$80,$00,$80,$00,$80,$FC,$80,$59,"                     "
		dta		"  ",$D9,$80,"W",$80,"E",$80,$FC,$80,"T",$80,"Y",$80,"U",$80,$FC,$80,$59,"                     "
pf_keyboard2:
		dta		"  ",$D9,$80,$FC,$80,$FC,$80,$FC,$80,$FC,$80,$FC,$80,$FC,$80,$FC,$80,$59,"                     "
		dta		"  ",$D9,$A1,$FC,$B3,$FC,$A4,$FC,$A6,$FC,$A7,$FC,$A8,$FC,$AA,$FC,$AB,$59,"                     "
main:
		sei
		mva		#0 nmien
		mva		#0 dmactl

		;clear zp
		ldx		#$80
		sta:rmi	0,x+

		;reset P/M graphics buffer
		ldx		#0
		txa
		sta:rpl	missiles,x+
		
		lda		#2
		sta		missiles+$10
		sta		missiles+$11
		sta		missiles+$12
		sta		missiles+$13
					
		lda		#124
		cmp:rne	vcount
		
		mwa		#dlist dlistl
		mva		#$CA colpf1
		mva		#$94 colpf2
		mva		#$0E colpm0
		mva		#$00 colbk
		mva		#$E0 chbase
		mva		#$04 pmbase
		mva		#$02 chactl
		mva		#$26 dmactl
		mva		#$01 prior
		mva		#$01 gractl
		
		ldx		#3
		lda		#0
		sta:rpl	grafp0,x-
		
		sta		sizem
		sta		vdelay

		ldx		#$80
		jsr		SetPan

		;reset POKEY
		mva		#$00 skctl
		sta		irqen
		mva		#$03 skctl
		mva		#$40 irqen
		
		sta		nmires
		
key_loop:
		bit		nmist
		svc:jsr	VbiHandler
		bit		irqen
		bvs		key_loop
		
		mva		#0 irqen
		mva		#$40 irqen
		
		lda		kbcode
		ldx		#12
key_lookup:
		cmp		key_table,x
		sne:stx	$d100
		dex
		bpl		key_lookup
		
		jmp		key_loop	
		
key_table:
		dta		$3F	;A
		dta		$2E	; W
		dta		$3E	;S
		dta		$2A	; E
		dta		$3A	;D
		dta		$38	;F
		dta		$2D	; T
		dta		$3D	;G
		dta		$2B	; Y
		dta		$39	;H
		dta		$0B	; U
		dta		$01	;J
		dta		$05	;K
		
SetPan:
		stx		panpos
		stx		$d101
SetPanMarker:
		lda		panpos
		lsr
		lsr
		lsr
		clc
		adc		#$a8
		sta		hposm0
		rts

.proc VbiHandler
		sta		nmires

		lda		#$04
		bit		skstat
		sne:jsr	handle_key_down
		rts
		
handle_key_down:
		lda		kbcode
		cmp		#$20
		bne		not_pan_left
		
		ldx		panpos
		cpx		#2
		bcc		not_pan_left
		dex
		jmp		SetPan
		
not_pan_left:
		cmp		#$22
		bne		not_pan_right
		
		ldx		panpos
		inx
		beq		not_pan_right
		jmp		SetPan

not_pan_right:
		rts
.endp


		.if *&$7F!=$7F
		org		*|$7F
		dta		0
		.endif
disk_end:
