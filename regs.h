#include "swap.h"

/* this number includes the 2 system register sets plus n "user"
   register sets.  Some Prime models had more than 2 system register
   sets, but the emulator only has 2 */

#define REGSETS 10

#define T_REGS  0x00100000

/* these are 16-bit absolute offsets into the register file */

#define PSWKEYS16 031*2
#define PLA16     032*2
#define PCBA16    032*2+1
#define PLB16     033*2
#define PCBB16    033*2+1
#define REGDMX16  040*2

/* these are 32-bit absolute offsets into the register file */

#define DSWPB32   036
#define DSWSTAT32 035
#define DSWRMA32  034
#define PSWPB32   030

/* these are 16-bit offsets into crs (current register set) */

#define A 4
#define B 5
#define L 4
#define E 6

#define S 10
#define Y 10
#define YH 10
#define YL 11

#define X 14
#define XH 14
#define XL 15

#define FLTH 20
#define FLTL 21
#define FLTD 22
#define FEXP 23
#define VSC 23

#define PB 24
#define PBH 24
#define PBL 25

#define SB 26
#define SBH 26
#define SBL 27

#define LB 28
#define LBH 28
#define LBL 29

#define XB 30
#define XBH 30
#define XBL 31

#define DTAR3 32
#define DTAR2 34
#define DTAR1 36
#define DTAR0 38

#define KEYS 40
#define MODALS 41

#define OWNER 42
#define OWNERH 42
#define OWNERL 43
#define FCODE 44
#define FADDR 46
#define TIMER 48
#define TIMERH 48
#define TIMERL 49

/* I-mode offsets for 16-bit access to registers, eg, crs[GR0H] */

#define GR0H 0
#define GR1H 2
#define GR2H 4
#define GR3H 6
#define GR4H 8
#define GR5H 10
#define GR6H 12
#define GR7H 14

/* these are 32-bit offsets into crsl (current register set long) */

#define GR0 0
#define GR1 1
#define GR2 2
#define GR3 3
#define GR4 4
#define GR5 5
#define GR6 6
#define GR7 7
#define FAR0 8
#define FLR0 9
#define FAR1 10
#define FLR1 11
#define FAC0 8
#define FAC1 10
#define BR 12
#define OWNER32 (OWNERH/2)
#define TIMER32 (TIMERH/2)

/* table of CPU names & types */

static struct {
  short cputype;
  short cpumodel;
} cputab[45] = { \
  {1, 400},
  {3, 350},
  {4, 450},
  {5, 750},
  {7, 150},
  {7, 250},
  {8, 850},
  {9, 550},
  {10, 650},
  {11, 2250},
  {15, 9950},
  {16, 9650},
  {17, 2550},
  {19, 9750},
  {21, 2350},
  {22, 2655},
  {23, 9655},
  {24, 9955},
  {25, 2450},
  {26, 4050},
  {27, 4150},
  {28, 6350},
  {29, 6550},
  {31, 2755},
  {32, 2455},
  {33, 5310},
  {34, 9755},
  {35, 2850},
  {36, 2950},
  {37, 5330},
  {38, 4450},
  {39, 5370},
  {40, 6650},
  {41, 6450},
  {42, 6150},
  {43, 5320},
  {44, 5340},
#if 0
  {45, 5510),
  {46, 5520),
  {47, 5530),
  {48, 5540),
  {49, 5550),
  {50, 5560),
  {51, 5570),
  {52, 5580),
  {0, 0},
#endif
};

/* this is the number of user register sets for this cpuid */

