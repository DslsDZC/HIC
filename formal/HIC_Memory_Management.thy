(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

(* 
 * HIC 内存管理系统形式化验证
 * 
 * 验证内存管理的正确性和安全性：
 * 1. 物理内存分配器正确性
 * 2. 页表管理正确性
 * 3. 内存隔离性
 * 4. 内存配额管理
 *)

theory HIC_Memory_Management
imports Main
begin

(* ==================== 基础类型定义 ==================== *)

type_synonym phys_addr = nat
type_synonym virt_addr = nat
type_synonym size_t = nat
type_synonym frame_id = nat
type_synonym domain_id = nat

(* 页大小 *)
definition PAGE_SIZE :: nat where
  "PAGE_SIZE = 4096"  (* 4KB *)

(* 页对齐 *)
definition page_aligned :: "nat \<Rightarrow> bool" where
  "page_aligned addr \<equiv> addr mod PAGE_SIZE = 0"

(* ==================== 物理内存帧 ==================== *)

(* 帧状态 *)
datatype frame_state =
    FRAME_FREE
  | FRAME_ALLOCATED
  | FRAME_RESERVED
  | FRAME_MMIO

(* 帧记录 *)
record frame =
  frame_id      :: frame_id
  frame_addr    :: phys_addr
  frame_state   :: frame_state
  frame_owner   :: "domain_id option"

(* 帧位图 *)
type_synonym frame_bitmap = "frame_id \<Rightarrow> bool"

(* ==================== 物理内存管理器 ==================== *)

(* 内存区域 *)
record mem_region =
  mr_base  :: phys_addr
  mr_size  :: size_t

(* 物理内存管理器状态 *)
record pmm_state =
  pmm_bitmap       :: frame_bitmap
  pmm_max_frames   :: nat
  pmm_total_frames :: nat
  pmm_free_frames  :: nat
  pmm_used_memory  :: size_t
  pmm_total_memory :: size_t

(* ==================== 内存分配操作 ==================== *)

(* 分配帧 *)
definition alloc_frame :: "pmm_state \<Rightarrow> (phys_addr \<times> pmm_state) option" where
  "alloc_frame pmm \<equiv>
     if pmm_free_frames pmm = 0 then
       None
     else
       let free_id = (LEAST i. i < pmm_max_frames pmm \<and> 
                               \<not> pmm_bitmap pmm i)
       in Some (free_id * PAGE_SIZE,
                pmm\<lparr>
                  pmm_bitmap := (pmm_bitmap pmm)(free_id := True),
                  pmm_free_frames := pmm_free_frames pmm - 1,
                  pmm_used_memory := pmm_used_memory pmm + PAGE_SIZE
                \<rparr>)"

(* 释放帧 *)
definition free_frame :: "pmm_state \<Rightarrow> phys_addr \<Rightarrow> pmm_state option" where
  "free_frame pmm addr \<equiv>
     if \<not> page_aligned addr then
       None
     else
       let frame_id = addr div PAGE_SIZE
       in if frame_id \<ge> pmm_max_frames pmm then
            None
          else if \<not> pmm_bitmap pmm frame_id then
            None
          else
            Some (pmm\<lparr>
              pmm_bitmap := (pmm_bitmap pmm)(frame_id := False),
              pmm_free_frames := pmm_free_frames pmm + 1,
              pmm_used_memory := pmm_used_memory pmm - PAGE_SIZE
            \<rparr>)"

(* ==================== PMM 不变式 ==================== *)

(* PMM 一致性不变式 *)
definition pmm_invariant :: "pmm_state \<Rightarrow> bool" where
  "pmm_invariant pmm \<equiv>
     pmm_free_frames pmm + 
     (pmm_total_frames pmm - pmm_free_frames pmm) = pmm_total_frames pmm \<and>
     pmm_used_memory pmm = (pmm_total_frames pmm - pmm_free_frames pmm) * PAGE_SIZE \<and>
     pmm_total_frames pmm \<le> pmm_max_frames pmm"

(* 位图一致性 *)
definition bitmap_consistent :: "pmm_state \<Rightarrow> bool" where
  "bitmap_consistent pmm \<equiv>
     card {i. i < pmm_max_frames pmm \<and> pmm_bitmap pmm i} =
     pmm_total_frames pmm - pmm_free_frames pmm"

(* ==================== PMM 定理 ==================== *)

(* 定理1: 分配减少空闲帧 *)
theorem alloc_decreases_free:
  assumes "alloc_frame pmm = Some (addr, pmm')"
  shows   "pmm_free_frames pmm' = pmm_free_frames pmm - 1"
  using assms
  unfolding alloc_frame_def
  by (auto split: if_splits)

(* 定理2: 释放增加空闲帧 *)
theorem free_increases_free:
  assumes "free_frame pmm addr = Some pmm'"
  shows   "pmm_free_frames pmm' = pmm_free_frames pmm + 1"
  using assms
  unfolding free_frame_def page_aligned_def
  by (auto split: if_splits)

(* 定理3: 分配返回对齐地址 *)
theorem alloc_returns_aligned:
  assumes "alloc_frame pmm = Some (addr, pmm')"
  shows   "page_aligned addr"
  using assms
  unfolding alloc_frame_def page_aligned_def
  by auto

(* 定理4: PMM 不变式在分配后保持 *)
theorem alloc_preserves_invariant:
  assumes "pmm_invariant pmm"
  and     "pmm_free_frames pmm > 0"
  and     "alloc_frame pmm = Some (addr, pmm')"
  shows   "pmm_invariant pmm'"
  using assms
  unfolding pmm_invariant_def alloc_frame_def
  by auto

(* 定理5: PMM 不变式在释放后保持 *)
theorem free_preserves_invariant:
  assumes "pmm_invariant pmm"
  and     "free_frame pmm addr = Some pmm'"
  shows   "pmm_invariant pmm'"
  using assms
  unfolding pmm_invariant_def free_frame_def page_aligned_def
  by (auto split: if_splits)

(* ==================== 页表管理 ==================== *)

(* 页表项权限 *)
datatype page_perm =
    PERM_NONE
  | PERM_R
  | PERM_RW
  | PERM_RX
  | PERM_RWX

(* 页表项 *)
record pte =
  pte_present   :: bool
  pte_writable  :: bool
  pte_executable :: bool
  pte_user      :: bool
  pte_phys_addr :: phys_addr

(* 页表 *)
type_synonym page_table = "virt_addr \<Rightarrow> pte option"

(* 映射类型 *)
datatype map_type =
    MAP_TYPE_KERNEL
  | MAP_TYPE_USER
  | MAP_TYPE_SHARED

(* ==================== 页表操作 ==================== *)

(* 映射页 *)
definition map_page :: "page_table \<Rightarrow> virt_addr \<Rightarrow> phys_addr \<Rightarrow> page_perm \<Rightarrow> page_table option" where
  "map_page pt vaddr paddr perm = (
     if \<not> page_aligned vaddr \<or> \<not> page_aligned paddr then
       None
     else
       let new_pte = \<lparr>
         pte_present = True,
         pte_writable = perm = PERM_RW \<or> perm = PERM_RWX,
         pte_executable = perm = PERM_RX \<or> perm = PERM_RWX,
         pte_user = True,
         pte_phys_addr = paddr
       \<rparr>
       in Some (pt(vaddr := Some new_pte)))"

