#include "cpu.h"
#include "mmu.h"
#include "gb.h"

/* Shorthand macros */
#define A gb->cpu.a
#define F gb->cpu.f
#define B gb->cpu.b
#define C gb->cpu.c
#define D gb->cpu.d
#define E gb->cpu.e
#define H gb->cpu.h
#define L gb->cpu.l
#define AF gb->cpu.af
#define BC gb->cpu.bc
#define DE gb->cpu.de
#define HL gb->cpu.hl
#define SP gb->cpu.sp
#define PC gb->cpu.pc
#define IME gb->cpu.ime

#define FLAG_SET(f)   (F |= (f))
#define FLAG_CLEAR(f) (F &= ~(f))
#define FLAG_GET(f)   (F & (f))
#define FLAG_COND(cond, f) do { if(cond) FLAG_SET(f); else FLAG_CLEAR(f); } while(0)

#define READ8(a)      mmu_read(gb, a)
#define WRITE8(a, v)  mmu_write(gb, a, v)
#define READ16(a)     mmu_read16(gb, a)
#define WRITE16(a, v) mmu_write16(gb, a, v)

#define FETCH8()  (READ8(PC++))
#define FETCH16() (PC += 2, READ16(PC - 2))

/* Stack operations */
#define PUSH(v) do { SP -= 2; WRITE16(SP, v); } while(0)
#define POP(v)  do { v = READ16(SP); SP += 2; } while(0)

/* ADD A, r */
static inline void add_a(GB_State *gb, u8 val) {
    u16 res = (u16)A + val;
    FLAG_CLEAR(FLAG_N);
    FLAG_COND((A & 0xF) + (val & 0xF) > 0xF, FLAG_H);
    FLAG_COND(res > 0xFF, FLAG_C);
    A = (u8)res;
    FLAG_COND(A == 0, FLAG_Z);
}

/* ADC A, r */
static inline void adc_a(GB_State *gb, u8 val) {
    u8 c = FLAG_GET(FLAG_C) ? 1 : 0;
    u16 res = (u16)A + val + c;
    FLAG_CLEAR(FLAG_N);
    FLAG_COND((A & 0xF) + (val & 0xF) + c > 0xF, FLAG_H);
    FLAG_COND(res > 0xFF, FLAG_C);
    A = (u8)res;
    FLAG_COND(A == 0, FLAG_Z);
}

/* SUB A, r */
static inline void sub_a(GB_State *gb, u8 val) {
    FLAG_SET(FLAG_N);
    FLAG_COND((A & 0xF) < (val & 0xF), FLAG_H);
    FLAG_COND(A < val, FLAG_C);
    A -= val;
    FLAG_COND(A == 0, FLAG_Z);
}

/* SBC A, r */
static inline void sbc_a(GB_State *gb, u8 val) {
    u8 c = FLAG_GET(FLAG_C) ? 1 : 0;
    int res = (int)A - val - c;
    FLAG_SET(FLAG_N);
    FLAG_COND((A & 0xF) - (val & 0xF) - c < 0, FLAG_H);
    FLAG_COND(res < 0, FLAG_C);
    A = (u8)res;
    FLAG_COND(A == 0, FLAG_Z);
}

/* AND A, r */
static inline void and_a(GB_State *gb, u8 val) {
    A &= val;
    F = FLAG_H | (A == 0 ? FLAG_Z : 0);
}

/* XOR A, r */
static inline void xor_a(GB_State *gb, u8 val) {
    A ^= val;
    F = (A == 0 ? FLAG_Z : 0);
}

/* OR A, r */
static inline void or_a(GB_State *gb, u8 val) {
    A |= val;
    F = (A == 0 ? FLAG_Z : 0);
}

/* CP A, r */
static inline void cp_a(GB_State *gb, u8 val) {
    FLAG_SET(FLAG_N);
    FLAG_COND((A & 0xF) < (val & 0xF), FLAG_H);
    FLAG_COND(A < val, FLAG_C);
    FLAG_COND(A == val, FLAG_Z);
}

/* INC r */
static inline u8 inc_r(GB_State *gb, u8 val) {
    FLAG_CLEAR(FLAG_N);
    FLAG_COND((val & 0xF) == 0xF, FLAG_H);
    val++;
    FLAG_COND(val == 0, FLAG_Z);
    return val;
}

