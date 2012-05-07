/* 
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Segovia <benjamin.segovia@intel.com>
 */

/**
 * \file gen_eu.hpp
 * \author Benjamin Segovia <benjamin.segovia@intel.com>
 * This is a revamped Gen ISA encoder from Mesa code base
 */

#ifndef GEN_EU_H
#define GEN_EU_H

#include "backend/gen_defs.hpp"
#include "sys/platform.hpp"
#include <cassert>

#define GEN_SWIZZLE4(a,b,c,d) (((a)<<0) | ((b)<<2) | ((c)<<4) | ((d)<<6))
#define GEN_GET_SWZ(swz, idx) (((swz) >> ((idx)*2)) & 0x3)

#define GEN_SWIZZLE_NOOP      GEN_SWIZZLE4(0,1,2,3)
#define GEN_SWIZZLE_XYZW      GEN_SWIZZLE4(0,1,2,3)
#define GEN_SWIZZLE_XXXX      GEN_SWIZZLE4(0,0,0,0)
#define GEN_SWIZZLE_YYYY      GEN_SWIZZLE4(1,1,1,1)
#define GEN_SWIZZLE_ZZZZ      GEN_SWIZZLE4(2,2,2,2)
#define GEN_SWIZZLE_WWWW      GEN_SWIZZLE4(3,3,3,3)
#define GEN_SWIZZLE_XYXY      GEN_SWIZZLE4(0,1,0,1)

#define WRITEMASK_X     0x1
#define WRITEMASK_Y     0x2
#define WRITEMASK_XY    0x3
#define WRITEMASK_Z     0x4
#define WRITEMASK_XZ    0x5
#define WRITEMASK_YZ    0x6
#define WRITEMASK_XYZ   0x7
#define WRITEMASK_W     0x8
#define WRITEMASK_XW    0x9
#define WRITEMASK_YW    0xa
#define WRITEMASK_XYW   0xb
#define WRITEMASK_ZW    0xc
#define WRITEMASK_XZW   0xd
#define WRITEMASK_YZW   0xe
#define WRITEMASK_XYZW  0xf

#define GEN_REG_SIZE (8*4)
#define GEN_GRF_SIZE (GEN_REG_SIZE*112)
#define GEN_EU_MAX_INSN_STACK 5

#define VF_ZERO 0x0
#define VF_ONE  0x30
#define VF_NEG  (1<<7)

namespace gbe
{
  /*! Type size in bytes for each Gen type */
  INLINE int typeSize(uint32_t type) {
     switch(type) {
     case GEN_TYPE_UD:
     case GEN_TYPE_D:
     case GEN_TYPE_F:
        return 4;
     case GEN_TYPE_HF:
     case GEN_TYPE_UW:
     case GEN_TYPE_W:
        return 2;
     case GEN_TYPE_UB:
     case GEN_TYPE_B:
        return 1;
     default:
        return 0;
     }
  }

  /*! This is almost always called with a numeric constant argument, so make
   *  things easy to evaluate at compile time:
   */
  INLINE uint32_t cvt(uint32_t val) {
     switch (val) {
       case 0: return 0;
       case 1: return 1;
       case 2: return 2;
       case 4: return 3;
       case 8: return 4;
       case 16: return 5;
       case 32: return 6;
     }
     return 0;
  }

  /*! These are not hardware structs, just something useful to pass around */
  struct GenReg
  {
    /*! Empty constructor */
    INLINE GenReg(void) {}

    /*! General constructor */
    INLINE GenReg(uint32_t file,
                  uint32_t nr,
                  uint32_t subnr,
                  uint32_t type,
                  uint32_t vstride,
                  uint32_t width,
                  uint32_t hstride,
                  uint32_t swizzle,
                  uint32_t writemask)
    {
      if (file == GEN_GENERAL_REGISTER_FILE)
        assert(nr < GEN_MAX_GRF);
      else if (file == GEN_ARCHITECTURE_REGISTER_FILE)
        assert(nr <= GEN_ARF_IP);

      this->type = type;
      this->file = file;
      this->nr = nr;
      this->subnr = subnr * typeSize(type);
      this->negation = 0;
      this->absolute = 0;
      this->vstride = vstride;
      this->width = width;
      this->hstride = hstride;
      this->address_mode = GEN_ADDRESS_DIRECT;
      this->pad0 = 0;
      this->dw1.bits.swizzle = swizzle;
      this->dw1.bits.writemask = writemask;
      this->dw1.bits.indirect_offset = 0;
      this->dw1.bits.pad1 = 0;
    }