(* 取消映射 *)
definition unmap_page :: "page_table \<Rightarrow> virt_addr \<Rightarrow> page_table" where
  "unmap_page pt vaddr \<equiv> pt(vaddr := None)"

(* ==================== 页表不变式 ==================== *)

(* 页表有效性 *)
definition valid_page_table :: "page_table \<Rightarrow> bool" where
  "valid_page_table pt \<equiv>
     (\<forall>vaddr. case pt vaddr of
                None \<Rightarrow> True
              | Some entry \<Rightarrow>
                  pte_present entry \<longrightarrow>
                  page_aligned (pte_phys_addr entry))"

(* 双重映射检测 *)
definition no_double_mapping :: "page_table \<Rightarrow> bool" where
  "no_double_mapping pt \<equiv>
     \<forall>v1 v2. v1 \<noteq> v2 \<and> pt v1 \<noteq> None \<and> pt v2 \<noteq> None \<longrightarrow>
               pte_phys_addr (the (pt v1)) \<noteq> pte_phys_addr (the (pt v2))"

(* ==================== 页表定理 ==================== *)

(* 定理6: 映射创建有效条目 *)
theorem map_creates_valid_entry:
  assumes "map_page pt vaddr paddr perm = Some pt'"
  and     "pt' vaddr = Some entry"
  shows   "pte_present entry \<and> pte_phys_addr entry = paddr"
  using assms
  unfolding map_page_def Let_def
  by auto

(* 定理7: 取消映射移除条目 *)
theorem unmap_removes_entry:
  assumes "unmap_page pt vaddr = pt'"
  shows   "pt' vaddr = None"
  using assms
  unfolding unmap_page_def
  by auto