/* DEC r */
static inline u8 dec_r(GB_State *gb, u8 val) {
    FLAG_SET(FLAG_N);
    FLAG_COND((val & 0xF) == 0, FLAG_H);
    val--;
    FLAG_COND(val == 0, FLAG_Z);
    return val;
}

/* ADD HL, r16 */
static inline void add_hl(GB_State *gb, u16 val) {
    u32 res = (u32)HL + val;
    FLAG_CLEAR(FLAG_N);
    FLAG_COND((HL & 0xFFF) + (val & 0xFFF) > 0xFFF, FLAG_H);
    FLAG_COND(res > 0xFFFF, FLAG_C);
    HL = (u16)res;
}

/* RL / RLC / RR / RRC / SLA / SRA / SRL / SWAP for CB prefix */
static inline u8 rlc(GB_State *gb, u8 val) {
    u8 c = val >> 7;
    val = (val << 1) | c;
    F = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    return val;
}
static inline u8 rrc(GB_State *gb, u8 val) {
    u8 c = val & 1;
    val = (val >> 1) | (c << 7);
    F = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    return val;
}
static inline u8 rl(GB_State *gb, u8 val) {
    u8 old_c = FLAG_GET(FLAG_C) ? 1 : 0;
    u8 new_c = val >> 7;
    val = (val << 1) | old_c;
    F = (val == 0 ? FLAG_Z : 0) | (new_c ? FLAG_C : 0);
    return val;
}
static inline u8 rr(GB_State *gb, u8 val) {
    u8 old_c = FLAG_GET(FLAG_C) ? 1 : 0;
    u8 new_c = val & 1;
    val = (val >> 1) | (old_c << 7);
    F = (val == 0 ? FLAG_Z : 0) | (new_c ? FLAG_C : 0);
    return val;
}
static inline u8 sla(GB_State *gb, u8 val) {
    u8 c = val >> 7;
    val <<= 1;
    F = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    return val;
}
static inline u8 sra(GB_State *gb, u8 val) {
    u8 c = val & 1;
    val = (val >> 1) | (val & 0x80);
    F = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    return val;
}
static inline u8 srl(GB_State *gb, u8 val) {
    u8 c = val & 1;
    val >>= 1;
    F = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    return val;
}
static inline u8 swap(GB_State *gb, u8 val) {
    val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
    F = (val == 0 ? FLAG_Z : 0);
    return val;
}
static inline void bit_n(GB_State *gb, u8 val, u8 n) {
    FLAG_COND(!(val & (1 << n)), FLAG_Z);
    FLAG_CLEAR(FLAG_N);
    FLAG_SET(FLAG_H);
}

/* Handle interrupts */
static int handle_interrupts(GB_State *gb) {
    if (!IME && !gb->cpu.halted) return 0;
    
    u8 pending = gb->mmu.ie & gb->mmu.if_ & 0x1F;
    if (!pending) return 0;
    
    gb->cpu.halted = 0;
    if (!IME) return 0;
    
    IME = 0;
    PUSH(PC);
    
    /* VBlank */
    if (pending & 0x01) { gb->mmu.if_ &= ~0x01; PC = 0x0040; }
    /* LCD STAT */
    else if (pending & 0x02) { gb->mmu.if_ &= ~0x02; PC = 0x0048; }
    /* Timer */
    else if (pending & 0x04) { gb->mmu.if_ &= ~0x04; PC = 0x0050; }
    /* Serial */
    else if (pending & 0x08) { gb->mmu.if_ &= ~0x08; PC = 0x0058; }
    /* Joypad */
    else if (pending & 0x10) { gb->mmu.if_ &= ~0x10; PC = 0x0060; }
    
    return 20;
}

