/* -----------------------------------------------------------------------------
 *
 * (c) The University of Glasgow 2004
 *
 * This file is included at the top of all .cmm source files (and
 * *only* .cmm files).  It defines a collection of useful macros for
 * making .cmm code a bit less error-prone to write, and a bit easier
 * on the eye for the reader.
 *
 * For the syntax of .cmm files, see the parser in ghc/compiler/cmm/CmmParse.y.
 *
 * If you're used to the old HC file syntax, here's a quick cheat sheet
 * for converting HC code:
 *
 *       - Remove FB_/FE_
 *       - Remove all type casts
 *       - Remove '&'
 *       - STGFUN(foo) { ... }  ==>  foo { ... }
 *       - FN_(foo) { ... }  ==>  foo { ... }
 *       - JMP_(e)  ==> jump e;
 *       - Remove EXTFUN(foo)
 *       - Sp[n]  ==>  Sp(n)
 *       - Hp[n]  ==>  Hp(n)
 *       - Sp += n  ==> Sp_adj(n)
 *       - Hp += n  ==> Hp_adj(n)
 *       - R1.i   ==>  R1   (similarly for R1.w, R1.cl etc.)
 *       - You need to explicitly dereference variables; eg. 
 *             context_switch   ==>  CInt[context_switch]
 *       - convert all word offsets into byte offsets:
 *         	- e ==> WDS(e)
 *       - sizeofW(StgFoo)  ==>  SIZEOF_StgFoo
 *       - ENTRY_CODE(e)  ==>  %ENTRY_CODE(e)
 *       - get_itbl(c)  ==>  %GET_STD_INFO(c)
 *       - Change liveness masks in STK_CHK_GEN, HP_CHK_GEN:
 *        	R1_PTR | R2_PTR  ==>  R1_PTR & R2_PTR
 *         	(NOTE: | becomes &)
 *       - Declarations like 'StgPtr p;' become just 'W_ p;'
 *       - e->payload[n] ==> PAYLOAD(e,n)
 *       - Be very careful with comparisons: the infix versions (>, >=, etc.)
 *         are unsigned, so use %lt(a,b) to get signed less-than for example.
 *
 * Accessing fields of structures defined in the RTS header files is
 * done via automatically-generated macros in DerivedConstants.h.  For
 * example, where previously we used
 *
 *          CurrentTSO->what_next = x
 *
 * in C-- we now use
 *
 *          StgTSO_what_next(CurrentTSO) = x
 *
 * where the StgTSO_what_next() macro is automatically generated by
 * mkDerivedConstnants.c.  If you need to access a field that doesn't
 * already have a macro, edit that file (it's pretty self-explanatory).
 *
 * -------------------------------------------------------------------------- */

#ifndef CMM_H
#define CMM_H

/*
 * In files that are included into both C and C-- (and perhaps
 * Haskell) sources, we sometimes need to conditionally compile bits
 * depending on the language.  CMINUSMINUS==1 in .cmm sources:
 */
#define CMINUSMINUS 1

#include "ghcconfig.h"
#include "RtsConfig.h"

/* -----------------------------------------------------------------------------
   Types 

   The following synonyms for C-- types are declared here:

     I8, I16, I32, I64    MachRep-style names for convenience

     W_                   is shorthand for the word type (== StgWord)
     F_		 	  shorthand for float  (F_ == StgFloat == C's float)
     D_	 		  shorthand for double (D_ == StgDouble == C's double)

     CInt		  has the same size as an int in C on this platform
     CLong		  has the same size as a long in C on this platform
   
  --------------------------------------------------------------------------- */

#define I8  bits8
#define I16 bits16
#define I32 bits32
#define I64 bits64

#if SIZEOF_VOID_P == 4
#define W_ bits32
/* Maybe it's better to include MachDeps.h */
#define TAG_BITS                2
#elif SIZEOF_VOID_P == 8
#define W_ bits64
/* Maybe it's better to include MachDeps.h */
#define TAG_BITS                3
#else
#error Unknown word size
#endif

