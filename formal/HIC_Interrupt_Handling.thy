(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

(* 
 * HIC 中断处理系统形式化验证
 * 
 * 验证中断处理的正确性和安全性：
 * 1. 中断路由正确性
 * 2. 中断嵌套安全性
 * 3. 中断与域隔离
 * 4. 动态中断池管理
 *)

theory HIC_Interrupt_Handling
imports Main
begin

(* ==================== 基础类型定义 ==================== *)

type_synonym irq_id = nat
type_synonym domain_id = nat
type_synonym service_id = nat
type_synonym cpu_id = nat
type_synonym vector_t = nat

(* 中断向量范围 *)
definition IRQ_STATIC_START :: nat where
  "IRQ_STATIC_START = 32"

definition IRQ_STATIC_END :: nat where
  "IRQ_STATIC_END = 200"

definition IRQ_DYNAMIC_START :: nat where
  "IRQ_DYNAMIC_START = 200"

definition IRQ_DYNAMIC_COUNT :: nat where
  "IRQ_DYNAMIC_COUNT = 56"

(* ==================== 中断类型定义 ==================== *)

(* 中断类型 *)
datatype irq_type =
    IRQ_TYPE_STATIC    (* 静态路由中断 *)
  | IRQ_TYPE_DYNAMIC   (* 动态分配中断 *)
  | IRQ_TYPE_EXCEPTION (* CPU 异常 *)
  | IRQ_TYPE_IPI       (* 处理器间中断 *)

(* 中断状态 *)
datatype irq_state =
    IRQ_STATE_DISABLED
  | IRQ_STATE_ENABLED
  | IRQ_STATE_PENDING
  | IRQ_STATE_ACTIVE

(* 中断优先级 *)
type_synonym irq_priority = nat

(* ==================== 中断路由表 ==================== *)

(* 路由条目 *)
record irq_route_entry =
  ire_vector      :: vector_t
  ire_type        :: irq_type
  ire_destination :: "domain_id option"  (* 目标服务 *)
  ire_handler     :: "service_id option"
  ire_enabled     :: bool
  ire_priority    :: irq_priority

(* 中断路由表 *)
type_synonym irq_route_table = "vector_t \<Rightarrow> irq_route_entry option"

(* ==================== 中断控制器状态 ==================== *)

(* CPU 中断状态 *)
record cpu_irq_state =
  cis_idt_loaded    :: bool
  cis_current_irq   :: "irq_id option"
  cis_irq_depth     :: nat
  cis_irq_enabled   :: bool

(* 中断控制器状态 *)
record irq_controller_state =
  ics_route_table    :: irq_route_table
  ics_cpu_states     :: "cpu_id \<Rightarrow> cpu_irq_state"
  ics_dynamic_bitmap :: "nat \<Rightarrow> bool"  (* 动态中断位图 *)

(* ==================== 静态中断路由 ==================== *)

(* 有效静态中断 *)
definition valid_static_irq :: "vector_t \<Rightarrow> bool" where
  "valid_static_irq vec \<equiv>
     vec \<ge> IRQ_STATIC_START \<and> vec < IRQ_STATIC_END"

(* 静态路由有效 *)
definition valid_static_route :: "irq_route_entry \<Rightarrow> bool" where
  "valid_static_route entry \<equiv>
     ire_type entry = IRQ_TYPE_STATIC \<and>
     valid_static_irq (ire_vector entry) \<and>
     ire_destination entry \<noteq> None"

(* ==================== 动态中断池 ==================== *)

(* 有效动态中断 *)
definition valid_dynamic_irq :: "vector_t \<Rightarrow> bool" where
  "valid_dynamic_irq vec \<equiv>
     vec \<ge> IRQ_DYNAMIC_START \<and>
     vec < IRQ_DYNAMIC_START + IRQ_DYNAMIC_COUNT"

(* 分配动态中断 *)
definition alloc_dynamic_irq :: "irq_controller_state \<Rightarrow> (vector_t \<times> irq_controller_state) option" where
  "alloc_dynamic_irq ics \<equiv>
     let free_vec = (LEAST v. valid_dynamic_irq v \<and> \<not> ics_dynamic_bitmap ics v)
     in if free_vec < IRQ_DYNAMIC_START + IRQ_DYNAMIC_COUNT then
          Some (free_vec, 
                ics\<lparr>ics_dynamic_bitmap := (ics_dynamic_bitmap ics)(free_vec := True)\<rparr>)
        else
          None"

(* 释放动态中断 *)
definition free_dynamic_irq :: "irq_controller_state \<Rightarrow> vector_t \<Rightarrow> irq_controller_state option" where
  "free_dynamic_irq ics vec \<equiv>
     if valid_dynamic_irq vec \<and> ics_dynamic_bitmap ics vec then
       Some (ics\<lparr>ics_dynamic_bitmap := (ics_dynamic_bitmap ics)(vec := False)\<rparr>)
     else
       None"

(* ==================== 中断处理流程 ==================== *)

(* 中断上下文 *)
record irq_context =
  ic_irq_id     :: irq_id
  ic_vector     :: vector_t
  ic_cpu        :: cpu_id
  ic_domain     :: domain_id
  ic_pre_irq    :: "cpu_irq_state"

(* 中断入口 *)
definition irq_entry :: "irq_controller_state \<Rightarrow> cpu_id \<Rightarrow> vector_t \<Rightarrow> (irq_context \<times> irq_controller_state) option" where
  "irq_entry ics cpu vec \<equiv>
     case ics_route_table ics vec of
       None \<Rightarrow> None
     | Some entry \<Rightarrow>
         if \<not> ire_enabled entry then
           None
         else
           let cpu_state = ics_cpu_states ics cpu;
               ctx = \<lparr>
                 ic_irq_id = vec,
                 ic_vector = vec,
                 ic_cpu = cpu,
                 ic_domain = the (ire_destination entry),
                 ic_pre_irq = cpu_state
               \<rparr>;
               new_cpu_state = cpu_state\<lparr>
                 cis_current_irq := Some vec,
                 cis_irq_depth := cis_irq_depth cpu_state + 1
               \<rparr>
           in Some (ctx, ics\<lparr>ics_cpu_states := (ics_cpu_states ics)(cpu := new_cpu_state)\<rparr>)"

(* 中断退出 *)
definition irq_exit :: "irq_controller_state \<Rightarrow> cpu_id \<Rightarrow> irq_controller_state" where
  "irq_exit ics cpu \<equiv>
     let cpu_state = ics_cpu_states ics cpu;
         new_depth = (if cis_irq_depth cpu_state > 0 then cis_irq_depth cpu_state - 1 else 0);
         new_cpu_state = cpu_state\<lparr>
           cis_current_irq := (if new_depth = 0 then None else cis_current_irq cpu_state),
           cis_irq_depth := new_depth
         \<rparr>
     in ics\<lparr>ics_cpu_states := (ics_cpu_states ics)(cpu := new_cpu_state)\<rparr>"

(* ==================== 中断不变式 ==================== *)

(* IDT 已加载 *)
definition idt_loaded_invariant :: "irq_controller_state \<Rightarrow> cpu_id \<Rightarrow> bool" where
  "idt_loaded_invariant ics cpu \<equiv>
     cis_idt_loaded (ics_cpu_states ics cpu)"

(* 中断深度一致性 *)
definition irq_depth_consistent :: "irq_controller_state \<Rightarrow> cpu_id \<Rightarrow> bool" where
  "irq_depth_consistent ics cpu \<equiv>
     let depth = cis_irq_depth (ics_cpu_states ics cpu)
     in (depth = 0 \<longleftrightarrow> cis_current_irq (ics_cpu_states ics cpu) = None) \<and>
        (depth > 0 \<longrightarrow> cis_current_irq (ics_cpu_states ics cpu) \<noteq> None)"

(* 路由表有效性 *)
definition route_table_valid :: "irq_controller_state \<Rightarrow> bool" where
  "route_table_valid ics \<equiv>
     \<forall>vec. case ics_route_table ics vec of
              None \<Rightarrow> True
            | Some entry \<Rightarrow>
                (ire_type entry = IRQ_TYPE_STATIC \<longrightarrow> valid_static_route entry) \<and>
                (ire_type entry = IRQ_TYPE_DYNAMIC \<longrightarrow> valid_dynamic_irq vec)"

(* 全局中断不变式 *)
definition irq_invariant :: "irq_controller_state \<Rightarrow> bool" where
  "irq_invariant ics \<equiv>
     route_table_valid ics \<and>
     (\<forall>cpu. idt_loaded_invariant ics cpu \<and> irq_depth_consistent ics cpu)"

(* ==================== 中断定理 ==================== *)

(* 定理1: 中断入口增加深度 *)
theorem irq_entry_increases_depth:
  assumes "irq_entry ics cpu vec = Some (ctx, ics')"
  shows   "cis_irq_depth (ics_cpu_states ics' cpu) = 
           cis_irq_depth (ics_cpu_states ics cpu) + 1"
  using assms
  unfolding irq_entry_def
  by (auto split: option.split)

(* 定理2: 中断退出减少深度 *)
theorem irq_exit_decreases_depth:
  assumes "cis_irq_depth (ics_cpu_states ics cpu) > 0"
  shows   "cis_irq_depth (ics_cpu_states (irq_exit ics cpu) cpu) = 
           cis_irq_depth (ics_cpu_states ics cpu) - 1"
  using assms
  unfolding irq_exit_def Let_def
  by auto

(* 定理3: 深度为零时无当前中断 *)
theorem zero_depth_no_current:
  assumes "irq_depth_consistent ics cpu"
  and     "cis_irq_depth (ics_cpu_states ics cpu) = 0"
  shows   "cis_current_irq (ics_cpu_states ics cpu) = None"
  using assms
  unfolding irq_depth_consistent_def Let_def
  by auto

(* 定理4: 动态中断分配有效 *)
theorem dynamic_alloc_valid:
  assumes "alloc_dynamic_irq ics = Some (vec, ics')"
  shows   "valid_dynamic_irq vec \<and> ics_dynamic_bitmap ics' vec"
  using assms
  unfolding alloc_dynamic_irq_def valid_dynamic_irq_def
  by (auto split: if_splits)

(* 定理5: 中断入口保持不变式 *)
theorem irq_entry_preserves_invariant:
  assumes "irq_invariant ics"
  and     "irq_entry ics cpu vec = Some (ctx, ics')"
  shows   "irq_depth_consistent ics' cpu"
  using assms
  unfolding irq_invariant_def irq_entry_def irq_depth_consistent_def Let_def
  by (auto split: option.split)

(* ==================== 中断嵌套安全 ==================== *)

(* 最大嵌套深度 *)
definition MAX_IRQ_DEPTH :: nat where
  "MAX_IRQ_DEPTH = 16"

(* 嵌套安全 *)
definition nesting_safe :: "irq_controller_state \<Rightarrow> cpu_id \<Rightarrow> bool" where
  "nesting_safe ics cpu \<equiv>
     cis_irq_depth (ics_cpu_states ics cpu) \<le> MAX_IRQ_DEPTH"

(* 定理6: 嵌套深度有界 *)
theorem nesting_depth_bounded:
  assumes "nesting_safe ics cpu"
  and     "irq_entry ics cpu vec = Some (ctx, ics')"
  and     "cis_irq_depth (ics_cpu_states ics cpu) < MAX_IRQ_DEPTH"
  shows   "nesting_safe ics' cpu"
  using assms
  unfolding nesting_safe_def irq_entry_def MAX_IRQ_DEPTH_def
  by (auto split: option.split)

(* ==================== 中断与域隔离 ==================== *)

(* 中断域隔离检查 *)
definition irq_domain_isolated :: "irq_controller_state \<Rightarrow> domain_id \<Rightarrow> domain_id \<Rightarrow> bool" where
  "irq_domain_isolated ics d1 d2 \<equiv>
     d1 \<noteq> d2 \<longrightarrow>
     (\<forall>vec. case ics_route_table ics vec of
              None \<Rightarrow> True
            | Some entry \<Rightarrow>
                ire_destination entry = Some d1 \<longrightarrow>
                \<not> (\<exists>vec'. vec' \<noteq> vec \<and>
                           ics_route_table ics vec' \<noteq> None \<and>
                           ire_destination (the (ics_route_table ics vec')) = Some d2))"

(* 定理7: 域中断隔离 *)
theorem domain_irq_isolation:
  assumes "irq_domain_isolated ics d1 d2"
  and     "d1 \<noteq> d2"
  and     "ics_route_table ics vec1 = Some entry1"
  and     "ire_destination entry1 = Some d1"
  shows   "\<forall>vec2. ics_route_table ics vec2 = Some entry2 \<longrightarrow>
                   ire_destination entry2 \<noteq> Some d2"
  using assms
  unfolding irq_domain_isolated_def
  by auto

(* ==================== 中断屏蔽 ==================== *)

(* CPU 中断屏蔽状态 *)
datatype irq_mask_state =
    MASK_ALL        (* 屏蔽所有中断 *)
  | MASK_DYNAMIC    (* 仅屏蔽动态中断 *)
  | MASK_NONE       (* 不屏蔽 *)

(* 设置中断屏蔽 *)
definition set_irq_mask :: "cpu_irq_state \<Rightarrow> irq_mask_state \<Rightarrow> cpu_irq_state" where
  "set_irq_mask cis mask_val = (
     case mask_val of
       MASK_ALL \<Rightarrow> cis\<lparr>cis_irq_enabled := False\<rparr>
     | MASK_NONE \<Rightarrow> cis\<lparr>cis_irq_enabled := True\<rparr>
     | MASK_DYNAMIC \<Rightarrow> cis)"

(* 定理8: 屏蔽状态正确 *)
theorem mask_state_correct:
  assumes "set_irq_mask cis MASK_ALL = cis'"
  shows   "\<not> cis_irq_enabled cis'"
  using assms
  unfolding set_irq_mask_def
  by auto

(* ==================== 异常处理 ==================== *)

(* CPU 异常类型 *)
datatype exception_type =
    EXC_DIVIDE_ERROR
  | EXC_DEBUG
  | EXC_NMI
  | EXC_BREAKPOINT
  | EXC_OVERFLOW
  | EXC_BOUND_RANGE
  | EXC_INVALID_OPCODE
  | EXC_DEVICE_NOT_AVAILABLE
  | EXC_DOUBLE_FAULT
  | EXC_PAGE_FAULT
  | EXC_GENERAL_PROTECTION

(* 异常信息 *)
record exception_info =
  ei_type       :: exception_type
  ei_vector     :: vector_t
  ei_error_code :: nat
  ei_rip        :: nat
  ei_cs         :: nat
  ei_rflags     :: nat

(* 异常处理结果 *)
datatype exception_result =
    EXC_RESUME      (* 恢复执行 *)
  | EXC_TERMINATE   (* 终止进程 *)
  | EXC_PANIC       (* 系统崩溃 *)

(* 异常处理函数类型 *)
type_synonym exception_handler = "exception_info \<Rightarrow> exception_result"

(* ==================== 完整中断系统 ==================== *)

(* 完整中断系统状态 *)
record full_irq_system =
  fis_controller  :: irq_controller_state
  fis_exceptions  :: "exception_type \<Rightarrow> exception_handler option"

(* 完整中断系统不变式 *)
definition full_irq_invariant :: "full_irq_system \<Rightarrow> bool" where
  "full_irq_invariant fis \<equiv>
     irq_invariant (fis_controller fis) \<and>
     (\<forall>exc. fis_exceptions fis exc \<noteq> None)"

(* 定理9: 完整系统一致性 *)
theorem full_system_consistent:
  assumes "full_irq_invariant fis"
  shows   "irq_invariant (fis_controller fis)"
  using assms
  unfolding full_irq_invariant_def
  by auto

end