/* CB prefix opcodes */
static int cpu_step_cb(GB_State *gb) {
    u8 op = FETCH8();
    u8 reg_idx = op & 0x07;
    u8 bit_n_val = (op >> 3) & 0x07;
    
    /* Get register value */
    u8 val;
    switch (reg_idx) {
        case 0: val = B; break;
        case 1: val = C; break;
        case 2: val = D; break;
        case 3: val = E; break;
        case 4: val = H; break;
        case 5: val = L; break;
        case 6: val = READ8(HL); break;
        case 7: val = A; break;
        default: val = 0;
    }
    
    int cycles = (reg_idx == 6) ? 16 : 8;
    
    if (op < 0x40) {
        /* Rotates/shifts/swap */
        switch (op >> 3) {
            case 0: val = rlc(gb, val); break;
            case 1: val = rrc(gb, val); break;
            case 2: val = rl(gb, val);  break;
            case 3: val = rr(gb, val);  break;
            case 4: val = sla(gb, val); break;
            case 5: val = sra(gb, val); break;
            case 6: val = swap(gb, val); break;
            case 7: val = srl(gb, val); break;
        }
        /* Write back */
        switch (reg_idx) {
            case 0: B = val; break; case 1: C = val; break;
            case 2: D = val; break; case 3: E = val; break;
            case 4: H = val; break; case 5: L = val; break;
            case 6: WRITE8(HL, val); break;
            case 7: A = val; break;
        }
    } else if (op < 0x80) {
        /* BIT */
        bit_n(gb, val, bit_n_val);
        if (reg_idx == 6) cycles = 12;
    } else if (op < 0xC0) {
        /* RES */
        val &= ~(1 << bit_n_val);
        switch (reg_idx) {
            case 0: B = val; break; case 1: C = val; break;
            case 2: D = val; break; case 3: E = val; break;
            case 4: H = val; break; case 5: L = val; break;
            case 6: WRITE8(HL, val); break;
            case 7: A = val; break;
        }
    } else {
        /* SET */
        val |= (1 << bit_n_val);
        switch (reg_idx) {
            case 0: B = val; break; case 1: C = val; break;
            case 2: D = val; break; case 3: E = val; break;
            case 4: H = val; break; case 5: L = val; break;
            case 6: WRITE8(HL, val); break;
            case 7: A = val; break;
        }
    }
    
    return cycles;
}

