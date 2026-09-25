; Minimal ROM used by the automated tests (assembled with RGBDS at build time).
;  - Writes $42 to cartridge RAM ($A000) so battery saves can be verified
;  - Increments $C000 once per frame
;  - Mirrors the button half of the joypad register into $C001 every frame

SECTION "Header", ROM0[$100]
    nop
    jp Start
    ds $150 - @, 0

SECTION "Main", ROM0[$150]
Start:
    di
    ld sp, $FFFE
    ld a, $0A
    ld [$0000], a       ; Enable cartridge RAM
    ld a, $42
    ld [$A000], a
    xor a
    ld [$C000], a
    ld [$C001], a
.loop
.waitVBlank
    ldh a, [$FF44]
    cp 144
    jr nz, .waitVBlank
    ld hl, $C000
    inc [hl]
    ld a, $10           ; Select the button keys
    ldh [$FF00], a
    ldh a, [$FF00]
    ldh a, [$FF00]
    ld [$C001], a
.waitLeaveVBlank
    ldh a, [$FF44]
    cp 144
    jr z, .waitLeaveVBlank
    jr .loop