/*
 * The RTS must UNTAG a pointer before dereferencing it.
 * The use of UNTAG follows the following rules of thumb:
 *
 * - Any pointer might be tagged.
 * - Except the pointers that are passed in R1 to RTS functions.
 * - R1 is also untagged when entering constructor code.
 * 
 * TODO:
 *
 * - Remove redundancies of tagging and untagging in code generation.
 * - Optimize getTag or dataToTag# ?
 *
 */
#define TAG_MASK ((1 << TAG_BITS) - 1)
#define UNTAG(p) (p & ~TAG_MASK)
#define GETTAG(p) (p & TAG_MASK)

#if SIZEOF_INT == 4
#define CInt bits32
#elif SIZEOF_INT == 8
#define CInt bits64
#else
#error Unknown int size
#endif

#if SIZEOF_LONG == 4
#define CLong bits32
#elif SIZEOF_LONG == 8
#define CLong bits64
#else
#error Unknown long size
#endif

#define F_ float32
#define D_ float64
#define L_ bits64

#define SIZEOF_StgDouble 8
#define SIZEOF_StgWord64 8

/* -----------------------------------------------------------------------------
   Misc useful stuff
   -------------------------------------------------------------------------- */

#define NULL (0::W_)

#define STRING(name,str)			\
  section "rodata" {				\
	name : bits8[] str;			\
  }						\

/* -----------------------------------------------------------------------------
   Byte/word macros

   Everything in C-- is in byte offsets (well, most things).  We use
   some macros to allow us to express offsets in words and to try to
   avoid byte/word confusion.
   -------------------------------------------------------------------------- */

#define SIZEOF_W  SIZEOF_VOID_P
#define W_MASK    (SIZEOF_W-1)

#if SIZEOF_W == 4
#define W_SHIFT 2
#elif SIZEOF_W == 8
#define W_SHIFT 4
#endif

/* Converting quantities of words to bytes */
#define WDS(n) ((n)*SIZEOF_W)

/*
 * Converting quantities of bytes to words
 * NB. these work on *unsigned* values only
 */
#define BYTES_TO_WDS(n) ((n) / SIZEOF_W)
#define ROUNDUP_BYTES_TO_WDS(n) (((n) + SIZEOF_W - 1) / SIZEOF_W)

/* TO_W_(n) converts n to W_ type from a smaller type */
#if SIZEOF_W == 4
#define TO_W_(x) %sx32(x)
#define HALF_W_(x) %lobits16(x)
#elif SIZEOF_W == 8
#define TO_W_(x) %sx64(x)
#define HALF_W_(x) %lobits32(x)
#endif

#if SIZEOF_INT == 4 && SIZEOF_W == 8
#define W_TO_INT(x) %lobits32(x)
#elif SIZEOF_INT == SIZEOF_W
#define W_TO_INT(x) (x)
#endif

/* -----------------------------------------------------------------------------
   Heap/stack access, and adjusting the heap/stack pointers.
   -------------------------------------------------------------------------- */

#define Sp(n)  W_[Sp + WDS(n)]
#define Hp(n)  W_[Hp + WDS(n)]

#define Sp_adj(n) Sp = Sp + WDS(n)
#define Hp_adj(n) Hp = Hp + WDS(n)

/* -----------------------------------------------------------------------------
   Assertions and Debuggery
   -------------------------------------------------------------------------- */

#ifdef DEBUG
#define ASSERT(predicate)			\
	if (predicate) {			\
	    /*null*/;				\
	} else {				\
	    foreign "C" _assertFail(NULL, __LINE__); \
        }
#else
#define ASSERT(p) /* nothing */
#endif

#ifdef DEBUG
#define DEBUG_ONLY(s) s
#else
#define DEBUG_ONLY(s) /* nothing */
#endif

/*
 * The IF_DEBUG macro is useful for debug messages that depend on one
 * of the RTS debug options.  For example:
 * 
 *   IF_DEBUG(RtsFlags_DebugFlags_apply,
 *      foreign "C" fprintf(stderr, stg_ap_0_ret_str));
 *
 * Note the syntax is slightly different to the C version of this macro.
 */