static short regsets[] = { \
			   2,  /* 00  P400 */
			   2,  /* 01  P400 (> REV A U-CODE) */
			   2,  /* 02  RESERVED */
			   2,  /* 03  P350 */
			   2,  /* 04  P450/P550 */
			   2,  /* 05  P750 */
			   2,  /* 06  P650 */
			   2,  /* 07  P150/P250 */
			   2,  /* 08  P850 */
			   2,  /* 09  MOLE/550 */
			   2,  /* 10  MOLE/650 */
			   2,  /* 11  P2250 */
			   2,  /* 12  P750Y */
			   2,  /* 13  P550Y */
			   2,  /* 14  P850Y */
			   4,  /* 15  P9950 */
			   8,  /* 16  P9650 */
			   8,  /* 17  P2550 */
			   4,  /* 18  P9955 */
			   4,  /* 19  P9750 */
			   2,  /* 20  TBD */
			   8,  /* 21  P2350 */
			   8,  /* 22  P2655 */
			   8,  /* 23  P9655 */
			   4,  /* 24  P9955-TIGGER */
			   8,  /* 25  P2450 */
			   4,  /* 26  P4050 */
			   4,  /* 27  P4150 */
			   4,  /* 28  P6350 */
			   4,  /* 29  P6550 */
			   4,  /* 30  P9955-II */
			   8,  /* 31  P2755 */
			   8,  /* 32  P2455 */
			   4,  /* 33  P5310 */
			   4,  /* 34  P9755 */
			   4,  /* 35  P2850 */
			   4,  /* 36  P2950 */
			   4,  /* 37  P5330 */
			   4,  /* 38  P4450 */
			   4,  /* 39  P5370 */
			   4,  /* 40  P6650 */
			   4,  /* 41  P6450 */
			   4,  /* 42  P6150 */
			   4,  /* 43  P5320 */
			   4}; /* 44  P5340 */

static union {
    int rs[REGSETS][32];

    unsigned short rs16[REGSETS][64];

    /* locs '0-'177 as signed 32-bit integers */
    int s32[32*REGSETS];

    /* locs '0-'177 as unsigned 32-bit integers */
    unsigned int u32[32*REGSETS];

    /* locs '0-'377 as signed 16-bit integers */
    short s16[64*REGSETS];

    /* locs '0-'377 as signed 16-bit integers */
    unsigned short u16[64*REGSETS];

    /* symbolic register file locations */
    struct {
      unsigned int tr0,tr1,tr2,tr3,tr4,tr5,tr6,tr7;     /*  '0-7  */
      unsigned int rdmx1,rdmx2;                         /* '10-11 */
      unsigned short rdum1[1],ratmpl;                   /* '12    */
      unsigned int rsgt1,rsgt2,recc1,recc2;             /* '13-16 */
      unsigned short rdum2[1],reoiv,zero,one;           /* '17-20 */
      unsigned int pbsave,rdmx3,rdmx4,c377,rdum3[3];    /* '21-27 */
      unsigned int pswpb;                               /* '30    */
      unsigned short pswkeys,rdum4[1];                  /* '31    */
      unsigned short pla,pcba,plb,pcbb;                 /* '32-33 */
      unsigned int dswrma;                              /* '34    */
      unsigned int dswstat;                             /* '35    */
      unsigned int dswpb,rsavptr;                       /* '36-37 */
      unsigned short regdmx[64];                        /* '40-77 */
      unsigned int userregs[REGSETS-2][32];             /* '100-  */
    } sym;
  } regs;

union {
  struct {
#ifdef __BIG_ENDIAN__
    unsigned short rph;
    unsigned short rpl;
#else
    unsigned short rpl;
    unsigned short rph;
#endif
  } s;
  unsigned int ul;
} rpreg;

#define grp RP              /* turns grp assignments into dummies */
#define gcrsl crsl          /* turns gcrsl assignments into dummies */

static union {
  short *i16;
  unsigned short *u16;
  int *i32;
  unsigned int *u32;
  long long *i64;
  unsigned long long *u64;
} cr;

#define crs  cr.u16
#define crsl cr.u32

#define RP rpreg.ul
#define RPH rpreg.s.rph
#define RPL rpreg.s.rpl

/************  16-bit offset macros:  *************/

/* fetch 16-bit unsigned at 16-bit offset */
//#define getcrs16(offset) crs[(offset)]
static inline uint16_t getcrs16(int offset) {  \
  return swap16(crs[offset]);  \
}

/* store 16-bit unsigned at 16-bit offset */
//#define putcrs16(offset, val) crs[(offset)] = (val)
static inline void putcrs16(int offset, uint16_t val) {  \
  TRACE(T_REGS, "  putcrs16: offset %d val %06o\n", offset, val);
  crs[(offset)] = swap16(val); \
}

/* get 16-bit signed at 16-bit offset */
//#define getcrs16s(offset) *(short *)(crs+(offset))
#define getcrs16s(offset) (int16_t) getcrs16((offset))

