/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014, Red Hat Inc. All rights reserved.
 * Copyright (c) 2020, 2022, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef CPU_RISCV_FRAME_RISCV_INLINE_HPP
#define CPU_RISCV_FRAME_RISCV_INLINE_HPP

#include "code/codeCache.hpp"
#include "code/vmreg.inline.hpp"

// Inline functions for RISCV frames:

// Constructors:

inline frame::frame() {
  _pc = NULL;
  _sp = NULL;
  _unextended_sp = NULL;
  _fp = NULL;
  _cb = NULL;
  _deopt_state = unknown;
  _on_heap = false;
  DEBUG_ONLY(_frame_index = -1;)
}

static int spin;

inline void frame::init(intptr_t* ptr_sp, intptr_t* ptr_fp, address pc) {
  intptr_t a = intptr_t(ptr_sp);
  intptr_t b = intptr_t(ptr_fp);
  _sp = ptr_sp;
  _unextended_sp = ptr_sp;
  _fp = ptr_fp;
  _pc = pc;
  assert(pc != NULL, "no pc?");
  _cb = CodeCache::find_blob(pc);
  adjust_unextended_sp();

  address original_pc = CompiledMethod::get_deopt_original_pc(this);
  if (original_pc != NULL) {
    _pc = original_pc;
    _deopt_state = is_deoptimized;
  } else {
    _deopt_state = not_deoptimized;
  }

  _on_heap = false;
  DEBUG_ONLY(_frame_index = -1;)
}

inline frame::frame(intptr_t* ptr_sp, intptr_t* ptr_fp, address pc) {
  init(ptr_sp, ptr_fp, pc);
}

inline frame::frame(intptr_t* ptr_sp, intptr_t* unextended_sp, intptr_t* ptr_fp, address pc) {
  intptr_t a = intptr_t(ptr_sp);
  intptr_t b = intptr_t(ptr_fp);
  _sp = ptr_sp;
  _unextended_sp = unextended_sp;
  _fp = ptr_fp;
  _pc = pc;
  assert(pc != NULL, "no pc?");
  _cb = CodeCache::find_blob(pc);
  adjust_unextended_sp();

  address original_pc = CompiledMethod::get_deopt_original_pc(this);
  if (original_pc != NULL) {
    _pc = original_pc;
    assert(_cb->as_compiled_method()->insts_contains_inclusive(_pc),
           "original PC must be in the main code section of the the compiled method (or must be immediately following it)");
    _deopt_state = is_deoptimized;
  } else {
    _deopt_state = not_deoptimized;
  }

  _on_heap = false;
  DEBUG_ONLY(_frame_index = -1;)
}

inline frame::frame(intptr_t* ptr_sp) {
  Unimplemented();
}

inline frame::frame(intptr_t* ptr_sp, intptr_t* ptr_fp) {
  intptr_t a = intptr_t(ptr_sp);
  intptr_t b = intptr_t(ptr_fp);
  _sp = ptr_sp;
  _unextended_sp = ptr_sp;
  _fp = ptr_fp;
  _pc = (address)(ptr_sp[-1]);

  // Here's a sticky one. This constructor can be called via AsyncGetCallTrace
  // when last_Java_sp is non-null but the pc fetched is junk. If we are truly
  // unlucky the junk value could be to a zombied method and we'll die on the
  // find_blob call. This is also why we can have no asserts on the validity
  // of the pc we find here. AsyncGetCallTrace -> pd_get_top_frame_for_signal_handler
  // -> pd_last_frame should use a specialized version of pd_last_frame which could
  // call a specilaized frame constructor instead of this one.
  // Then we could use the assert below. However this assert is of somewhat dubious
  // value.

  _cb = CodeCache::find_blob(_pc);
  adjust_unextended_sp();

  address original_pc = CompiledMethod::get_deopt_original_pc(this);
  if (original_pc != NULL) {
    _pc = original_pc;
    _deopt_state = is_deoptimized;
  } else {
    _deopt_state = not_deoptimized;
  }

  _on_heap = false;
  DEBUG_ONLY(_frame_index = -1;)
}

// Accessors

inline bool frame::equal(frame other) const {
  bool ret =  sp() == other.sp() &&
              unextended_sp() == other.unextended_sp() &&
              fp() == other.fp() &&
              pc() == other.pc();
  assert(!ret || ret && cb() == other.cb() && _deopt_state == other._deopt_state, "inconsistent construction");
  return ret;
}