/* Main CPU step - returns cycles elapsed */
int cpu_step(GB_State *gb) {
    int int_cycles = handle_interrupts(gb);
    if (int_cycles) return int_cycles;
    
    if (gb->cpu.halted) return 4;
    
    u8 op = FETCH8();
    int cycles = 4;
    u8 tmp8; u16 tmp16; s8 imm8;
    
    switch (op) {
        /* NOP */
        case 0x00: cycles = 4; break;
        
        /* LD r16, d16 */
        case 0x01: BC = FETCH16(); cycles = 12; break;
        case 0x11: DE = FETCH16(); cycles = 12; break;
        case 0x21: HL = FETCH16(); cycles = 12; break;
        case 0x31: SP = FETCH16(); cycles = 12; break;
        
        /* LD (r16), A */
        case 0x02: WRITE8(BC, A); cycles = 8; break;
        case 0x12: WRITE8(DE, A); cycles = 8; break;
        case 0x22: WRITE8(HL, A); HL++; cycles = 8; break;
        case 0x32: WRITE8(HL, A); HL--; cycles = 8; break;
        
        /* INC r16 */
        case 0x03: BC++; cycles = 8; break;
        case 0x13: DE++; cycles = 8; break;
        case 0x23: HL++; cycles = 8; break;
        case 0x33: SP++; cycles = 8; break;
        
        /* INC r8 */
        case 0x04: B = inc_r(gb, B); cycles = 4; break;
        case 0x0C: C = inc_r(gb, C); cycles = 4; break;
        case 0x14: D = inc_r(gb, D); cycles = 4; break;
        case 0x1C: E = inc_r(gb, E); cycles = 4; break;
        case 0x24: H = inc_r(gb, H); cycles = 4; break;
        case 0x2C: L = inc_r(gb, L); cycles = 4; break;
        case 0x34: WRITE8(HL, inc_r(gb, READ8(HL))); cycles = 12; break;
        case 0x3C: A = inc_r(gb, A); cycles = 4; break;
        
        /* DEC r8 */
        case 0x05: B = dec_r(gb, B); cycles = 4; break;
        case 0x0D: C = dec_r(gb, C); cycles = 4; break;
        case 0x15: D = dec_r(gb, D); cycles = 4; break;
        case 0x1D: E = dec_r(gb, E); cycles = 4; break;
        case 0x25: H = dec_r(gb, H); cycles = 4; break;
        case 0x2D: L = dec_r(gb, L); cycles = 4; break;
        case 0x35: WRITE8(HL, dec_r(gb, READ8(HL))); cycles = 12; break;
        case 0x3D: A = dec_r(gb, A); cycles = 4; break;
        
        /* LD r8, d8 */
        case 0x06: B = FETCH8(); cycles = 8; break;
        case 0x0E: C = FETCH8(); cycles = 8; break;
        case 0x16: D = FETCH8(); cycles = 8; break;
        case 0x1E: E = FETCH8(); cycles = 8; break;
        case 0x26: H = FETCH8(); cycles = 8; break;
        case 0x2E: L = FETCH8(); cycles = 8; break;
        case 0x36: WRITE8(HL, FETCH8()); cycles = 12; break;
        case 0x3E: A = FETCH8(); cycles = 8; break;
        
        /* RLCA, RRCA, RLA, RRA */
        case 0x07: { u8 c = A >> 7; A = (A << 1) | c; F = c ? FLAG_C : 0; cycles = 4; break; }
        case 0x0F: { u8 c = A & 1; A = (A >> 1) | (c << 7); F = c ? FLAG_C : 0; cycles = 4; break; }
        case 0x17: { u8 c = A >> 7; u8 oc = FLAG_GET(FLAG_C) ? 1 : 0; A = (A << 1) | oc; F = c ? FLAG_C : 0; cycles = 4; break; }
        case 0x1F: { u8 c = A & 1; u8 oc = FLAG_GET(FLAG_C) ? 1 : 0; A = (A >> 1) | (oc << 7); F = c ? FLAG_C : 0; cycles = 4; break; }
        
        /* LD (a16), SP */
        case 0x08: tmp16 = FETCH16(); WRITE16(tmp16, SP); cycles = 20; break;
        
        /* ADD HL, r16 */
        case 0x09: add_hl(gb, BC); cycles = 8; break;
        case 0x19: add_hl(gb, DE); cycles = 8; break;
        case 0x29: add_hl(gb, HL); cycles = 8; break;
        case 0x39: add_hl(gb, SP); cycles = 8; break;
        
        /* LD A, (r16) */
        case 0x0A: A = READ8(BC); cycles = 8; break;
        case 0x1A: A = READ8(DE); cycles = 8; break;
        case 0x2A: A = READ8(HL); HL++; cycles = 8; break;
        case 0x3A: A = READ8(HL); HL--; cycles = 8; break;
        
        /* DEC r16 */
        case 0x0B: BC--; cycles = 8; break;
        case 0x1B: DE--; cycles = 8; break;
        case 0x2B: HL--; cycles = 8; break;
        case 0x3B: SP--; cycles = 8; break;
        
        /* STOP / HALT */
        case 0x10: gb->cpu.stopped = 1; cycles = 4; break;
        case 0x76: gb->cpu.halted = 1; cycles = 4; break;
        
        /* JR */
        case 0x18: imm8 = (s8)FETCH8(); PC += imm8; cycles = 12; break;
        case 0x20: imm8 = (s8)FETCH8(); if (!FLAG_GET(FLAG_Z)) { PC += imm8; cycles = 12; } else cycles = 8; break;
        case 0x28: imm8 = (s8)FETCH8(); if (FLAG_GET(FLAG_Z))  { PC += imm8; cycles = 12; } else cycles = 8; break;
        case 0x30: imm8 = (s8)FETCH8(); if (!FLAG_GET(FLAG_C)) { PC += imm8; cycles = 12; } else cycles = 8; break;
        case 0x38: imm8 = (s8)FETCH8(); if (FLAG_GET(FLAG_C))  { PC += imm8; cycles = 12; } else cycles = 8; break;
        
        /* DAA */
        case 0x27: {
            if (!FLAG_GET(FLAG_N)) {
                if (FLAG_GET(FLAG_C) || A > 0x99) { A += 0x60; FLAG_SET(FLAG_C); }
                if (FLAG_GET(FLAG_H) || (A & 0x0F) > 0x09) { A += 0x06; }
            } else {
                if (FLAG_GET(FLAG_C)) A -= 0x60;
                if (FLAG_GET(FLAG_H)) A -= 0x06;
            }
            FLAG_CLEAR(FLAG_H);
            FLAG_COND(A == 0, FLAG_Z);
            cycles = 4; break;
        }
        
        /* CPL */
        case 0x2F: A = ~A; FLAG_SET(FLAG_N); FLAG_SET(FLAG_H); cycles = 4; break;
        
        /* SCF */
        case 0x37: FLAG_CLEAR(FLAG_N); FLAG_CLEAR(FLAG_H); FLAG_SET(FLAG_C); cycles = 4; break;
        
        /* CCF */
        case 0x3F: FLAG_CLEAR(FLAG_N); FLAG_CLEAR(FLAG_H); if(FLAG_GET(FLAG_C)) FLAG_CLEAR(FLAG_C); else FLAG_SET(FLAG_C); cycles = 4; break;
        
        /* LD r8, r8 block (0x40-0x7F) */
        case 0x40: B = B; break; case 0x41: B = C; break; case 0x42: B = D; break; case 0x43: B = E; break;
        case 0x44: B = H; break; case 0x45: B = L; break; case 0x46: B = READ8(HL); cycles = 8; break; case 0x47: B = A; break;
        case 0x48: C = B; break; case 0x49: C = C; break; case 0x4A: C = D; break; case 0x4B: C = E; break;
        case 0x4C: C = H; break; case 0x4D: C = L; break; case 0x4E: C = READ8(HL); cycles = 8; break; case 0x4F: C = A; break;
        case 0x50: D = B; break; case 0x51: D = C; break; case 0x52: D = D; break; case 0x53: D = E; break;
        case 0x54: D = H; break; case 0x55: D = L; break; case 0x56: D = READ8(HL); cycles = 8; break; case 0x57: D = A; break;
        case 0x58: E = B; break; case 0x59: E = C; break; case 0x5A: E = D; break; case 0x5B: E = E; break;
        case 0x5C: E = H; break; case 0x5D: E = L; break; case 0x5E: E = READ8(HL); cycles = 8; break; case 0x5F: E = A; break;
        case 0x60: H = B; break; case 0x61: H = C; break; case 0x62: H = D; break; case 0x63: H = E; break;
        case 0x64: H = H; break; case 0x65: H = L; break; case 0x66: H = READ8(HL); cycles = 8; break; case 0x67: H = A; break;
        case 0x68: L = B; break; case 0x69: L = C; break; case 0x6A: L = D; break; case 0x6B: L = E; break;
        case 0x6C: L = H; break; case 0x6D: L = L; break; case 0x6E: L = READ8(HL); cycles = 8; break; case 0x6F: L = A; break;
        case 0x70: WRITE8(HL, B); cycles = 8; break; case 0x71: WRITE8(HL, C); cycles = 8; break;
        case 0x72: WRITE8(HL, D); cycles = 8; break; case 0x73: WRITE8(HL, E); cycles = 8; break;
        case 0x74: WRITE8(HL, H); cycles = 8; break; case 0x75: WRITE8(HL, L); cycles = 8; break;
        case 0x77: WRITE8(HL, A); cycles = 8; break;
        case 0x78: A = B; break; case 0x79: A = C; break; case 0x7A: A = D; break; case 0x7B: A = E; break;
        case 0x7C: A = H; break; case 0x7D: A = L; break; case 0x7E: A = READ8(HL); cycles = 8; break; case 0x7F: A = A; break;
        
        /* ALU A, r8 (0x80-0xBF) */
        case 0x80: add_a(gb, B); break; case 0x81: add_a(gb, C); break; case 0x82: add_a(gb, D); break; case 0x83: add_a(gb, E); break;
        case 0x84: add_a(gb, H); break; case 0x85: add_a(gb, L); break; case 0x86: add_a(gb, READ8(HL)); cycles=8; break; case 0x87: add_a(gb, A); break;
        case 0x88: adc_a(gb, B); break; case 0x89: adc_a(gb, C); break; case 0x8A: adc_a(gb, D); break; case 0x8B: adc_a(gb, E); break;
        case 0x8C: adc_a(gb, H); break; case 0x8D: adc_a(gb, L); break; case 0x8E: adc_a(gb, READ8(HL)); cycles=8; break; case 0x8F: adc_a(gb, A); break;
        case 0x90: sub_a(gb, B); break; case 0x91: sub_a(gb, C); break; case 0x92: sub_a(gb, D); break; case 0x93: sub_a(gb, E); break;
        case 0x94: sub_a(gb, H); break; case 0x95: sub_a(gb, L); break; case 0x96: sub_a(gb, READ8(HL)); cycles=8; break; case 0x97: sub_a(gb, A); break;
        case 0x98: sbc_a(gb, B); break; case 0x99: sbc_a(gb, C); break; case 0x9A: sbc_a(gb, D); break; case 0x9B: sbc_a(gb, E); break;
        case 0x9C: sbc_a(gb, H); break; case 0x9D: sbc_a(gb, L); break; case 0x9E: sbc_a(gb, READ8(HL)); cycles=8; break; case 0x9F: sbc_a(gb, A); break;
        case 0xA0: and_a(gb, B); break; case 0xA1: and_a(gb, C); break; case 0xA2: and_a(gb, D); break; case 0xA3: and_a(gb, E); break;
        case 0xA4: and_a(gb, H); break; case 0xA5: and_a(gb, L); break; case 0xA6: and_a(gb, READ8(HL)); cycles=8; break; case 0xA7: and_a(gb, A); break;
        case 0xA8: xor_a(gb, B); break; case 0xA9: xor_a(gb, C); break; case 0xAA: xor_a(gb, D); break; case 0xAB: xor_a(gb, E); break;
        case 0xAC: xor_a(gb, H); break; case 0xAD: xor_a(gb, L); break; case 0xAE: xor_a(gb, READ8(HL)); cycles=8; break; case 0xAF: xor_a(gb, A); break;
        case 0xB0: or_a(gb, B); break; case 0xB1: or_a(gb, C); break; case 0xB2: or_a(gb, D); break; case 0xB3: or_a(gb, E); break;
        case 0xB4: or_a(gb, H); break; case 0xB5: or_a(gb, L); break; case 0xB6: or_a(gb, READ8(HL)); cycles=8; break; case 0xB7: or_a(gb, A); break;
        case 0xB8: cp_a(gb, B); break; case 0xB9: cp_a(gb, C); break; case 0xBA: cp_a(gb, D); break; case 0xBB: cp_a(gb, E); break;
        case 0xBC: cp_a(gb, H); break; case 0xBD: cp_a(gb, L); break; case 0xBE: cp_a(gb, READ8(HL)); cycles=8; break; case 0xBF: cp_a(gb, A); break;
        
        /* RET cc */
        case 0xC0: if (!FLAG_GET(FLAG_Z)) { POP(PC); cycles = 20; } else cycles = 8; break;
        case 0xC8: if (FLAG_GET(FLAG_Z))  { POP(PC); cycles = 20; } else cycles = 8; break;
        case 0xD0: if (!FLAG_GET(FLAG_C)) { POP(PC); cycles = 20; } else cycles = 8; break;
        case 0xD8: if (FLAG_GET(FLAG_C))  { POP(PC); cycles = 20; } else cycles = 8; break;
        
        /* POP r16 */
        case 0xC1: POP(BC); cycles = 12; break;
        case 0xD1: POP(DE); cycles = 12; break;
        case 0xE1: POP(HL); cycles = 12; break;
        case 0xF1: POP(AF); F &= 0xF0; cycles = 12; break;
        
        /* JP cc, a16 */
        case 0xC2: tmp16 = FETCH16(); if (!FLAG_GET(FLAG_Z)) { PC = tmp16; cycles = 16; } else cycles = 12; break;
        case 0xCA: tmp16 = FETCH16(); if (FLAG_GET(FLAG_Z))  { PC = tmp16; cycles = 16; } else cycles = 12; break;
        case 0xD2: tmp16 = FETCH16(); if (!FLAG_GET(FLAG_C)) { PC = tmp16; cycles = 16; } else cycles = 12; break;
        case 0xDA: tmp16 = FETCH16(); if (FLAG_GET(FLAG_C))  { PC = tmp16; cycles = 16; } else cycles = 12; break;
        case 0xC3: PC = FETCH16(); cycles = 16; break;
        case 0xE9: PC = HL; cycles = 4; break;
        
        /* CALL cc, a16 */
        case 0xC4: tmp16 = FETCH16(); if (!FLAG_GET(FLAG_Z)) { PUSH(PC); PC = tmp16; cycles = 24; } else cycles = 12; break;
        case 0xCC: tmp16 = FETCH16(); if (FLAG_GET(FLAG_Z))  { PUSH(PC); PC = tmp16; cycles = 24; } else cycles = 12; break;
        case 0xD4: tmp16 = FETCH16(); if (!FLAG_GET(FLAG_C)) { PUSH(PC); PC = tmp16; cycles = 24; } else cycles = 12; break;
        case 0xDC: tmp16 = FETCH16(); if (FLAG_GET(FLAG_C))  { PUSH(PC); PC = tmp16; cycles = 24; } else cycles = 12; break;
        case 0xCD: tmp16 = FETCH16(); PUSH(PC); PC = tmp16; cycles = 24; break;
        
        /* PUSH r16 */
        case 0xC5: PUSH(BC); cycles = 16; break;
        case 0xD5: PUSH(DE); cycles = 16; break;
        case 0xE5: PUSH(HL); cycles = 16; break;
        case 0xF5: PUSH(AF); cycles = 16; break;
        
        /* ALU A, d8 */
        case 0xC6: add_a(gb, FETCH8()); cycles = 8; break;
        case 0xCE: adc_a(gb, FETCH8()); cycles = 8; break;
        case 0xD6: sub_a(gb, FETCH8()); cycles = 8; break;
        case 0xDE: sbc_a(gb, FETCH8()); cycles = 8; break;
        case 0xE6: and_a(gb, FETCH8()); cycles = 8; break;
        case 0xEE: xor_a(gb, FETCH8()); cycles = 8; break;
        case 0xF6: or_a(gb, FETCH8()); cycles = 8; break;
        case 0xFE: cp_a(gb, FETCH8()); cycles = 8; break;
        
        /* RST */
        case 0xC7: PUSH(PC); PC = 0x00; cycles = 16; break;
        case 0xCF: PUSH(PC); PC = 0x08; cycles = 16; break;
        case 0xD7: PUSH(PC); PC = 0x10; cycles = 16; break;
        case 0xDF: PUSH(PC); PC = 0x18; cycles = 16; break;
        case 0xE7: PUSH(PC); PC = 0x20; cycles = 16; break;
        case 0xEF: PUSH(PC); PC = 0x28; cycles = 16; break;
        case 0xF7: PUSH(PC); PC = 0x30; cycles = 16; break;
        case 0xFF: PUSH(PC); PC = 0x38; cycles = 16; break;
        
        /* RET */
        case 0xC9: POP(PC); cycles = 16; break;
        case 0xD9: POP(PC); IME = 1; cycles = 16; break; /* RETI */
        
        /* CB prefix */
        case 0xCB: cycles = cpu_step_cb(gb); break;
        
        /* LDH (a8), A / LDH A, (a8) */
        case 0xE0: WRITE8(0xFF00 | FETCH8(), A); cycles = 12; break;
        case 0xF0: A = READ8(0xFF00 | FETCH8()); cycles = 12; break;
        
        /* LD (C), A / LD A, (C) */
        case 0xE2: WRITE8(0xFF00 | C, A); cycles = 8; break;
        case 0xF2: A = READ8(0xFF00 | C); cycles = 8; break;
        
        /* LD (a16), A / LD A, (a16) */
        case 0xEA: WRITE8(FETCH16(), A); cycles = 16; break;
        case 0xFA: A = READ8(FETCH16()); cycles = 16; break;
        
        /* ADD SP, r8 */
        case 0xE8: {
            imm8 = (s8)FETCH8();
            u32 res = (u32)SP + imm8;
            FLAG_CLEAR(FLAG_Z); FLAG_CLEAR(FLAG_N);
            FLAG_COND((SP & 0xF) + (imm8 & 0xF) > 0xF, FLAG_H);
            FLAG_COND((SP & 0xFF) + (imm8 & 0xFF) > 0xFF, FLAG_C);
            SP = (u16)res;
            cycles = 16; break;
        }
        
        /* LD HL, SP+r8 */
        case 0xF8: {
            imm8 = (s8)FETCH8();
            u32 res = (u32)SP + imm8;
            FLAG_CLEAR(FLAG_Z); FLAG_CLEAR(FLAG_N);
            FLAG_COND((SP & 0xF) + (imm8 & 0xF) > 0xF, FLAG_H);
            FLAG_COND((SP & 0xFF) + (imm8 & 0xFF) > 0xFF, FLAG_C);
            HL = (u16)res;
            cycles = 12; break;
        }
        
        /* LD SP, HL */
        case 0xF9: SP = HL; cycles = 8; break;
        
        /* DI / EI */
        case 0xF3: IME = 0; cycles = 4; break;
        case 0xFB: IME = 1; cycles = 4; break;
        
        default:
            /* Undefined opcode */
            cycles = 4;
            break;
    }
    
    return cycles;
}