#ifdef DEBUG
#define IF_DEBUG(c,s)  if (RtsFlags_DebugFlags_##c(RtsFlags) != 0::I32) { s; }
#else
#define IF_DEBUG(c,s)  /* nothing */
#endif

/* -----------------------------------------------------------------------------
   Entering 

   It isn't safe to "enter" every closure.  Functions in particular
   have no entry code as such; their entry point contains the code to
   apply the function.

   ToDo: range should end in N_CLOSURE_TYPES-1, not N_CLOSURE_TYPES,
   but switch doesn't allow us to use exprs there yet.

   If R1 points to a tagged object it points either to
   * A constructor.
   * A function with arity <= TAG_MASK.
   In both cases the right thing to do is to return.
   Note: it is rather lucky that we can use the tag bits to do this
         for both objects. Maybe it points to a brittle design?

   Indirections can contain tagged pointers, so their tag is checked.
   -------------------------------------------------------------------------- */

#define ENTER()						\
 again:							\
  W_ info;						\
  if (GETTAG(R1) != 0) {                                \
      jump %ENTRY_CODE(Sp(0));				\
  }                                                     \
  info = %INFO_PTR(R1);					\
  switch [INVALID_OBJECT .. N_CLOSURE_TYPES]		\
         (TO_W_( %INFO_TYPE(%STD_INFO(info)) )) {	\
  case							\
    IND,						\
    IND_OLDGEN,						\
    IND_PERM,						\
    IND_OLDGEN_PERM,					\
    IND_STATIC:						\
   {							\
      R1 = StgInd_indirectee(R1);			\
      goto again;					\
   }							\
  case							\
    FUN,						\
    FUN_1_0,						\
    FUN_0_1,						\
    FUN_2_0,						\
    FUN_1_1,						\
    FUN_STATIC,                                         \
    BCO,						\
    PAP:						\
   {							\
      jump %ENTRY_CODE(Sp(0));				\
   }							\
  default:						\
   {							\
      jump %ENTRY_CODE(info);				\
   }							\
  }

// The FUN cases almost never happen: a pointer to a non-static FUN
// should always be tagged.  This unfortunately isn't true for the
// interpreter right now, which leaves untagged FUNs on the stack.

/* -----------------------------------------------------------------------------
   Constants.
   -------------------------------------------------------------------------- */

#include "Constants.h"
#include "DerivedConstants.h"
#include "ClosureTypes.h"
#include "StgFun.h"
#include "OSThreads.h"
#include "SMP.h"

/*
 * Need MachRegs, because some of the RTS code is conditionally
 * compiled based on REG_R1, REG_R2, etc.
 */
#define STOLEN_X86_REGS 4
#include "MachRegs.h"

#include "Liveness.h"
#include "StgLdvProf.h"

#undef BLOCK_SIZE
#undef MBLOCK_SIZE
#include "Block.h"  /* For Bdescr() */


/* Can't think of a better place to put this. */
#if SIZEOF_mp_limb_t != SIZEOF_VOID_P
#error mp_limb_t != StgWord: assumptions in PrimOps.cmm are now false
#endif

#define MyCapability()  (BaseReg - OFFSET_Capability_r)

/* -------------------------------------------------------------------------
   Allocation and garbage collection
   ------------------------------------------------------------------------- */

/*
 * ALLOC_PRIM is for allocating memory on the heap for a primitive
 * object.  It is used all over PrimOps.cmm.
 *
 * We make the simplifying assumption that the "admin" part of a
 * primitive closure is just the header when calculating sizes for
 * ticky-ticky.  It's not clear whether eg. the size field of an array
 * should be counted as "admin", or the various fields of a BCO.
 */
#define ALLOC_PRIM(bytes,liveness,reentry)			\
   HP_CHK_GEN_TICKY(bytes,liveness,reentry);			\
   TICK_ALLOC_PRIM(SIZEOF_StgHeader,bytes-SIZEOF_StgHeader,0);	\
   CCCS_ALLOC(bytes);

/* CCS_ALLOC wants the size in words, because ccs->mem_alloc is in words */
#define CCCS_ALLOC(__alloc) CCS_ALLOC(BYTES_TO_WDS(__alloc), W_[CCCS])