/* get 32-bit unsigned at 16-bit offset */
//#define getcrs32(offset) *(unsigned int *)(crs+offset)
static inline uint32_t getcrs32(int offset) {  \
  return swap32(*(unsigned int *)(crs+offset));  \
}

/* get 32-bit signed at 16-bit offset */
//#define getcrs32s(offset) *(int *)(crs+(offset))
#define getcrs32s(offset) (int32_t) getcrs32((offset))

/* put 32-bit unsigned at 16-bit offset */
//#define putcrs32(offset, val) *(unsigned int *)(crs+(offset)) = (val)
static inline void putcrs32(int offset, uint32_t val) {  \
  TRACE(T_REGS, "  putcrs32: offset %d val %012lo\n", offset, val);
  *(unsigned int *)(crs+offset) = swap32(val);  \
}

/* put 32-bit signed at 16-bit offset */
//#define putcrs32s(offset, val) *(int *)(crs+(offset)) = (val)
#define putcrs32s(offset, val) putcrs32((offset), (val))

/* get 32-bit effective address at 16-bit offset */
//#define getcrs32ea(offset) *(ea_t *)(crs+(offset))
#define  getcrs32ea(offset) getcrs32(offset)

/* put 32-bit effective address at 16-bit offset */
//#define putcrs32ea(offset, val) *(ea_t *)(crs+(offset)) = (val)
#define putcrs32ea(offset, val) putcrs32((offset), (val))

/* get 64-bit signed at 16-bit offset */
//#define getcrs64s(offset) *(long long *)(crs+(offset))
static inline int64_t getcrs64s(int offset) {  \
  return (long long)swap64(*(long long *)(crs+(offset)));	\
}

/* put 64-bit signed at 16-bit offset */
//#define putcrs64s(offset, val) *(long long *)(crs+(offset)) = (val)
static inline void putcrs64s(int offset, int64_t val) {  \
  *(long long *)(crs+offset) = swap64(val);  \
}

/* put 64-bit double at 16-bit offset (remove later) */
//#define putcrs64d(offset, val) *(double *)(crs+(offset)) = (val)
static inline void putcrs64d(int offset, double val) {  \
  *(unsigned long long *)(crs+offset) = swap64(*(uint64_t *)&val);	  \
}

/* get 16-bit unsigned at 16-bit absolute register file address */
static inline uint16_t getar16(int offset) {  \
  return swap16(regs.u16[offset]);  \
}

/* put 16-bit unsigned at 16-bit absolute register file address */
static inline void putar16(int offset, uint16_t val) {  \
  TRACE(T_REGS, "  putar16: offset %d val %06o\n", offset, val);
  regs.u16[(offset)] = swap16(val); \
}

/******* 32-bit offset macros: ***********/

/* fetch 16-bit unsigned at 32-bit offset (left halfword is returned) */
//#define getgr16(offset) crs[(offset)*2]
static inline uint16_t getgr16(int offset) {  \
  return swap16(crs[offset*2]); \
}

/* store 16-bit unsigned at 32-bit offset (in left halfword) */
//#define putgr16(offset, val) crs[(offset)*2] = (val)
static inline void putgr16(int offset, uint16_t val) {  \
  crs[(offset)*2] = swap16(val); \
}

/* fetch 16-bit signed at 32-bit offset (right halfword is returned) */
//#define getgr16s(offset) *(short *)(crs+(offset)*2)
#define getgr16s(offset) (int16_t) getgr16((offset))

/* store 16-bit signed at 32-bit offset (in left halfword) */
//#define putgr16s(offset, val) *(short *)(crs+(offset)*2) = (val)
#define putgr16s(offset, val) putgr16((offset), (val))

/* fetch 32-bit unsigned at 32-bit offset */
//#define getgr32(offset) crsl[(offset)]
static inline uint32_t getgr32(int offset) {  \
  return swap32(crsl[offset]);
}

/* store 32-bit unsigned at 32-bit offset */
//#define putgr32(offset, val) crsl[(offset)] = (val)
static inline void putgr32(int offset, uint32_t val) {  \
  crsl[offset] = swap32(val); \
}