// Return unique id for this frame. The id must have a value where we can distinguish
// identity and younger/older relationship. NULL represents an invalid (incomparable)
// frame.
inline intptr_t* frame::id(void) const { return unextended_sp(); }

// Return true if the frame is older (less recent activation) than the frame represented by id
inline bool frame::is_older(intptr_t* id) const   { assert(this->id() != NULL && id != NULL, "NULL frame id");
                                                    return this->id() > id ; }

inline intptr_t* frame::link() const              { return (intptr_t*) *(intptr_t **)addr_at(link_offset); }

inline intptr_t* frame::link_or_null() const {
  intptr_t** ptr = (intptr_t **)addr_at(link_offset);
  return os::is_readable_pointer(ptr) ? *ptr : NULL;
}

inline intptr_t* frame::unextended_sp() const     { return _unextended_sp; }

inline void frame::set_unextended_sp(intptr_t* value) {
  Unimplemented();
}

inline int frame::offset_unextended_sp() const {
  Unimplemented();
  return 0;
}

inline void frame::set_offset_unextended_sp(int value) {
  Unimplemented();
}

inline intptr_t* frame::real_fp() const {
  if (_cb != NULL) {
    // use the frame size if valid
    int size = _cb->frame_size();
    if (size > 0) {
      return unextended_sp() + size;
    }
  }
  // else rely on fp()
  assert(!is_compiled_frame(), "unknown compiled frame size");
  return fp();
}

inline int frame::frame_size() const {
  return is_interpreted_frame()
    ? sender_sp() - sp()
    : cb()->frame_size();
}

// Return address
inline address* frame::sender_pc_addr() const     { return (address*) addr_at(return_addr_offset); }
inline address  frame::sender_pc() const          { return *sender_pc_addr(); }
inline intptr_t* frame::sender_sp() const         { return addr_at(sender_sp_offset); }

inline intptr_t** frame::interpreter_frame_locals_addr() const {
  return (intptr_t**)addr_at(interpreter_frame_locals_offset);
}

inline intptr_t* frame::interpreter_frame_last_sp() const {
  return (intptr_t*)at(interpreter_frame_last_sp_offset);
}

inline intptr_t* frame::interpreter_frame_bcp_addr() const {
  return (intptr_t*)addr_at(interpreter_frame_bcp_offset);
}

inline intptr_t* frame::interpreter_frame_mdp_addr() const {
  return (intptr_t*)addr_at(interpreter_frame_mdp_offset);
}


// Constant pool cache

inline ConstantPoolCache** frame::interpreter_frame_cache_addr() const {
  return (ConstantPoolCache**)addr_at(interpreter_frame_cache_offset);
}

// Method

inline Method** frame::interpreter_frame_method_addr() const {
  return (Method**)addr_at(interpreter_frame_method_offset);
}

// Mirror

inline oop* frame::interpreter_frame_mirror_addr() const {
  return (oop*)addr_at(interpreter_frame_mirror_offset);
}

// top of expression stack
inline intptr_t* frame::interpreter_frame_tos_address() const {
  intptr_t* last_sp = interpreter_frame_last_sp();
  if (last_sp == NULL) {
    return sp();
  } else {
    // sp() may have been extended or shrunk by an adapter.  At least
    // check that we don't fall behind the legal region.
    // For top deoptimized frame last_sp == interpreter_frame_monitor_end.
    assert(last_sp <= (intptr_t*) interpreter_frame_monitor_end(), "bad tos");
    return last_sp;
  }
}

inline oop* frame::interpreter_frame_temp_oop_addr() const {
  return (oop *)(fp() + interpreter_frame_oop_temp_offset);
}

inline int frame::interpreter_frame_monitor_size() {
  return BasicObjectLock::size();
}


// expression stack
// (the max_stack arguments are used by the GC; see class FrameClosure)

inline intptr_t* frame::interpreter_frame_expression_stack() const {
  intptr_t* monitor_end = (intptr_t*) interpreter_frame_monitor_end();
  return monitor_end-1;
}


// Entry frames

inline JavaCallWrapper** frame::entry_frame_call_wrapper_addr() const {
 return (JavaCallWrapper**)addr_at(entry_frame_call_wrapper_offset);
}


// Compiled frames
PRAGMA_DIAG_PUSH
PRAGMA_NONNULL_IGNORED
inline oop frame::saved_oop_result(RegisterMap* map) const {
  oop* result_adr = (oop *)map->location(x10->as_VMReg(), nullptr);
  guarantee(result_adr != NULL, "bad register save location");
  return (*result_adr);
}

