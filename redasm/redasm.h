#ifndef REDASM_H
#define REDASM_H

#include <memory>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "support/utils.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s64 register_t;
typedef u64 address_t;
typedef u64 offset_t;

#define RE_UNUSED(x)           (void)x
#define ENTRYPOINT_FUNCTION    "entrypoint"
#define REGISTER_INVALID       static_cast<s64>(-1)

namespace REDasm {

namespace ByteOrder {
    enum { LittleEndian = 0, BigEndian = 1, };

    /*
    static int current()
    {
        int i = 1;
        char* p = reinterpret_cast<char*>(&i);

        if (p[0] == 1)
            return ByteOrder::LittleEndian;

        return ByteOrder::BigEndian;
    }
    */
}

namespace SegmentTypes {
    enum: u32 {
        None       = 0x00000000,

        Code       = 0x00000001,
        Data       = 0x00000002,

        Read       = 0x00000010,
        Write      = 0x00000020,
        Bss        = 0x00000040,
    };
}

namespace InstructionTypes {
    enum: u32 {
        None        = 0x00000000, Stop = 0x00000001, Nop = 0x00000002,
        Jump        = 0x00000004, Call = 0x00000008,
        Add         = 0x00000010, Sub  = 0x00000020, Mul = 0x00000040, Div = 0x0000080, Mod = 0x00000100,
        And         = 0x00000200, Or   = 0x00000400, Xor = 0x00000800, Not = 0x0001000,
        Push        = 0x00002000, Pop  = 0x00004000,
        Compare     = 0x00008000,

        Conditional = 0x01000000, Privileged = 0x02000000, JumpTable = 0x04000000,
        Invalid     = 0x10000000,
        Branch      = Jump | Call,
    };
}

namespace OperandTypes {
    enum: u32 {
        None = 0,
        Register,     // Register
        Immediate,    // Immediate Value
        Memory,       // Direct Memory Pointer
        Displacement, // Indirect Memory Pointer
    };
}

struct Buffer
{
    Buffer(): data(NULL), length(0) { }
    Buffer(u8* data, u64 length): data(data), length(length) { }
    Buffer(char* data, u64 length): data(reinterpret_cast<u8*>(data)), length(length) { }
    Buffer operator +(u64 v) const { Buffer b(data, length); b.data += v; b.length -= v; return b; }
    Buffer& operator +=(u64 v) { data += v; length -= v; return *this; }
    Buffer& operator++(int) { *this += 1; return *this; }
    u8 operator[](int index) const { return data[index]; }
    u8 operator*() const { return *data; }
    bool eob() const { return !length; }

    u8* data;
    u64 length;
};

struct Segment
{
    Segment(): offset(0), address(0), endaddress(0), type(0) { }
    Segment(const std::string& name, offset_t offset, address_t address, u64 size, u64 flags): name(name), offset(offset), address(address), endaddress(address + size), type(flags) { }

    u64 size() const { return endaddress - address; }
    bool contains(address_t address) const { return (address >= this->address) && (address < endaddress); }
    bool is(u32 t) const { return type & t; }

    std::string name;
    offset_t offset;
    address_t address, endaddress;
    u32 type;
};

struct RegisterOperand
{
    RegisterOperand(): type(0), r(REGISTER_INVALID) { }
    RegisterOperand(u32 type, register_t r): type(type), r(r) { }
    RegisterOperand(register_t r): type(0), r(r) { }

    u32 type;
    register_t r;

    bool isValid() const { return r != REGISTER_INVALID; }
};

struct MemoryOperand
{
    MemoryOperand(): scale(1), displacement(0) { }
    MemoryOperand(const RegisterOperand& base, const RegisterOperand& index, s32 scale, s64 displacement): base(base), index(index), scale(scale), displacement(displacement) { }

    RegisterOperand base, index;
    s32 scale;
    s64 displacement;

    bool displacementOnly() const { return !base.isValid() && !index.isValid(); }
};

struct Operand
{
    Operand(): type(OperandTypes::None), pos(-1), u_value(0) { }
    Operand(u32 type, s32 value, s32 pos): type(type), pos(pos), s_value(value) { }
    Operand(u32 type, u32 value, s32 pos): type(type), pos(pos), u_value(value) { }
    Operand(u32 type, s64 value, s32 pos): type(type), pos(pos), s_value(value) { }
    Operand(u32 type, u64 value, s32 pos): type(type), pos(pos), u_value(value) { }

    u32 type;
    s32 pos;
    RegisterOperand reg;
    MemoryOperand mem;

    union {
        s64 s_value;
        u64 u_value;
    };

    bool is(u32 t) const { return type == t; }
};

struct Instruction
{
    Instruction(): address(0), type(0), size(0), segment(NULL), userdata(NULL) { }
    ~Instruction() { reset(); }

    std::function<void(void*)> free;

    std::string mnemonic, signature;
    std::vector<Operand> operands;
    std::list<std::string> comments;
    address_t address;
    u32 type, size;
    Segment* segment;
    void* userdata;

    bool is(u32 t) const { return type & t; }
    bool isInvalid() const { return type == InstructionTypes::Invalid; }
    void reset() { type = 0; operands.clear(); if(free && userdata) { free(userdata); userdata = NULL; } }
    address_t endAddress() const { return address + size; }

    Instruction& cmt(const std::string& s) { comments.push_back(s); return *this; }
    Instruction& op(Operand op) { op.pos = operands.size(); operands.push_back(op); return *this; }
    Instruction& mem(address_t v) { operands.push_back(Operand(OperandTypes::Memory, v, operands.size())); return *this; }
    template<typename T> Instruction& imm(T v) { operands.push_back(Operand(OperandTypes::Immediate, v, operands.size())); return *this; }
    template<typename T> Instruction& disp(register_t base, T displacement) { return disp(base, REGISTER_INVALID, displacement); }
    template<typename T> Instruction& disp(register_t base, register_t index, T displacement) { return disp(base, index, 1, displacement); }
    template<typename T> Instruction& disp(register_t base, register_t index, s32 scale, T displacement);

    Instruction& reg(register_t r, u64 type = 0)
    {
        Operand op;
        op.pos = operands.size();
        op.type = OperandTypes::Register;
        op.reg = RegisterOperand(type, r);

        operands.push_back(op);
        return *this;
    }
};

template<typename T> Instruction& Instruction::disp(register_t base, register_t index, s32 scale, T displacement)
{
    Operand op;
    op.pos = operands.size();
    op.type = OperandTypes::Displacement;
    op.mem = MemoryOperand(RegisterOperand(base), RegisterOperand(index), scale, displacement);

    operands.push_back(op);
    return *this;
}

typedef std::shared_ptr<Instruction> InstructionPtr;
typedef std::vector<Operand> OperandList;

typedef std::set<address_t> AddressSet;
typedef std::vector<address_t> AddressVector;
typedef std::vector<Segment> SegmentVector;
}

#endif // REDASM_H