#define HP_CHK_GEN_TICKY(alloc,liveness,reentry)	\
   HP_CHK_GEN(alloc,liveness,reentry);			\
   TICK_ALLOC_HEAP_NOCTR(alloc);

// allocateLocal() allocates from the nursery, so we check to see
// whether the nursery is nearly empty in any function that uses
// allocateLocal() - this includes many of the primops.
#define MAYBE_GC(liveness,reentry)			\
  if (bdescr_link(CurrentNursery) == NULL || CInt[alloc_blocks] >= CInt[alloc_blocks_lim]) {		\
	R9  = liveness;					\
        R10 = reentry;					\
        HpAlloc = 0;					\
        jump stg_gc_gen_hp;				\
   }

/* -----------------------------------------------------------------------------
   Closure headers
   -------------------------------------------------------------------------- */

/*
 * This is really ugly, since we don't do the rest of StgHeader this
 * way.  The problem is that values from DerivedConstants.h cannot be 
 * dependent on the way (SMP, PROF etc.).  For SIZEOF_StgHeader we get
 * the value from GHC, but it seems like too much trouble to do that
 * for StgThunkHeader.
 */
#define SIZEOF_StgThunkHeader SIZEOF_StgHeader+SIZEOF_StgSMPThunkHeader

#define StgThunk_payload(__ptr__,__ix__) \
    W_[__ptr__+SIZEOF_StgThunkHeader+ WDS(__ix__)]

/* -----------------------------------------------------------------------------
   Closures
   -------------------------------------------------------------------------- */

/* The offset of the payload of an array */
#define BYTE_ARR_CTS(arr)  ((arr) + SIZEOF_StgArrWords)

/* Getting/setting the info pointer of a closure */
#define SET_INFO(p,info) StgHeader_info(p) = info
#define GET_INFO(p) StgHeader_info(p)

/* Determine the size of an ordinary closure from its info table */
#define sizeW_fromITBL(itbl) \
  SIZEOF_StgHeader + WDS(%INFO_PTRS(itbl)) + WDS(%INFO_NPTRS(itbl))

/* NB. duplicated from InfoTables.h! */
#define BITMAP_SIZE(bitmap) ((bitmap) & BITMAP_SIZE_MASK)
#define BITMAP_BITS(bitmap) ((bitmap) >> BITMAP_BITS_SHIFT)

/* Debugging macros */
#define LOOKS_LIKE_INFO_PTR(p)				\
   ((p) != NULL && 					\
     (TO_W_(%INFO_TYPE(%STD_INFO(p))) != INVALID_OBJECT) &&	\
     (TO_W_(%INFO_TYPE(%STD_INFO(p))) <  N_CLOSURE_TYPES))

#define LOOKS_LIKE_CLOSURE_PTR(p) (LOOKS_LIKE_INFO_PTR(GET_INFO(UNTAG(p))))

/*
 * The layout of the StgFunInfoExtra part of an info table changes
 * depending on TABLES_NEXT_TO_CODE.  So we define field access
 * macros which use the appropriate version here:
 */
#ifdef TABLES_NEXT_TO_CODE
/*
 * when TABLES_NEXT_TO_CODE, slow_apply is stored as an offset
 * instead of the normal pointer.
 */
        
#define StgFunInfoExtra_slow_apply(fun_info)    \
        (TO_W_(StgFunInfoExtraRev_slow_apply_offset(fun_info))    \
               + (fun_info) + SIZEOF_StgFunInfoExtraRev + SIZEOF_StgInfoTable)

#define StgFunInfoExtra_fun_type(i)   StgFunInfoExtraRev_fun_type(i)
#define StgFunInfoExtra_arity(i)      StgFunInfoExtraRev_arity(i)
#define StgFunInfoExtra_bitmap(i)     StgFunInfoExtraRev_bitmap(i)
#else
#define StgFunInfoExtra_slow_apply(i) StgFunInfoExtraFwd_slow_apply(i)
#define StgFunInfoExtra_fun_type(i)   StgFunInfoExtraFwd_fun_type(i)
#define StgFunInfoExtra_arity(i)      StgFunInfoExtraFwd_arity(i)
#define StgFunInfoExtra_bitmap(i)     StgFunInfoExtraFwd_bitmap(i)
#endif