    static INLINE GenReg vec16(uint32_t file, uint32_t nr, uint32_t subnr) {
      if (typeSize(file) == 4)
        return GenReg(file,
                      nr,
                      subnr,
                      GEN_TYPE_F,
                      GEN_VERTICAL_STRIDE_8,
                      GEN_WIDTH_8,
                      GEN_HORIZONTAL_STRIDE_1,
                      GEN_SWIZZLE_XYZW,
                      WRITEMASK_XYZW);
      else if (typeSize(file) == 2)
        return GenReg(file,
                      nr,
                      subnr,
                      GEN_TYPE_F,
                      GEN_VERTICAL_STRIDE_16,
                      GEN_WIDTH_16,
                      GEN_HORIZONTAL_STRIDE_1,
                      GEN_SWIZZLE_XYZW,
                      WRITEMASK_XYZW);
      else {
        NOT_IMPLEMENTED;
        return GenReg();
      }
    }

    static INLINE GenReg vec8(uint32_t file, uint32_t nr, uint32_t subnr) {
      return GenReg(file,
                    nr,
                    subnr,
                    GEN_TYPE_F,
                    GEN_VERTICAL_STRIDE_8,
                    GEN_WIDTH_8,
                    GEN_HORIZONTAL_STRIDE_1,
                    GEN_SWIZZLE_XYZW,
                    WRITEMASK_XYZW);
    }

    static INLINE GenReg vec4(uint32_t file, uint32_t nr, uint32_t subnr) {
      return GenReg(file,
                    nr,
                    subnr,
                    GEN_TYPE_F,
                    GEN_VERTICAL_STRIDE_4,
                    GEN_WIDTH_4,
                    GEN_HORIZONTAL_STRIDE_1,
                    GEN_SWIZZLE_XYZW,
                    WRITEMASK_XYZW);
    }

    static INLINE GenReg vec2(uint32_t file, uint32_t nr, uint32_t subnr) {
      return GenReg(file,
                    nr,
                    subnr,
                    GEN_TYPE_F,
                    GEN_VERTICAL_STRIDE_2,
                    GEN_WIDTH_2,
                    GEN_HORIZONTAL_STRIDE_1,
                    GEN_SWIZZLE_XYXY,
                    WRITEMASK_XY);
    }

    static INLINE GenReg vec1(uint32_t file, uint32_t nr, uint32_t subnr) {
      return GenReg(file,
                    nr,
                    subnr,
                    GEN_TYPE_F,
                    GEN_VERTICAL_STRIDE_0,
                    GEN_WIDTH_1,
                    GEN_HORIZONTAL_STRIDE_0,
                    GEN_SWIZZLE_XXXX,
                    WRITEMASK_X);
    }

    static INLINE GenReg retype(GenReg reg, uint32_t type) {
      reg.type = type;
      return reg;
    }

    static INLINE GenReg suboffset(GenReg reg, uint32_t delta) {
      reg.subnr += delta * typeSize(reg.type);
      return reg;
    }

    static INLINE GenReg offset(GenReg reg, uint32_t delta) {
      reg.nr += delta;
      return reg;
    }

    static INLINE GenReg byte_offset(GenReg reg, uint32_t bytes) {
      uint32_t newoffset = reg.nr * GEN_REG_SIZE + reg.subnr + bytes;
      reg.nr = newoffset / GEN_REG_SIZE;
      reg.subnr = newoffset % GEN_REG_SIZE;
      return reg;
    }

    static INLINE GenReg uw16(uint32_t file, uint32_t nr, uint32_t subnr) {
      return suboffset(retype(vec16(file, nr, 0), GEN_TYPE_UW), subnr);
    }

