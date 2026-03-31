;	Altirra - Atari 800/800XL/5200 emulator
;	VGM player
;	Copyright (C) 2025 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License along
;	with this program. If not, see <http://www.gnu.org/licenses/>.

		icl		'kerneldb.inc'
		icl		'hardware.inc'

xitvbv = $e462

		org		$2400

VPD_CONTROL = $D240
VPD_STATUS = $D240
VPD_DATA = $D241

VPC_IDENTIFY = $A0
VPC_STOP = $A1
VPC_PLAY = $A2
VPC_READ_TIME = $A3
VPC_READ_DURATION = $A4

player0 = $2200

;===============================================================================
dlist:
		:3 dta	$70
		dta		$42, a(playfield)
		dta		$70
		dta		$02
		dta		$41, a(dlist)

playfield:
		;		 0123456789012345678901234567890123456789
		dta		"  VGM Player                            "*
		dta		"  Time        00:00:00.00 / 00:00:00.00 "


;===============================================================================
.proc main
		;clear player 0 memory
		lda		#0
		tax
		sta:rpl	player0,x+

		;move all P/M graphics offscreen
		ldx		#3
pmreset_loop:
		sta		hposp0,x
		sta		hposm0,x
		sta		sizep0,x
		dex
		bpl		pmreset_loop
		sta		sizem

		;wait for vbl
		lda		#123
		cmp:rcs	vcount

		;reset POKEY
		sta		wsync
		sta		wsync

		ldx		#0
		ldy		#3
		stx		skctl
		sty		skctl

		;Reset secondary POKEY, if there is one; displace it by ~half a sample
		;(40 cycles). If there is no secondary POKEY, this will just reset
		;the primary POKEY again.
		stx		skctl+$10
		jsr		delay36
		sty		skctl+$10

		;enter critical section
		sei

		;set up display
		mwa		#dlist sdlstl
		mva		#$2e sdmctl
		mva		#$00 color2
		mva		#$0a color1
		mva		#$04 pcolr0
		mva		#$20 pmbase
		mva		#$02 gractl
		mva		#$01 gprior
		
		;position and write player 0 to highlight fields
		mva		#$34 hposp0
		mva		#$03 sizep0
		lda		#%11111100
		sta		player0+24
		sta		player0+25
		sta		player0+26
		sta		player0+27

		;write duration to playfield -- do this before VBI deferred goes live
		mva		#VPC_READ_DURATION VPD_CONTROL
		ldy		#68
		jsr		PutTimestamp

		;set up VBI deferred handler
		mwa		#VbiHandler vvblkd

		;exit critical section
		cli

		;wait for display to go live
		mva		#$ff ptrig0
		cmp:req	ptrig0

loop:
		jmp		*
		
delay36:
		jsr		delay12
delay24:
		jsr		delay12
delay12:
		rts
.endp

;===============================================================================
.proc VbiHandler
		;restart playback if stopped
		bit		VPD_STATUS
		bmi		is_playing

		mva		#VPC_PLAY VPD_CONTROL

is_playing:

		;read timestamp
		mva		#VPC_READ_TIME VPD_CONTROL

		ldy		#54
		jsr		PutTimestamp

		jmp		xitvbv

.endp

;===============================================================================
.proc PutTimestamp
		ldx		#4
time_loop:
		lda		VPD_DATA
		jsr		PutBCD
		iny
		dex
		bne		time_loop
		rts
.endp

;===============================================================================
.proc PutBCD
		pha
		lsr
		lsr
		lsr
		lsr
		ora		#$10
		sta		playfield,y+
		pla
		and		#$0f
		ora		#$10
		sta		playfield,y+
		rts
.endp

;===============================================================================
		run		main
