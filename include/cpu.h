#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Gameboy;

#define INT_VBLANK 0x01
#define INT_LCD 0x02
#define INT_TIMER 0x04
#define INT_SERIAL 0x08
#define INT_JOYPAD 0x10

struct Registers {
    union {
        u8 r8[12];
        u16 r16[6];
    };
};

enum {
    R8_F = 0,
    R8_A,
    R8_C,
    R8_B,
    R8_E,
    R8_D,
    R8_L,
    R8_H,
    R8_SP_P,
    R8_SP_S,
    R8_PC_C,
    R8_PC_P
};

enum {
    R16_AF = 0,
    R16_BC,
    R16_DE,
    R16_HL,
    R16_SP,
    R16_PC
};

class CPU {
public:
    CPU(Gameboy* gameboy);
    inline void setA(u8 val) { this->registers.r8[R8_A] = val; }
    inline void setB(u8 val) { this->registers.r8[R8_B] = val; }
    inline u8 getA() const { return this->registers.r8[R8_A]; }
    inline u8 getB() const { return this->registers.r8[R8_B]; }
    inline void setAF(u16 val) { this->registers.r16[R16_AF] = val; }
    inline void setBC(u16 val) { this->registers.r16[R16_BC] = val; }
    inline void setDE(u16 val) { this->registers.r16[R16_DE] = val; }
    inline void setHL(u16 val) { this->registers.r16[R16_HL] = val; }
    inline void setSP(u16 val) { this->registers.r16[R16_SP] = val; }
    inline void setPC(u16 val) { this->registers.r16[R16_PC] = val; }
    inline u16 getAF() const { return this->registers.r16[R16_AF]; }
    inline u16 getBC() const { return this->registers.r16[R16_BC]; }
    inline u16 getDE() const { return this->registers.r16[R16_DE]; }
    inline u16 getHL() const { return this->registers.r16[R16_HL]; }
    inline u16 getSP() const { return this->registers.r16[R16_SP]; }
    inline u16 getPC() const { return this->registers.r16[R16_PC]; }

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void run();

    inline void advanceCycles(u64 cycles) {
        this->cycleCount += cycles;

        if(this->cycleCount >= this->eventCycle) {
            this->eventCycle = UINT64_MAX;

            this->updateEvents();
        }
    }

    inline u64 getCycle() {
        return this->cycleCount;
    }

    inline void setEventCycle(u64 cycle) {
        if(cycle < this->eventCycle) {
            this->eventCycle = cycle > this->cycleCount ? cycle : this->cycleCount;
        }
    }
private:
    void updateEvents();

    void alu(u8 func, u8 val);
    u8 rot(u8 func, u8 val);

    Gameboy* gameboy;

    u64 cycleCount;
    u64 eventCycle;

    struct Registers registers;

    bool haltState;
    bool haltBug;

    bool ime;
    u64 imeCycle;
};