    static INLINE GenReg uw8(uint32_t file, uint32_t nr, uint32_t subnr) {
      return suboffset(retype(vec8(file, nr, 0), GEN_TYPE_UW), subnr);
    }

    static INLINE GenReg uw1(uint32_t file, uint32_t nr, uint32_t subnr) {
      return suboffset(retype(vec1(file, nr, 0), GEN_TYPE_UW), subnr);
    }

    static INLINE GenReg ud16(uint32_t file, uint32_t nr, uint32_t subnr) {
      return retype(vec16(file, nr, subnr), GEN_TYPE_UD);
    }

    static INLINE GenReg ud8(uint32_t file, uint32_t nr, uint32_t subnr) {
      return retype(vec8(file, nr, subnr), GEN_TYPE_UD);
    }

    static INLINE GenReg ud1(uint32_t file, uint32_t nr, uint32_t subnr) {
      return retype(vec1(file, nr, subnr), GEN_TYPE_UD);
    }

    static INLINE GenReg d8(uint32_t file, uint32_t nr, uint32_t subnr) {
      return retype(vec8(file, nr, subnr), GEN_TYPE_D);
    }

    static INLINE GenReg d1(uint32_t file, uint32_t nr, uint32_t subnr) {
      return retype(vec1(file, nr, subnr), GEN_TYPE_D);
    }

    static INLINE GenReg imm(uint32_t type) {
      return GenReg(GEN_IMMEDIATE_VALUE,
                    0,
                    0,
                    type,
                    GEN_VERTICAL_STRIDE_0,
                    GEN_WIDTH_1,
                    GEN_HORIZONTAL_STRIDE_0,
                    0,
                    0);
    }

    static INLINE GenReg immf(float f) {
      GenReg immediate = imm(GEN_TYPE_F);
      immediate.dw1.f = f;
      return immediate;
    }

    static INLINE GenReg immd(int d) {
      GenReg immediate = imm(GEN_TYPE_D);
      immediate.dw1.d = d;
      return immediate;
    }

    static INLINE GenReg immud(uint32_t ud) {
      GenReg immediate = imm(GEN_TYPE_UD);
      immediate.dw1.ud = ud;
      return immediate;
    }

    static INLINE GenReg immuw(uint16_t uw) {
      GenReg immediate = imm(GEN_TYPE_UW);
      immediate.dw1.ud = uw | (uw << 16);
      return immediate;
    }

    static INLINE GenReg immw(short w) {
      GenReg immediate = imm(GEN_TYPE_W);
      immediate.dw1.d = w | (w << 16);
      return immediate;
    }

    static INLINE GenReg immv(uint32_t v) {
      GenReg immediate = imm(GEN_TYPE_V);
      immediate.vstride = GEN_VERTICAL_STRIDE_0;
      immediate.width = GEN_WIDTH_8;
      immediate.hstride = GEN_HORIZONTAL_STRIDE_1;
      immediate.dw1.ud = v;
      return immediate;
    }

    static INLINE GenReg immvf(uint32_t v) {
      GenReg immediate = imm(GEN_TYPE_VF);
      immediate.vstride = GEN_VERTICAL_STRIDE_0;
      immediate.width = GEN_WIDTH_4;
      immediate.hstride = GEN_HORIZONTAL_STRIDE_1;
      immediate.dw1.ud = v;
      return immediate;
    }

    static INLINE GenReg immvf4(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3) {
      GenReg immediate = imm(GEN_TYPE_VF);
      immediate.vstride = GEN_VERTICAL_STRIDE_0;
      immediate.width = GEN_WIDTH_4;
      immediate.hstride = GEN_HORIZONTAL_STRIDE_1;
      immediate.dw1.ud = ((v0 << 0) | (v1 << 8) | (v2 << 16) | (v3 << 24));
      return immediate;
    }

    static INLINE GenReg address(GenReg reg) {
      return immuw(reg.nr * GEN_REG_SIZE + reg.subnr);
    }