(* 定理8: 有效页表在映射后保持 *)
theorem map_preserves_validity:
  assumes "valid_page_table pt"
  and     "map_page pt vaddr paddr perm = Some pt'"
  shows   "valid_page_table pt'"
  using assms
  unfolding valid_page_table_def map_page_def
  by (auto split: option.split)

(* ==================== 内存隔离 ==================== *)

(* 域内存空间 *)
record domain_memory =
  dm_phys_base  :: phys_addr
  dm_phys_size  :: size_t
  dm_virt_base  :: virt_addr
  dm_page_table :: page_table

(* 内存隔离性 *)
definition memories_isolated :: "domain_memory \<Rightarrow> domain_memory \<Rightarrow> bool" where
  "memories_isolated dm1 dm2 \<equiv>
     let end1 = dm_phys_base dm1 + dm_phys_size dm1;
         end2 = dm_phys_base dm2 + dm_phys_size dm2
     in dm_phys_base dm1 \<ge> end2 \<or> dm_phys_base dm2 \<ge> end1"

(* 域内存表 *)
type_synonym domain_memory_table = "domain_id \<Rightarrow> domain_memory option"

(* 全局内存隔离 *)
definition all_memories_isolated :: "domain_memory_table \<Rightarrow> bool" where
  "all_memories_isolated dmt \<equiv>
     \<forall>d1 d2. d1 \<noteq> d2 \<longrightarrow>
       (case dmt d1 of
          None \<Rightarrow> True
        | Some dm1 \<Rightarrow>
            case dmt d2 of
              None \<Rightarrow> True
            | Some dm2 \<Rightarrow> memories_isolated dm1 dm2)"

(* ==================== 内存隔离定理 ==================== *)

(* 定理9: 隔离对称性 *)
theorem isolation_symmetric:
  assumes "memories_isolated dm1 dm2"
  shows   "memories_isolated dm2 dm1"
  using assms
  unfolding memories_isolated_def Let_def
  by auto

(* 定理10: 全局隔离蕴含域间隔离 *)
theorem global_isolation_implies_pairwise:
  assumes "all_memories_isolated dmt"
  and     "dmt d1 = Some dm1"
  and     "dmt d2 = Some dm2"
  and     "d1 \<noteq> d2"
  shows   "memories_isolated dm1 dm2"
  using assms
  unfolding all_memories_isolated_def
  by auto

(* ==================== 内存配额 ==================== *)

(* 内存配额 *)
record memory_quota =
  mq_max_pages   :: nat
  mq_used_pages  :: nat

(* 配额检查 *)
definition quota_satisfied :: "memory_quota \<Rightarrow> bool" where
  "quota_satisfied mq \<equiv> mq_used_pages mq \<le> mq_max_pages mq"

(* 配额分配 *)
definition alloc_under_quota :: "memory_quota \<Rightarrow> (memory_quota) option" where
  "alloc_under_quota mq \<equiv>
     if mq_used_pages mq < mq_max_pages mq then
       Some (mq\<lparr>mq_used_pages := mq_used_pages mq + 1\<rparr>)
     else
       None"

(* 定理11: 配额分配保持约束 *)
theorem alloc_preserves_quota:
  assumes "alloc_under_quota mq = Some mq'"
  shows   "quota_satisfied mq'"
  using assms
  unfolding alloc_under_quota_def quota_satisfied_def
  by auto

(* ==================== 完整内存系统 ==================== *)

(* 完整内存系统状态 *)
record memory_system =
  ms_pmm         :: pmm_state
  ms_domains     :: domain_memory_table
  ms_quotas     :: "domain_id \<Rightarrow> memory_quota"

(* 内存系统不变式 *)
definition memory_system_invariant :: "memory_system \<Rightarrow> bool" where
  "memory_system_invariant ms \<equiv>
     pmm_invariant (ms_pmm ms) \<and>
     all_memories_isolated (ms_domains ms) \<and>
     (\<forall>d. case ms_domains ms d of
            None \<Rightarrow> True
          | Some dm \<Rightarrow>
              quota_satisfied (ms_quotas ms d))"

(* 定理12: 内存系统一致性 *)
theorem memory_system_consistent:
  assumes "memory_system_invariant ms"
  shows   "pmm_invariant (ms_pmm ms) \<and>
           all_memories_isolated (ms_domains ms)"
  using assms
  unfolding memory_system_invariant_def
  by auto

end