/* -----------------------------------------------------------------------------
   Voluntary Yields/Blocks

   We only have a generic version of this at the moment - if it turns
   out to be slowing us down we can make specialised ones.
   -------------------------------------------------------------------------- */

#define YIELD(liveness,reentry)			\
   R9  = liveness;				\
   R10 = reentry;				\
   jump stg_gen_yield;

#define BLOCK(liveness,reentry)			\
   R9  = liveness;				\
   R10 = reentry;				\
   jump stg_gen_block;

/* -----------------------------------------------------------------------------
   Ticky macros 
   -------------------------------------------------------------------------- */

#ifdef TICKY_TICKY
#define TICK_BUMP_BY(ctr,n) CLong[ctr] = CLong[ctr] + n
#else
#define TICK_BUMP_BY(ctr,n) /* nothing */
#endif

#define TICK_BUMP(ctr)      TICK_BUMP_BY(ctr,1)

#define TICK_ENT_DYN_IND()  		TICK_BUMP(ENT_DYN_IND_ctr)
#define TICK_ENT_DYN_THK()  		TICK_BUMP(ENT_DYN_THK_ctr)
#define TICK_ENT_VIA_NODE()  		TICK_BUMP(ENT_VIA_NODE_ctr)
#define TICK_ENT_STATIC_IND()  		TICK_BUMP(ENT_STATIC_IND_ctr)
#define TICK_ENT_PERM_IND()  		TICK_BUMP(ENT_PERM_IND_ctr)
#define TICK_ENT_PAP()  		TICK_BUMP(ENT_PAP_ctr)
#define TICK_ENT_AP()  			TICK_BUMP(ENT_AP_ctr)
#define TICK_ENT_AP_STACK()  		TICK_BUMP(ENT_AP_STACK_ctr)
#define TICK_ENT_BH()  			TICK_BUMP(ENT_BH_ctr)
#define TICK_UNKNOWN_CALL()  		TICK_BUMP(UNKNOWN_CALL_ctr)
#define TICK_UPDF_PUSHED()  		TICK_BUMP(UPDF_PUSHED_ctr)
#define TICK_CATCHF_PUSHED()  		TICK_BUMP(CATCHF_PUSHED_ctr)
#define TICK_UPDF_OMITTED()  		TICK_BUMP(UPDF_OMITTED_ctr)
#define TICK_UPD_NEW_IND()  		TICK_BUMP(UPD_NEW_IND_ctr)
#define TICK_UPD_NEW_PERM_IND()  	TICK_BUMP(UPD_NEW_PERM_IND_ctr)
#define TICK_UPD_OLD_IND()  		TICK_BUMP(UPD_OLD_IND_ctr)
#define TICK_UPD_OLD_PERM_IND()  	TICK_BUMP(UPD_OLD_PERM_IND_ctr)
  
#define TICK_SLOW_CALL_FUN_TOO_FEW()	TICK_BUMP(SLOW_CALL_FUN_TOO_FEW_ctr)
#define TICK_SLOW_CALL_FUN_CORRECT()	TICK_BUMP(SLOW_CALL_FUN_CORRECT_ctr)
#define TICK_SLOW_CALL_FUN_TOO_MANY()	TICK_BUMP(SLOW_CALL_FUN_TOO_MANY_ctr)
#define TICK_SLOW_CALL_PAP_TOO_FEW()	TICK_BUMP(SLOW_CALL_PAP_TOO_FEW_ctr)
#define TICK_SLOW_CALL_PAP_CORRECT()	TICK_BUMP(SLOW_CALL_PAP_CORRECT_ctr)
#define TICK_SLOW_CALL_PAP_TOO_MANY()	TICK_BUMP(SLOW_CALL_PAP_TOO_MANY_ctr)