    static INLINE GenReg f1grf(uint32_t nr, uint32_t subnr) {
      return vec1(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg f2grf(uint32_t nr, uint32_t subnr) {
      return vec2(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg f4grf(uint32_t nr, uint32_t subnr) {
      return vec4(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg f8grf(uint32_t nr, uint32_t subnr) {
      return vec8(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg f16grf(uint32_t nr, uint32_t subnr) {
      return vec16(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg ud8grf(uint32_t nr, uint32_t subnr) {
      return ud8(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg d8grf(uint32_t nr, uint32_t subnr) {
      return d8(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg ud16grf(uint32_t nr, uint32_t subnr) {
      return ud16(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg uw1grf(uint32_t nr, uint32_t subnr) {
      return uw1(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }
    static INLINE GenReg uw8grf(uint32_t nr, uint32_t subnr) {
      return uw8(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg uw16grf(uint32_t nr, uint32_t subnr) {
      return uw16(GEN_GENERAL_REGISTER_FILE, nr, subnr);
    }

    static INLINE GenReg null(void) {
      return vec8(GEN_ARCHITECTURE_REGISTER_FILE, GEN_ARF_NULL, 0);
    }

    static INLINE GenReg address(uint32_t subnr) {
      return uw1(GEN_ARCHITECTURE_REGISTER_FILE, GEN_ARF_ADDRESS, subnr);
    }

    static INLINE GenReg acc(void) {
      return vec8(GEN_ARCHITECTURE_REGISTER_FILE, GEN_ARF_ACCUMULATOR, 0);
    }

    static INLINE GenReg ip(void) {
      return GenReg(GEN_ARCHITECTURE_REGISTER_FILE, 
                    GEN_ARF_IP, 
                    0,
                    GEN_TYPE_D,
                    GEN_VERTICAL_STRIDE_4,
                    GEN_WIDTH_1,
                    GEN_HORIZONTAL_STRIDE_0,
                    GEN_SWIZZLE_XYZW,
                    WRITEMASK_XYZW);
    }

    static INLINE GenReg notification1(void) {
      return GenReg(GEN_ARCHITECTURE_REGISTER_FILE,
                    GEN_ARF_NOTIFICATION_COUNT,
                    1,
                    GEN_TYPE_UD,
                    GEN_VERTICAL_STRIDE_0,
                    GEN_WIDTH_1,
                    GEN_HORIZONTAL_STRIDE_0,
                    GEN_SWIZZLE_XXXX,
                    WRITEMASK_X);
    }

    static INLINE GenReg flag(uint32_t nr, uint32_t subnr) {
      return uw1(GEN_ARCHITECTURE_REGISTER_FILE, GEN_ARF_FLAG | nr, subnr);
    }

    static INLINE GenReg mask(uint32_t subnr) {
      return uw1(GEN_ARCHITECTURE_REGISTER_FILE, GEN_ARF_MASK, subnr);
    }

    static INLINE GenReg next(GenReg reg) {
      reg.nr++;
      return reg;
    }

    static INLINE GenReg stride(GenReg reg, uint32_t vstride, uint32_t width, uint32_t hstride) {
      reg.vstride = cvt(vstride);
      reg.width = cvt(width) - 1;
      reg.hstride = cvt(hstride);
      return reg;
    }

    static INLINE GenReg vec16(GenReg reg) { return stride(reg, 16,16,1); }
    static INLINE GenReg vec8(GenReg reg) { return stride(reg, 8,8,1); }
    static INLINE GenReg vec4(GenReg reg) { return stride(reg, 4,4,1); }
    static INLINE GenReg vec2(GenReg reg) { return stride(reg, 2,2,1); }
    static INLINE GenReg vec1(GenReg reg) { return stride(reg, 0,1,0); }

    static INLINE GenReg getElement(GenReg reg, uint32_t elt) {
      return vec1(suboffset(reg, elt));
    }

    static INLINE GenReg getElementUD(GenReg reg, uint32_t elt) {
      return vec1(suboffset(retype(reg, GEN_TYPE_UD), elt));
    }

    static INLINE GenReg getElementD(GenReg reg, uint32_t elt) {
      return vec1(suboffset(retype(reg, GEN_TYPE_D), elt));
    }

    static INLINE GenReg swizzle(GenReg reg, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
      assert(reg.file != GEN_IMMEDIATE_VALUE);
      reg.dw1.bits.swizzle = GEN_SWIZZLE4(GEN_GET_SWZ(reg.dw1.bits.swizzle, x),
                                          GEN_GET_SWZ(reg.dw1.bits.swizzle, y),
                                          GEN_GET_SWZ(reg.dw1.bits.swizzle, z),
                                          GEN_GET_SWZ(reg.dw1.bits.swizzle, w));
      return reg;
    }

    static INLINE GenReg swizzle1(GenReg reg, uint32_t x) {
      return swizzle(reg, x, x, x, x);
    }

    static INLINE GenReg writemask(GenReg reg, uint32_t mask) {
      assert(reg.file != GEN_IMMEDIATE_VALUE);
      reg.dw1.bits.writemask &= mask;
      return reg;
    }

    static INLINE GenReg set_writemask(GenReg reg, uint32_t mask) {
      assert(reg.file != GEN_IMMEDIATE_VALUE);
      reg.dw1.bits.writemask = mask;
      return reg;
    }

    static INLINE GenReg negate(GenReg reg) {
      reg.negation ^= 1;
      return reg;
    }

    static INLINE GenReg abs(GenReg reg) {
      reg.absolute = 1;
      reg.negation = 0;
      return reg;
    }

    static INLINE GenReg vec4_indirect(uint32_t subnr, int offset) {
      GenReg reg =  f4grf(0, 0);
      reg.subnr = subnr;
      reg.address_mode = GEN_ADDRESS_REGISTER_INDIRECT_REGISTER;
      reg.dw1.bits.indirect_offset = offset;
      return reg;
    }

    static INLINE GenReg vec1_indirect(uint32_t subnr, int offset) {
      GenReg reg =  f1grf(0, 0);
      reg.subnr = subnr;
      reg.address_mode = GEN_ADDRESS_REGISTER_INDIRECT_REGISTER;
      reg.dw1.bits.indirect_offset = offset;
      return reg;
    }

    static INLINE bool same_reg(GenReg r1, GenReg r2) {
      return r1.file == r2.file && r1.nr == r2.nr;
    }

    uint32_t type:4;
    uint32_t file:2;
    uint32_t nr:8;
    uint32_t subnr:5;        /* :1 in align16 */
    uint32_t negation:1;     /* source only */
    uint32_t absolute:1;     /* source only */
    uint32_t vstride:4;      /* source only */
    uint32_t width:3;        /* src only, align1 only */
    uint32_t hstride:2;      /* align1 only */
    uint32_t address_mode:1; /* relative addressing, hopefully! */
    uint32_t pad0:1;

    union {
      struct {
        uint32_t swizzle:8;          /* src only, align16 only */
        uint32_t writemask:4;        /* dest only, align16 only */
        int32_t  indirect_offset:10; /* relative addressing offset */
        uint32_t pad1:10;            /* two dwords total */
      } bits;
      float f;
      int32_t d;
      uint32_t ud;
    } dw1;
  };

  /*! The state for each instruction.  */
  struct GenInstructionState
  {
    uint32_t execWidth:6;
    uint32_t quarterControl:2;
    uint32_t noMask:1;
    uint32_t flag:1;
    uint32_t subFlag:1;
    uint32_t predicate:4;
    uint32_t inversePredicate:1;
  };

  /*! Helper structure to emit Gen instructions */
  struct GenEmitter
  {
    /*! simdWidth is the default width for the instructions */
    GenEmitter(uint32_t simdWidth, uint32_t gen);
    /*! TODO use a vector */
    enum { MAX_INSN_NUM = 8192 };
    /*! Size of the stack (should be large enough) */
    enum { MAX_STATE_NUM = 16 };
    /*! Push the current instruction state */
    INLINE void push(void) {
      assert(stateNum < MAX_STATE_NUM);
      stack[stateNum++] = curr;
    }
    /*! Pop the latest pushed state */
    INLINE void pop(void) {
      assert(stateNum > 0);
      curr = stack[--stateNum];
    }
    /*! TODO Update that with a vector */
    GenInstruction store[MAX_INSN_NUM]; 
    /*! Number of instructions currently pushed */
    uint32_t insnNum;
    /*! Current instruction state to use */
    GenInstructionState curr;
    /*! State used to encode the instructions */
    GenInstructionState stack[MAX_STATE_NUM];
    /*! Number of states currently pushed */
    uint32_t stateNum;
    /*! Gen generation to encode */
    uint32_t gen;

    ////////////////////////////////////////////////////////////////////////
    // Encoding functions
    ////////////////////////////////////////////////////////////////////////

#define ALU1(OP) GenInstruction *OP(GenReg dest, GenReg src0);
#define ALU2(OP) GenInstruction *OP(GenReg dest, GenReg src0, GenReg src1);
#define ALU3(OP) GenInstruction *OP(GenReg dest, GenReg src0, GenReg src1, GenReg src2);
    ALU1(MOV)
    ALU1(RNDZ)
    ALU1(RNDE)
    ALU2(SEL)
    ALU1(NOT)
    ALU2(AND)
    ALU2(OR)
    ALU2(XOR)
    ALU2(SHR)
    ALU2(SHL)
    ALU2(RSR)
    ALU2(RSL)
    ALU2(ASR)
    ALU2(ADD)
    ALU2(MUL)
    ALU1(FRC)
    ALU1(RNDD)
    ALU2(MAC)
    ALU2(MACH)
    ALU1(LZD)
    ALU2(LINE)
    ALU2(PLN)
    ALU3(MAD)
#undef ALU1
#undef ALU2
#undef ALU3

    /*! Jump indexed instruction */
    void JMPI(GenReg src);
    /*! Compare instructions */
    void CMP(GenReg dst, uint32_t conditional, GenReg src0, GenReg src1);
    /*! EOT is used to finish GPGPU threads */
    void EOT(uint32_t msg_nr);
    /*! No-op */
    void NOP(void);
    /*! Wait instruction (used for the barrier) */
    void WAIT(void);
    /*! Untyped read (upto 4 channels) */
    void UNTYPED_READ(GenReg dst, GenReg src, uint32_t bti, uint32_t elemNum);
    /*! Untyped write (upto 4 channels) */
    void UNTYPED_WRITE(GenReg src, uint32_t bti, uint32_t elemNum);
    /*! Byte gather (for unaligned bytes, shorts and ints) */
    void BYTE_GATHER(GenReg dst, GenReg src, uint32_t bti, uint32_t type);
    /*! Byte scatter (for unaligned bytes, shorts and ints) */
    void BYTE_SCATTER(GenReg src, uint32_t bti, uint32_t type);
    /*! Send instruction for the sampler */
    void SAMPLE(GenReg dest,
                uint32_t msg_reg_nr,
                GenReg src0,
                uint32_t bti,
                uint32_t sampler,
                uint32_t writemask,
                uint32_t msg_type,
                uint32_t response_length,
                uint32_t msg_length,
                uint32_t header_present,
                uint32_t simd_mode,
                uint32_t return_format);
    /*! Extended math function, float[8] */
    void MATH(GenReg dest, uint32_t function, GenReg src0, GenReg src1);

    /*! Patch JMPI (located at index insnID) with the given jump distance */
    void patchJMPI(uint32_t insnID, int32_t jumpDistance);

    ////////////////////////////////////////////////////////////////////////
    // Helper functions to encode
    ////////////////////////////////////////////////////////////////////////
    void setHeader(GenInstruction *insn);
    void setDst(GenInstruction *insn, GenReg dest);
    void setSrc0(GenInstruction *insn, GenReg reg);
    void setSrc1(GenInstruction *insn, GenReg reg);
    GenInstruction *next(uint32_t opcode);
  };

  INLINE bool brw_is_single_value_swizzle(int swiz) {
     return (swiz == GEN_SWIZZLE_XXXX ||
             swiz == GEN_SWIZZLE_YYYY ||
             swiz == GEN_SWIZZLE_ZZZZ ||
             swiz == GEN_SWIZZLE_WWWW);
  }

  uint32_t brw_swap_cmod(uint32_t cmod);

} /* namespace gbe */

#endif /* GEN_EU_H */