inline void frame::set_saved_oop_result(RegisterMap* map, oop obj) {
  oop* result_adr = (oop *)map->location(x10->as_VMReg(), nullptr);
  guarantee(result_adr != NULL, "bad register save location");
  *result_adr = obj;
}
PRAGMA_DIAG_POP

inline const ImmutableOopMap* frame::get_oop_map() const {
  Unimplemented();
  return NULL;
}

inline int frame::compiled_frame_stack_argsize() const {
  Unimplemented();
  return 0;
}

inline void frame::interpreted_frame_oop_map(InterpreterOopMap* mask) const {
  Unimplemented();
}

inline int frame::sender_sp_ret_address_offset() {
  Unimplemented();
  return 0;
}

//------------------------------------------------------------------------------
// frame::sender
frame frame::sender(RegisterMap* map) const {
  frame result = sender_raw(map);

  if (map->process_frames()) {
    StackWatermarkSet::on_iteration(map->thread(), result);
  }

  return result;
}

//------------------------------------------------------------------------------
// frame::sender_raw
frame frame::sender_raw(RegisterMap* map) const {
  // Default is we done have to follow them. The sender_for_xxx will
  // update it accordingly
  assert(map != NULL, "map must be set");
  map->set_include_argument_oops(false);

  if (is_entry_frame()) {
    return sender_for_entry_frame(map);
  }
  if (is_interpreted_frame()) {
    return sender_for_interpreter_frame(map);
  }
  assert(_cb == CodeCache::find_blob(pc()),"Must be the same");

  // This test looks odd: why is it not is_compiled_frame() ?  That's
  // because stubs also have OOP maps.
  if (_cb != NULL) {
    return sender_for_compiled_frame(map);
  }

  // Must be native-compiled frame, i.e. the marshaling code for native
  // methods that exists in the core system.
  return frame(sender_sp(), link(), sender_pc());
}

//------------------------------------------------------------------------------
// frame::sender_for_compiled_frame
frame frame::sender_for_compiled_frame(RegisterMap* map) const {
  // we cannot rely upon the last fp having been saved to the thread
  // in C2 code but it will have been pushed onto the stack. so we
  // have to find it relative to the unextended sp

  assert(_cb->frame_size() >= 0, "must have non-zero frame size");
  intptr_t* l_sender_sp = unextended_sp() + _cb->frame_size();
  intptr_t* unextended_sp = l_sender_sp;

  // the return_address is always the word on the stack
  address sender_pc = (address) *(l_sender_sp + frame::return_addr_offset);

  intptr_t** saved_fp_addr = (intptr_t**) (l_sender_sp + frame::link_offset);

  assert(map != NULL, "map must be set");
  if (map->update_map()) {
    // Tell GC to use argument oopmaps for some runtime stubs that need it.
    // For C1, the runtime stub might not have oop maps, so set this flag
    // outside of update_register_map.
    map->set_include_argument_oops(_cb->caller_must_gc_arguments(map->thread()));
    if (_cb->oop_maps() != NULL) {
      OopMapSet::update_register_map(this, map);
    }

    // Since the prolog does the save and restore of FP there is no
    // oopmap for it so we must fill in its location as if there was
    // an oopmap entry since if our caller was compiled code there
    // could be live jvm state in it.
    update_map_with_saved_link(map, saved_fp_addr);
  }

  return frame(l_sender_sp, unextended_sp, *saved_fp_addr, sender_pc);
}

//------------------------------------------------------------------------------
// frame::update_map_with_saved_link
template <typename RegisterMapT>
void frame::update_map_with_saved_link(RegisterMapT* map, intptr_t** link_addr) {
  // The interpreter and compiler(s) always save fp in a known
  // location on entry. We must record where that location is
  // so that if fp was live on callout from c2 we can find
  // the saved copy no matter what it called.

  // Since the interpreter always saves fp if we record where it is then
  // we don't have to always save fp on entry and exit to c2 compiled
  // code, on entry will be enough.
  assert(map != NULL, "map must be set");
  map->set_location(::fp->as_VMReg(), (address) link_addr);
  // this is weird "H" ought to be at a higher address however the
  // oopMaps seems to have the "H" regs at the same address and the
  // vanilla register.
  map->set_location(::fp->as_VMReg()->next(), (address) link_addr);
}

#endif // CPU_RISCV_FRAME_RISCV_INLINE_HPP