#define TICK_SLOW_CALL_v()  		TICK_BUMP(SLOW_CALL_v_ctr)
#define TICK_SLOW_CALL_p()  		TICK_BUMP(SLOW_CALL_p_ctr)
#define TICK_SLOW_CALL_pv()  		TICK_BUMP(SLOW_CALL_pv_ctr)
#define TICK_SLOW_CALL_pp()  		TICK_BUMP(SLOW_CALL_pp_ctr)
#define TICK_SLOW_CALL_ppp()  		TICK_BUMP(SLOW_CALL_ppp_ctr)
#define TICK_SLOW_CALL_pppp()  		TICK_BUMP(SLOW_CALL_pppp_ctr)
#define TICK_SLOW_CALL_ppppp()  	TICK_BUMP(SLOW_CALL_ppppp_ctr)
#define TICK_SLOW_CALL_pppppp()  	TICK_BUMP(SLOW_CALL_pppppp_ctr)

/* NOTE: TICK_HISTO_BY and TICK_HISTO 
   currently have no effect.
   The old code for it didn't typecheck and I 
   just commented it out to get ticky to work.
   - krc 1/2007 */

#define TICK_HISTO_BY(histo,n,i) /* nothing */

#define TICK_HISTO(histo,n) TICK_HISTO_BY(histo,n,1)

/* An unboxed tuple with n components. */
#define TICK_RET_UNBOXED_TUP(n)			\
  TICK_BUMP(RET_UNBOXED_TUP_ctr++);		\
  TICK_HISTO(RET_UNBOXED_TUP,n)

/*
 * A slow call with n arguments.  In the unevald case, this call has
 * already been counted once, so don't count it again.
 */
#define TICK_SLOW_CALL(n)			\
  TICK_BUMP(SLOW_CALL_ctr);			\
  TICK_HISTO(SLOW_CALL,n)

/*
 * This slow call was found to be to an unevaluated function; undo the
 * ticks we did in TICK_SLOW_CALL.
 */
#define TICK_SLOW_CALL_UNEVALD(n)		\
  TICK_BUMP(SLOW_CALL_UNEVALD_ctr);		\
  TICK_BUMP_BY(SLOW_CALL_ctr,-1);		\
  TICK_HISTO_BY(SLOW_CALL,n,-1);

/* Updating a closure with a new CON */
#define TICK_UPD_CON_IN_NEW(n)			\
  TICK_BUMP(UPD_CON_IN_NEW_ctr);		\
  TICK_HISTO(UPD_CON_IN_NEW,n)

#define TICK_ALLOC_HEAP_NOCTR(n)		\
    TICK_BUMP(ALLOC_HEAP_ctr);			\
    TICK_BUMP_BY(ALLOC_HEAP_tot,n)

/* -----------------------------------------------------------------------------
   Misc junk
   -------------------------------------------------------------------------- */

#define NO_TREC                   stg_NO_TREC_closure
#define END_TSO_QUEUE             stg_END_TSO_QUEUE_closure
#define END_INVARIANT_CHECK_QUEUE stg_END_INVARIANT_CHECK_QUEUE_closure

#define dirtyTSO(tso) \
    StgTSO_flags(tso) = StgTSO_flags(tso) | TSO_DIRTY::I32;

#define recordMutableCap(p, gen, regs)					\
  W_ __bd;								\
  W_ mut_list;								\
  mut_list = Capability_mut_lists(MyCapability()) + WDS(gen);		\
 __bd = W_[mut_list];							\
  if (bdescr_free(__bd) >= bdescr_start(__bd) + BLOCK_SIZE) {		\
      W_ __new_bd;							\
      ("ptr" __new_bd) = foreign "C" allocBlock_lock() [regs];		\
      bdescr_link(__new_bd) = __bd;					\
      __bd = __new_bd;							\
      W_[mut_list] = __bd;						\
  }									\
  W_ free;								\
  free = bdescr_free(__bd);						\
  W_[free] = p;								\
  bdescr_free(__bd) = free + WDS(1);

#define recordMutable(p, regs)                                  \
      W_ __p;                                                   \
      W_ __bd;                                                  \
      W_ __gen;                                                 \
      __p = p;                                                  \
      __bd = Bdescr(__p);                                       \
      __gen = TO_W_(bdescr_gen_no(__bd));                       \
      if (__gen > 0) { recordMutableCap(__p, __gen, regs); }

#endif /* CMM_H */