/* fetch 32-bit signed at 32-bit offset */
//#define getgr32s(offset) *(int *)(crsl+(offset))
#define getgr32s(offset) (int32_t) getgr32((offset))

/* store 32-bit signed at 32-bit offset */
//#define putgr32s(offset, val) *(int *)(crsl+(offset)) = (val)
#define putgr32s(offset, val) putgr32((offset), (val))

/* fetch 64-bit signed at 32-bit offset */
//#define getgr64s(offset) *(long long *)(crsl+(offset))
static inline int64_t getgr64s(int offset) {  \
  return (int64_t) swap64(*(long long *)(crsl+offset));
}

/* store 64-bit signed at 32-bit offset */
//#define putgr64s(offset, val) *(long long *)(crsl+(offset)) = (val)
static inline void putgr64s(int offset, int64_t val) {  \
  *(long long *)(crsl+offset) = swap64(val); \
}

/* fetch 64-bit unsigned at 32-bit offset */
//#define getgr64(offset) *(unsigned long long *)(crsl+(offset))
#define getgr64(offset) getgr64s(offset)

/* get 32-bit unsigned at 32-bit absolute register file address */
static inline uint32_t getar32(int offset) {  \
  return swap32(regs.u32[offset]);  \
}

/* put 32-bit unsigned at 32-bit absolute register file address */
static inline void putar32(int offset, uint32_t val) {  \
  regs.u32[(offset)] = swap32(val); \
}

/* fetch 32-bit unsigned at FP register 0 or 1
   For FP 0, offset=0; for FP 1, offset=2
   NOTE: instead of doing FAC0+offset, there could be another
   pointer to FR0, then use offset as an index */
#define getfr32(offset) getgr32(FAC0+offset)

/* fetch 64-bit unsigned at FP register 0 or 1
   For FP 0, offset=0; for FP 1, offset=2 */
static inline uint64_t getfr64(int offset) {  \
  return (uint64_t) swap64(*(unsigned long long *)(crsl+FAC0+offset));
}

/* put 64-bit double in FP reg 0 or 1
   For FP 0, offset=0; for FP 1, offset=2 */
//#define putfr64(offset, val) 
static inline void putfr64(int offset, unsigned long long val) {  \
  *(unsigned long long *)(crsl+FAC0+offset) = swap64((val));
}

/* put 64-bit double in FP reg 0 or 1
   For FP 0, offset=0; for FP 1, offset=2 */
//#define putfr64d(offset, val) *(double *)(crsl+FAC0+offset) = (val)
static inline void putfr64d(int offset, double val) {  \
  *(unsigned long long *)(crsl+FAC0+offset) = swap64(*(uint64_t *)&val);	  \
}

#define PCBLEV 0
#define PCBLINK 1
#define PCBWAIT 2
#define PCBABT 4
#define PCBCPU 5
#define PCBPET 8
#define PCBDTAR2 10
#define PCBDTAR3 12
#define PCBIT 14
#define PCBMASK 16
#define PCBKEYS 17
#define PCBREGS 18
#define PCBFVEC 50
#define PCBFVR0 50
#define PCBFVR1 52
#define PCBFVR2 54
#define PCBFVR3 56
#define PCBFVPF 58
#define PCBCSFIRST 60
#define PCBCSNEXT 61
#define PCBCSLAST 62

/* define mapping between memory addresses and the current register set */

static unsigned short memtocrs[] = {
  X,      /* 0 = X */
  A,      /* 1 = A */
  B,      /* 2 = B */
  Y,      /* 3 = Y */
  FLTH,   /* 4 = FAC1/FLTH */
  FLTL,   /* 5 = FAC1/FLTL */
  FEXP,   /* 6 = FAC1/FEXP */
  -1,     /* 7 = PC (this is in the microcode scratch register set - TR7) */
  32,     /* 10 = unnamed */
  FCODE,  /* 11 = FCODE */
  FADDR+1,/* 12 = FADDR (word) */
  16,     /* 13 = unnamed */
  SBH,    /* 14 = unnamed (SB seg) */
  SBL,    /* 15 = unnamed (SB word) */
  LBH,    /* 16 = unnamed (LB seg) */
  LBL};    /* 17 = unnamed (LB word) */

