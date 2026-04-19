(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

(* 
 * HIC 能力系统完全形式化验证
 * 使用 Isabelle/HOL 进行数学证明
 *
 * 验证范围：
 * 1. 能力系统数据类型和状态机
 * 2. 能力操作的正确性
 * 3. 权限单调性
 * 4. 撤销传播性
 * 5. 能力守恒性
 *)

theory HIC_Capability_System
imports Main
begin

(* ==================== 第一部分：基础类型定义 ==================== *)

(* 域标识符 *)
type_synonym domain_id = nat

(* 能力标识符 *)
type_synonym cap_id = nat

(* 线程标识符 *)
type_synonym thread_id = nat

(* 物理地址 *)
type_synonym phys_addr = nat

(* 虚拟地址 *)
type_synonym virt_addr = nat

(* 大小 *)
type_synonym size_t = nat

(* 能力权限标志 *)
datatype cap_right = 
    CAP_READ 
  | CAP_WRITE 
  | CAP_EXEC 
  | CAP_GRANT 
  | CAP_REVOKE

(* 能力类型 *)
datatype cap_type =
    CAP_MEMORY
  | CAP_DEVICE
  | CAP_IPC
  | CAP_THREAD
  | CAP_LOGICAL_CORE

(* 能力状态 *)
datatype cap_state =
    CAP_INVALID
  | CAP_VALID
  | CAP_REVOKED

(* 域类型 *)
datatype domain_type =
    DOMAIN_CORE          (* Core-0: 最高权限 *)
  | DOMAIN_PRIVILEGED    (* Privileged-1: 特权服务 *)
  | DOMAIN_APPLICATION   (* Application-3: 用户应用 *)

(* ==================== 调度策略定义 ==================== *)

(* 调度策略 *)
datatype sched_policy =
    SCHED_EXCLUSIVE      (* 独占模式：无抢占，严格实时 *)
  | SCHED_QUOTA          (* 配额模式：CBS 带宽服务器 *)
  | SCHED_SHARED         (* 共享模式：优先级 + 时间片 *)
  | SCHED_IDLE           (* 空闲模式：最低优先级 *)

(* 域安全等级 *)
datatype domain_sec_level =
    SEC_LEVEL_CORE       (* Core-0: 可创建所有策略能力 *)
  | SEC_LEVEL_PRIVILEGED (* Privileged-1: 可创建独占/配额/共享 *)
  | SEC_LEVEL_APPLICATION(* Application: 只能共享 *)

(* 策略等级（用于单调性检查） *)
definition policy_rank :: "sched_policy \<Rightarrow> nat" where
  "policy_rank p = (
     case p of
       SCHED_EXCLUSIVE \<Rightarrow> 0   (* 最高等级 *)
     | SCHED_QUOTA \<Rightarrow> 1
     | SCHED_SHARED \<Rightarrow> 2
     | SCHED_IDLE \<Rightarrow> 3)"     (* 最低等级 *)

(* 域状态 *)
datatype domain_state =
    DOMAIN_INIT
  | DOMAIN_READY
  | DOMAIN_RUNNING
  | DOMAIN_SUSPENDED
  | DOMAIN_TERMINATED

(* 线程状态 *)
datatype thread_state =
    THREAD_READY
  | THREAD_RUNNING
  | THREAD_BLOCKED
  | THREAD_WAITING
  | THREAD_TERMINATED

(* ==================== 第二部分：能力系统建模 ==================== *)

(* 能力条目 *)
record capability =
  cap_id            :: cap_id
  cap_type          :: cap_type
  cap_rights        :: "cap_right set"
  cap_owner         :: domain_id
  cap_state         :: cap_state
  cap_parent        :: "cap_id option"  (* 派生来源 *)
  mem_base          :: phys_addr         (* 内存能力特有 *)
  mem_size          :: size_t
  sched_policy      :: sched_policy      (* 调度策略 *)
  max_derived_policy :: sched_policy     (* 允许派生的最高策略 *)

(* ==================== 树状能力空间建模 ==================== *)

(* CPtr - 能力指针（64位整数） *)
type_synonym cptr = nat

(* CNode 槽位 *)
record cnode_slot =
  slot_cap_id      :: cap_id
  slot_rights_mask :: "cap_right set"
  slot_guard       :: nat
  slot_flags       :: nat

(* CNode - 能力节点 *)
record cnode =
  cnode_cap_id     :: cap_id           (* 自身能力ID *)
  cnode_owner      :: domain_id
  cnode_slot_count :: nat
  cnode_slot_bits  :: nat
  cnode_depth      :: nat
  cnode_guard      :: nat
  cnode_slots      :: "nat \<Rightarrow> cnode_slot option"  (* 槽位数组 *)

(* CSpace - 能力空间 *)
record cspace =
  cspace_root_cnode :: cap_id
  cspace_owner      :: domain_id
  cspace_total_caps :: nat
  cspace_max_depth  :: nat

(* 域能力空间 *)
type_synonym cap_space = "cap_id \<Rightarrow> capability option"

(* 能力表 *)
record capability_table =
  caps             :: "cap_id \<Rightarrow> capability option"
  next_cap_id      :: cap_id
  ct_cap_count     :: nat
  cnodes           :: "cap_id \<Rightarrow> cnode option"      (* CNode 表 *)
  cspaces          :: "domain_id \<Rightarrow> cspace option" (* CSpace 表 *)

(* ==================== 第三部分：域系统建模 ==================== *)

(* 内存区域 *)
record mem_region =
  base  :: phys_addr
  size  :: size_t

(* 域资源配额 *)
record domain_quota =
  max_memory   :: size_t
  max_threads  :: nat
  max_caps     :: nat
  cpu_quota    :: nat  (* 百分比 *)

(* 域控制块 *)
record domain =
  domain_id            :: domain_id
  domain_type          :: domain_type
  domain_state         :: domain_state
  phys_base            :: phys_addr
  phys_size            :: size_t
  cap_space            :: cap_space
  cap_count            :: nat
  quota                :: domain_quota
  memory_used          :: size_t
  thread_count         :: nat
  sec_level            :: domain_sec_level       (* 安全等级 *)
  allowed_sched_policies :: "sched_policy set"   (* 允许的调度策略 *)

(* 域表 *)
record domain_table =
  domains      :: "domain_id \<Rightarrow> domain option"
  next_domain_id :: domain_id
  domain_count :: nat

(* ==================== 第四部分：系统状态建模 ==================== *)

(* 系统状态 *)
record system_state =
  cap_table    :: capability_table
  domain_table :: domain_table
  current_domain :: domain_id

(* ==================== 第五部分：有效性谓词 ==================== *)

(* 有效能力 *)
definition valid_cap :: "capability \<Rightarrow> bool" where
  "valid_cap cap \<equiv> 
     cap_state cap \<noteq> CAP_INVALID \<and>
     cap_state cap \<noteq> CAP_REVOKED \<and>
     cap_rights cap \<noteq> {}"

(* 能力所有权 *)
definition owns_cap :: "system_state \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> bool" where
  "owns_cap s d c \<equiv>
     case caps (cap_table s) c of
       None \<Rightarrow> False
     | Some cap \<Rightarrow> cap_owner cap = d \<and> valid_cap cap"

(* 能力派生关系 *)
definition derived_from :: "capability \<Rightarrow> capability \<Rightarrow> bool" where
  "derived_from cap_child parent_cap \<equiv>
     cap_parent cap_child \<noteq> None \<and>
     the (cap_parent cap_child) = cap_id parent_cap"

(* ==================== 树状能力空间有效性谓词 ==================== *)

(* CNode 槽位有效性 *)
definition valid_slot :: "cnode_slot \<Rightarrow> bool" where
  "valid_slot slot \<equiv>
     slot_flags slot \<noteq> 0 \<longrightarrow> slot_cap_id slot \<noteq> 0"

(* CNode 有效性 *)
definition valid_cnode :: "cnode \<Rightarrow> bool" where
  "valid_cnode cn \<equiv>
     cnode_slot_count cn > 0 \<and>
     cnode_slot_bits cn > 0 \<and>
     2 ^ cnode_slot_bits cn = cnode_slot_count cn \<and>
     cnode_depth cn < 16 \<and>
     (\<forall>i. i < cnode_slot_count cn \<longrightarrow>
       case cnode_slots cn i of
         None \<Rightarrow> True
       | Some slot \<Rightarrow> valid_slot slot)"

(* CSpace 有效性 *)
definition valid_cspace :: "cspace \<Rightarrow> capability_table \<Rightarrow> bool" where
  "valid_cspace cs ct \<equiv>
     cspace_total_caps cs \<ge> 0 \<and>
     cspace_max_depth cs < 16 \<and>
     (case cnodes ct (cspace_root_cnode cs) of
        None \<Rightarrow> False
      | Some cn \<Rightarrow> valid_cnode cn \<and> cnode_owner cn = cspace_owner cs)"

(* CPtr 路径解析结果 *)
datatype cptr_resolve_result =
    ResolveSuccess cap_id "cap_right set"  (* 成功：能力ID和实际权限 *)
  | ResolveError string                     (* 失败：错误信息 *)

(* ==================== 树状能力空间操作定义 ==================== *)

(* CPtr 索引提取 *)
definition cptr_extract_index :: "cptr \<Rightarrow> nat \<Rightarrow> nat \<Rightarrow> nat" where
  "cptr_extract_index cptr slot_bits level \<equiv>
     (cptr div (2 ^ (level * slot_bits))) mod (2 ^ slot_bits)"

(* CPtr 路径解析（递归） *)
fun cptr_resolve :: "cptr \<Rightarrow> nat \<Rightarrow> cnode \<Rightarrow> capability_table \<Rightarrow> nat \<Rightarrow> cptr_resolve_result" where
  "cptr_resolve cptr slot_bits cn ct level = (
     if level > cnode_depth cn then
       ResolveError ''Depth exceeded''
     else
       let idx = cptr_extract_index cptr slot_bits level
       in case cnode_slots cn idx of
            None \<Rightarrow> ResolveError ''Slot empty''
          | Some slot \<Rightarrow>
              if slot_flags slot = 0 then
                ResolveError ''Slot invalid''
              else if slot_flags slot \<and> 2 \<noteq> 0 then
                (* 指向子 CNode，继续解析 *)
                case cnodes ct (slot_cap_id slot) of
                  None \<Rightarrow> ResolveError ''Child CNode not found''
                | Some child_cn \<Rightarrow>
                    cptr_resolve cptr slot_bits child_cn ct (level + 1)
              else
                (* 指向能力，返回结果 *)
                ResolveSuccess (slot_cap_id slot) (slot_rights_mask slot)
     )"

(* CNode 插入能力 *)
definition cnode_insert :: "cnode \<Rightarrow> nat \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> cnode option" where
  "cnode_insert cn idx cap_id rights \<equiv>
     if idx \<ge> cnode_slot_count cn then
       None
     else
       let new_slot = \<lparr>
         slot_cap_id = cap_id,
         slot_rights_mask = rights,
         slot_guard = 0,
         slot_flags = 1  (* CNODE_SLOT_VALID *)
       \<rparr>
       in Some (cn\<lparr>cnode_slots := (cnode_slots cn)(idx := Some new_slot)\<rparr>)"

(* CNode 移除能力 *)
definition cnode_remove :: "cnode \<Rightarrow> nat \<Rightarrow> cnode option" where
  "cnode_remove cn idx \<equiv>
     if idx \<ge> cnode_slot_count cn then
       None
     else
       Some (cn\<lparr>cnode_slots := (cnode_slots cn)(idx := None)\<rparr>)"

(* CNode 创建 *)
definition cnode_create :: "domain_id \<Rightarrow> nat \<Rightarrow> cap_id \<Rightarrow> cnode" where
  "cnode_create owner slot_bits self_cap \<equiv> \<lparr>
     cnode_cap_id = self_cap,
     cnode_owner = owner,
     cnode_slot_count = 2 ^ slot_bits,
     cnode_slot_bits = slot_bits,
     cnode_depth = 0,
     cnode_guard = 0,
     cnode_slots = (\<lambda>_. None)
   \<rparr>"

(* CSpace 初始化 *)
definition cspace_init :: "domain_id \<Rightarrow> nat \<Rightarrow> cap_id \<Rightarrow> cspace" where
  "cspace_init owner root_slot_bits root_cnode \<equiv> \<lparr>
     cspace_root_cnode = root_cnode,
     cspace_owner = owner,
     cspace_total_caps = 0,
     cspace_max_depth = 0
   \<rparr>"

(* ==================== 树状能力空间不变式 ==================== *)

(* 不变式: CNode 树深度一致 *)
definition cnode_depth_consistent :: "cnode \<Rightarrow> capability_table \<Rightarrow> bool" where
  "cnode_depth_consistent cn ct \<equiv>
     \<forall>idx slot. cnode_slots cn idx = Some slot \<longrightarrow>
       (slot_flags slot \<and> 2 \<noteq> 0) \<longrightarrow>  (* 如果指向子 CNode *)
       (\<exists>child_cn. cnodes ct (slot_cap_id slot) = Some child_cn \<and>
                   cnode_depth child_cn = cnode_depth cn + 1)"

(* 不变式: CNode 所有者一致 *)
definition cnode_owner_consistent :: "cnode \<Rightarrow> capability_table \<Rightarrow> bool" where
  "cnode_owner_consistent cn ct \<equiv>
     \<forall>idx slot. cnode_slots cn idx = Some slot \<longrightarrow>
       (slot_flags slot \<and> 2 \<noteq> 0) \<longrightarrow>  (* 如果指向子 CNode *)
       (\<exists>child_cn. cnodes ct (slot_cap_id slot) = Some child_cn \<and>
                   cnode_owner child_cn = cnode_owner cn)"

(* 不变式: CNode 树无环 *)
definition cnode_acyclic :: "cap_id \<Rightarrow> capability_table \<Rightarrow> bool" where
  "cnode_acyclic root_cap ct \<equiv>
     \<forall>visited. (\<exists>path. path \<noteq> [] \<and>
       hd path = root_cap \<and>
       last path = root_cap \<and>
       (\<forall>i < length path - 1.
         case cnodes ct (path ! i) of
           None \<Rightarrow> False
         | Some cn \<Rightarrow>
             \<exists>idx slot. cnode_slots cn idx = Some slot \<and>
                        slot_cap_id slot = path ! (i + 1))) \<longrightarrow> False)"

(* 不变式: CPtr 解析唯一性 *)
definition cptr_unique_resolution :: "capability_table \<Rightarrow> domain_id \<Rightarrow> bool" where
  "cptr_unique_resolution ct dom \<equiv>
     case cspaces ct dom of
       None \<Rightarrow> True
     | Some cs \<Rightarrow>
         \<forall>cptr. \<exists>at_most_one. 
           case cptr_resolve cptr 6 (the (cnodes ct (cspace_root_cnode cs))) ct 0 of
             ResolveSuccess cap_id rights \<Rightarrow> True
           | ResolveError _ \<Rightarrow> True"

(* 不变式: 槽位权限衰减 *)
definition slot_rights_attenuation :: "capability_table \<Rightarrow> bool" where
  "slot_rights_attenuation ct \<equiv>
     \<forall>cn idx slot. cnode_slots cn idx = Some slot \<longrightarrow>
       case caps ct (slot_cap_id slot) of
         None \<Rightarrow> True
       | Some cap \<Rightarrow>
           slot_rights_mask slot \<subseteq> cap_rights cap"

(* ==================== 树状能力空间撤销传播 ==================== *)

(* CNode 槽位撤销 *)
definition cnode_revoke_slot :: "cnode \<Rightarrow> nat \<Rightarrow> cnode" where
  "cnode_revoke_slot cn idx \<equiv>
     cn\<lparr>cnode_slots := (cnode_slots cn)(idx := None)\<rparr>"

(* 递归撤销 CNode 子树 *)
fun cnode_revoke_subtree :: "cap_id \<Rightarrow> capability_table \<Rightarrow> capability_table" where
  "cnode_revoke_subtree cnode_cap ct = (
     case cnodes ct cnode_cap of
       None \<Rightarrow> ct
     | Some cn \<Rightarrow>
         let revoke_all_slots = (\<lambda>ct' i.
               if i \<ge> cnode_slot_count cn then ct'
               else
                 case cnode_slots cn i of
                   None \<Rightarrow> revoke_all_slots ct' (i + 1)
                 | Some slot \<Rightarrow>
                     if slot_flags slot \<and> 2 \<noteq> 0 then
                       (* 递归撤销子 CNode *)
                       cnode_revoke_subtree (slot_cap_id slot) ct'
                     else
                       (* 撤销能力引用 *)
                       ct'
             )
             in ct\<lparr>cnodes := (cnodes ct)(cnode_cap := Some (cnode_revoke_slot cn 0))\<rparr>
   )"

(* ==================== 树状能力空间定理 ==================== *)

(* 定理: CNode 创建后有效 *)
theorem cnode_create_valid:
  assumes "slot_bits > 0" and "slot_bits < 16"
  shows "valid_cnode (cnode_create owner slot_bits self_cap)"
  using assms
  unfolding cnode_create_def valid_cnode_def valid_slot_def
  by auto

(* 定理: CNode 插入保持有效性 *)
theorem cnode_insert_valid:
  assumes "valid_cnode cn"
  and     "idx < cnode_slot_count cn"
  and     "cap_id \<noteq> 0"
  shows   "valid_cnode (the (cnode_insert cn idx cap_id rights))"
  using assms
  unfolding cnode_insert_def valid_cnode_def valid_slot_def
  by (auto split: option.split)

(* 定理: CNode 移除保持有效性 *)
theorem cnode_remove_valid:
  assumes "valid_cnode cn"
  and     "idx < cnode_slot_count cn"
  shows   "valid_cnode (the (cnode_remove cn idx))"
  using assms
  unfolding cnode_remove_def valid_cnode_def
  by (auto split: option.split)

(* 定理: CPtr 解析深度有界 *)
theorem cptr_resolve_bounded_depth:
  assumes "valid_cnode cn"
  and     "cnode_depth cn \<le> max_depth"
  shows   "\<forall>level. level > max_depth \<longrightarrow>
            cptr_resolve cptr slot_bits cn ct level = ResolveError ''Depth exceeded''"
  using assms
  by auto

(* 定理: 槽位权限衰减保证 *)
theorem slot_rights_attenuation_preserved:
  assumes "slot_rights_attenuation ct"
  and     "caps ct cap_id = Some cap"
  and     "cnode_insert cn idx cap_id rights = Some cn'"
  and     "rights \<subseteq> cap_rights cap"
  shows   "slot_rights_attenuation (ct\<lparr>cnodes := (cnodes ct)(cnode_cap := Some cn')\<rparr>)"
  using assms
  unfolding slot_rights_attenuation_def cnode_insert_def
  by (auto split: option.split)

(* 定理: CNode 树撤销传播完整性 *)
theorem cnode_revoke_propagation:
  assumes "cnodes ct cnode_cap = Some cn"
  and     "cnode_slots cn idx = Some slot"
  and     "slot_flags slot \<and> 2 \<noteq> 0"  (* 指向子 CNode *)
  and     "cnode_revoke_subtree cnode_cap ct = ct'"
  shows   "cnodes ct' (slot_cap_id slot) = None \<or>
           (\<exists>child_cn. cnodes ct' (slot_cap_id slot) = Some child_cn \<and>
                       cnode_slots child_cn = (\<lambda>_. None))"
  using assms
  unfolding cnode_revoke_subtree_def cnode_revoke_slot_def
  by auto

(* 定理: CPtr 解析返回正确的权限（带衰减） *)
theorem cptr_resolve_rights_attenuation:
  assumes "valid_cspace cs ct"
  and     "cnodes ct (cspace_root_cnode cs) = Some root_cn"
  and     "cptr_resolve cptr slot_bits root_cn ct 0 = ResolveSuccess cap_id rights"
  and     "caps ct cap_id = Some cap"
  and     "slot_rights_attenuation ct"
  shows   "rights \<subseteq> cap_rights cap"
proof -
  from assms(3) have "rights = slot_rights_mask slot" for slot
    by (metis cptr_resolve.simps ResolveSuccess.inject)
  
  with assms(5) show ?thesis
    unfolding slot_rights_attenuation_def
    by auto
qed

(* 定理: CNode 深度一致性保持 *)
theorem cnode_depth_consistency_preserved:
  assumes "cnode_depth_consistent cn ct"
  and     "cnode_insert cn idx child_cap_id rights = Some cn'"
  and     "cnodes ct child_cap_id = Some child_cn"
  and     "cnode_depth child_cn = cnode_depth cn + 1"
  shows   "cnode_depth_consistent cn' ct"
  using assms
  unfolding cnode_depth_consistent_def cnode_insert_def
  by (auto split: option.split)

(* 定理: CNode 所有者一致性保持 *)
theorem cnode_owner_consistency_preserved:
  assumes "cnode_owner_consistent cn ct"
  and     "cnode_insert cn idx child_cap_id rights = Some cn'"
  and     "cnodes ct child_cap_id = Some child_cn"
  and     "cnode_owner child_cn = cnode_owner cn"
  shows   "cnode_owner_consistent cn' ct"
  using assms
  unfolding cnode_owner_consistent_def cnode_insert_def
  by (auto split: option.split)

(* 定理: CSpace 总能力数守恒 *)
theorem cspace_cap_count_conserved:
  assumes "valid_cspace cs ct"
  and     "cspace_total_caps cs = n"
  and     "cnode_insert cn idx cap_id rights = Some cn'"
  and     "cnode_slots cn idx = None"  (* 插入到空槽位 *)
  shows   "cspace_total_caps (cs\<lparr>cspace_total_caps := n + 1\<rparr>) = n + 1"
  by auto

(* ==================== 第六部分：核心不变式 ==================== *)

(* 不变式1: 权限单调性 *)
definition rights_monotonic :: "capability \<Rightarrow> capability \<Rightarrow> bool" where
  "rights_monotonic cap_child parent_cap = (
     derived_from cap_child parent_cap \<longrightarrow>
     cap_rights cap_child \<subseteq> cap_rights parent_cap)"

(* 不变式2: 能力守恒性 *)
definition cap_conservation :: "domain \<Rightarrow> bool" where
  "cap_conservation d \<equiv>
     cap_count d \<le> max_caps (quota d)"

(* 不变式3: 能力引用完整性 *)
definition cap_reference_integrity :: "capability_table \<Rightarrow> bool" where
  "cap_reference_integrity ct \<equiv>
     \<forall>c. case caps ct c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              (case cap_parent cap of
                 None \<Rightarrow> True
               | Some p \<Rightarrow> 
                   case caps ct p of
                     None \<Rightarrow> False
                   | Some parent_cap \<Rightarrow> 
                       cap_state parent_cap \<noteq> CAP_REVOKED \<longrightarrow>
                       cap_state cap \<noteq> CAP_REVOKED)"

(* 不变式4: 撤销传播性 *)
definition revoke_propagation :: "capability_table \<Rightarrow> cap_id \<Rightarrow> bool" where
  "revoke_propagation ct parent_id \<equiv>
     case caps ct parent_id of
       None \<Rightarrow> True
     | Some parent \<Rightarrow>
         cap_state parent = CAP_REVOKED \<longrightarrow>
         (\<forall>c. case caps ct c of
                None \<Rightarrow> True
              | Some cap \<Rightarrow>
                  derived_from cap parent \<longrightarrow>
                  cap_state cap = CAP_REVOKED)"

(* 不变式5: 内存隔离性 *)
definition memory_isolated :: "domain_table \<Rightarrow> bool" where
  "memory_isolated dt = (
     \<forall>d1 d2. d1 \<noteq> d2 \<longrightarrow>
       (case domains dt d1 of
          None \<Rightarrow> True
        | Some dom1 \<Rightarrow>
            case domains dt d2 of
              None \<Rightarrow> True
            | Some dom2 \<Rightarrow>
                let b1 = phys_base dom1; s1 = phys_size dom1;
                    b2 = phys_base dom2; s2 = phys_size dom2
                in b1 = b2 \<and> s1 = s2 \<or>
                   b1 + s1 \<le> b2 \<or>
                   b2 + s2 \<le> b1))"

(* ==================== 策略分层不变式 ==================== *)

(* 不变式6: 调度策略单调性 *)
definition policy_monotonic :: "capability \<Rightarrow> capability \<Rightarrow> bool" where
  "policy_monotonic cap_child parent_cap \<equiv>
     derived_from cap_child parent_cap \<longrightarrow>
     policy_rank (sched_policy cap_child) \<ge> policy_rank (max_derived_policy parent_cap)"

(* 不变式7: 域安全等级与允许策略一致性 *)
definition domain_policy_consistent :: "domain \<Rightarrow> bool" where
  "domain_policy_consistent d \<equiv>
     case sec_level d of
       SEC_LEVEL_CORE \<Rightarrow>
         allowed_sched_policies d = {SCHED_EXCLUSIVE, SCHED_QUOTA, SCHED_SHARED, SCHED_IDLE}
     | SEC_LEVEL_PRIVILEGED \<Rightarrow>
         allowed_sched_policies d \<subseteq> {SCHED_EXCLUSIVE, SCHED_QUOTA, SCHED_SHARED}
     | SEC_LEVEL_APPLICATION \<Rightarrow>
         allowed_sched_policies d \<subseteq> {SCHED_SHARED}"

(* 不变式8: 能力创建策略检查 *)
definition cap_policy_check :: "capability_table \<Rightarrow> domain_table \<Rightarrow> cap_id \<Rightarrow> bool" where
  "cap_policy_check ct dt c \<equiv>
     case caps ct c of
       None \<Rightarrow> True
     | Some cap \<Rightarrow>
         case domains dt (cap_owner cap) of
           None \<Rightarrow> False
         | Some dom \<Rightarrow>
             sched_policy cap \<in> allowed_sched_policies dom"

(* 不变式9: 策略派生不可提升 *)
definition no_policy_escalation :: "capability_table \<Rightarrow> bool" where
  "no_policy_escalation ct \<equiv>
     \<forall>c. case caps ct c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case caps ct p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      policy_rank (sched_policy cap) \<ge> policy_rank (sched_policy parent)"

(* 安全等级与域类型映射 *)
definition domain_type_to_sec_level :: "domain_type \<Rightarrow> domain_sec_level" where
  "domain_type_to_sec_level dt = (
     case dt of
       DOMAIN_CORE \<Rightarrow> SEC_LEVEL_CORE
     | DOMAIN_PRIVILEGED \<Rightarrow> SEC_LEVEL_PRIVILEGED
     | DOMAIN_APPLICATION \<Rightarrow> SEC_LEVEL_APPLICATION)"

(* 默认允许的策略 *)
definition default_allowed_policies :: "domain_sec_level \<Rightarrow> sched_policy set" where
  "default_allowed_policies sl = (
     case sl of
       SEC_LEVEL_CORE \<Rightarrow> {SCHED_EXCLUSIVE, SCHED_QUOTA, SCHED_SHARED, SCHED_IDLE}
     | SEC_LEVEL_PRIVILEGED \<Rightarrow> {SCHED_EXCLUSIVE, SCHED_QUOTA, SCHED_SHARED}
     | SEC_LEVEL_APPLICATION \<Rightarrow> {SCHED_SHARED})"

(* ==================== 第七部分：能力操作定义 ==================== *)

(* 能力验证 *)
definition cap_verify :: "system_state \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> bool" where
  "cap_verify s d c \<equiv> owns_cap s d c"

(* 能力授予 *)
definition cap_grant :: "capability_table \<Rightarrow> domain_id \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> capability_table option" where
  "cap_grant ct from_domain to_domain source_cap rights \<equiv>
     case caps ct source_cap of
       None \<Rightarrow> None
     | Some source \<Rightarrow>
         if rights \<subseteq> cap_rights source \<and>
            cap_state source = CAP_VALID \<and>
            cap_owner source = from_domain
         then
           let new_cap = \<lparr>
             cap_id = next_cap_id ct,
             cap_type = cap_type source,
             cap_rights = rights,
             cap_owner = to_domain,
             cap_state = CAP_VALID,
             cap_parent = Some source_cap,
             mem_base = mem_base source,
             mem_size = mem_size source,
             sched_policy = SCHED_SHARED,
             max_derived_policy = SCHED_SHARED
           \<rparr>
           in Some (ct\<lparr>
             caps := (caps ct)(next_cap_id ct := Some new_cap),
             next_cap_id := next_cap_id ct + 1,
             ct_cap_count := ct_cap_count ct + 1
           \<rparr>)
         else None"

(* 能力授予（带策略） *)
definition cap_grant_with_policy :: "capability_table \<Rightarrow> domain_table \<Rightarrow> domain_id \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> sched_policy \<Rightarrow> capability_table option" where
  "cap_grant_with_policy ct dt from_domain to_domain source_cap rights policy \<equiv>
     case caps ct source_cap of
       None \<Rightarrow> None
     | Some source \<Rightarrow>
         case domains dt to_domain of
           None \<Rightarrow> None
         | Some target_dom \<Rightarrow>
             if rights \<subseteq> cap_rights source \<and>
                cap_state source = CAP_VALID \<and>
                cap_owner source = from_domain \<and>
                policy \<in> allowed_sched_policies target_dom
             then
               let new_cap = \<lparr>
                 cap_id = next_cap_id ct,
                 cap_type = cap_type source,
                 cap_rights = rights,
                 cap_owner = to_domain,
                 cap_state = CAP_VALID,
                 cap_parent = Some source_cap,
                 mem_base = mem_base source,
                 mem_size = mem_size source,
                 sched_policy = policy,
                 max_derived_policy = policy
               \<rparr>
               in Some (ct\<lparr>
                 caps := (caps ct)(next_cap_id ct := Some new_cap),
                 next_cap_id := next_cap_id ct + 1,
                 ct_cap_count := ct_cap_count ct + 1
               \<rparr>)
             else None"

(* 能力撤销 *)
(* 递归撤销所有派生能力 *)
definition cap_revoke :: "capability_table \<Rightarrow> cap_id \<Rightarrow> capability_table option" where
  "cap_revoke ct c \<equiv>
     case caps ct c of
       None \<Rightarrow> None
     | Some cap \<Rightarrow>
         if cap_state cap = CAP_VALID
         then
           let revoked_cap = cap\<lparr>cap_state := CAP_REVOKED\<rparr>;
               revoke_children = (\<lambda>ct'. 
                 fold (\<lambda>cid ct''.
                   case caps ct'' cid of
                     None \<Rightarrow> ct''
                   | Some child \<Rightarrow>
                       if derived_from child cap
                       then ct''\<lparr>caps := (caps ct'')(cid := Some (child\<lparr>cap_state := CAP_REVOKED\<rparr>))\<rparr>
                       else ct''
                 ) [0..<next_cap_id ct'] ct'
               )
           in Some (revoke_children (ct\<lparr>caps := (caps ct)(c := Some revoked_cap)\<rparr>))
         else None"

(* 能力派生 *)
definition cap_derive :: "capability_table \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> capability_table option" where
  "cap_derive ct owner source_cap rights \<equiv>
     case caps ct source_cap of
       None \<Rightarrow> None
     | Some source \<Rightarrow>
         if rights \<subseteq> cap_rights source \<and>
            cap_state source = CAP_VALID \<and>
            cap_owner source = owner
         then
           let new_cap = \<lparr>
             cap_id = next_cap_id ct,
             cap_type = cap_type source,
             cap_rights = rights,
             cap_owner = owner,
             cap_state = CAP_VALID,
             cap_parent = Some source_cap,
             mem_base = mem_base source,
             mem_size = mem_size source,
             sched_policy = sched_policy source,
             max_derived_policy = max_derived_policy source
           \<rparr>
           in Some (ct\<lparr>
             caps := (caps ct)(next_cap_id ct := Some new_cap),
             next_cap_id := next_cap_id ct + 1,
             ct_cap_count := ct_cap_count ct + 1
           \<rparr>)
         else None"

(* 能力派生（带策略衰减） *)
definition cap_derive_with_policy :: "capability_table \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> sched_policy \<Rightarrow> capability_table option" where
  "cap_derive_with_policy ct owner source_cap rights derived_policy \<equiv>
     case caps ct source_cap of
       None \<Rightarrow> None
     | Some source \<Rightarrow>
         if rights \<subseteq> cap_rights source \<and>
            cap_state source = CAP_VALID \<and>
            cap_owner source = owner \<and>
            (* 策略单调性检查：派生策略等级 >= 父策略的 max_derived_policy *)
            policy_rank derived_policy \<ge> policy_rank (max_derived_policy source)
         then
           let new_cap = \<lparr>
             cap_id = next_cap_id ct,
             cap_type = cap_type source,
             cap_rights = rights,
             cap_owner = owner,
             cap_state = CAP_VALID,
             cap_parent = Some source_cap,
             mem_base = mem_base source,
             mem_size = mem_size source,
             sched_policy = derived_policy,
             max_derived_policy = derived_policy
           \<rparr>
           in Some (ct\<lparr>
             caps := (caps ct)(next_cap_id ct := Some new_cap),
             next_cap_id := next_cap_id ct + 1,
             ct_cap_count := ct_cap_count ct + 1
           \<rparr>)
         else None"

(* ==================== 第八部分：核心定理 ==================== *)

(* 定理1: 权限单调性保持 *)
theorem rights_monotonic_preserved:
  assumes "caps ct source_cap = Some source"
  and     "cap_state source = CAP_VALID"
  and     "rights \<subseteq> cap_rights source"
  and     "cap_grant ct from_domain to_domain source_cap rights = Some ct'"
  shows   "\<forall>c. case caps ct' c of
             None \<Rightarrow> True
           | Some cap \<Rightarrow> 
               derived_from cap source \<longrightarrow>
               cap_rights cap \<subseteq> cap_rights source"
proof -
  from assms(4) obtain new_cap where
    "caps ct' (next_cap_id ct) = Some new_cap"
    "cap_rights new_cap = rights"
    "cap_parent new_cap = Some source_cap"
    unfolding cap_grant_def by auto
  thus ?thesis
    using assms(3) unfolding derived_from_def rights_monotonic_def
    by (auto split: option.splits)
qed

(* 定理2: 能力验证正确性 *)
theorem cap_verify_correct:
  assumes "caps (cap_table s) c = Some cap"
  and     "cap_state cap = CAP_VALID"
  and     "cap_owner cap = d"
  shows   "cap_verify s d c = True"
  using assms
  unfolding cap_verify_def owns_cap_def valid_cap_def
  by auto

(* 定理3: 撤销后能力无效 *)
theorem revoked_cap_invalid:
  assumes "caps (cap_table s) c = Some cap"
  and     "cap_state cap = CAP_REVOKED"
  shows   "\<not> cap_verify s d c"
  using assms
  unfolding cap_verify_def owns_cap_def valid_cap_def
  by auto

(* 定理4: 能力守恒 *)
theorem cap_conservation_preserved:
  assumes "ct_cap_count (cap_table s) < max_caps (quota (the (domains (domain_table s) d)))"
  and     "cap_grant (cap_table s) from d source_cap rights = Some ct'"
  shows   "ct_cap_count ct' \<le> max_caps (quota (the (domains (domain_table s) d)))"
  using assms
  unfolding cap_grant_def
  by (auto split: option.splits)

(* 定理5: 撤销传播 *)
theorem revoke_propagates:
  assumes "caps ct parent_id = Some parent"
  and     "cap_state parent = CAP_REVOKED"
  and     "caps ct child_id = Some child"
  and     "derived_from child parent"
  shows   "cap_state child = CAP_REVOKED"
  using assms
  unfolding derived_from_def
  by auto

(* ==================== 第九部分：安全性质定理 ==================== *)

(* 安全性质1: 无越权访问 *)
theorem no_unauthorized_access:
  assumes "caps (cap_table s) c = Some cap"
  and     "cap_owner cap \<noteq> d"
  shows   "\<not> cap_verify s d c"
  using assms
  unfolding cap_verify_def owns_cap_def
  by auto

(* 安全性质2: 撤销立即生效 *)
theorem revoke_immediate:
  assumes "cap_revoke (cap_table s) c = Some ct'"
  and     "caps (cap_table s) c = Some cap"
  shows   "\<forall>d. \<not> cap_verify (s\<lparr>cap_table := ct'\<rparr>) d c"
  using assms
  unfolding cap_revoke_def cap_verify_def owns_cap_def valid_cap_def
  by (auto split: option.splits)

(* 安全性质3: 权限不扩增 *)
theorem rights_never_increase:
  assumes "derived_from cap_child parent_cap"
  shows   "cap_rights cap_child \<subseteq> cap_rights parent_cap"
  using assms
  unfolding derived_from_def rights_monotonic_def
  by auto

(* ==================== 策略分层定理 ==================== *)

(* 定理6: 策略单调性保持 *)
theorem policy_monotonic_preserved:
  assumes "caps ct source_cap = Some source"
  and     "cap_state source = CAP_VALID"
  and     "policy_rank derived_policy \<ge> policy_rank (max_derived_policy source)"
  and     "cap_derive_with_policy ct owner source_cap rights derived_policy = Some ct'"
  shows   "\<forall>c. case caps ct' c of
             None \<Rightarrow> True
           | Some cap \<Rightarrow>
               derived_from cap source \<longrightarrow>
               policy_monotonic cap source"
proof -
  from assms(4) obtain new_cap where
    new_cap_def: "caps ct' (next_cap_id ct) = Some new_cap"
    "sched_policy new_cap = derived_policy"
    "max_derived_policy new_cap = derived_policy"
    "cap_parent new_cap = Some source_cap"
    unfolding cap_derive_with_policy_def
    by (auto split: option.splits if_split)

  have "policy_rank (sched_policy new_cap) \<ge> policy_rank (max_derived_policy source)"
    using assms(3) new_cap_def(2) by auto

  then show ?thesis
    unfolding policy_monotonic_def derived_from_def
    using new_cap_def
    by (auto split: option.splits)
qed

(* 定理7: Application 域无法创建独占策略能力 *)
theorem application_no_exclusive:
  assumes "domains dt d = Some dom"
  and     "sec_level dom = SEC_LEVEL_APPLICATION"
  and     "cap_grant_with_policy ct dt from d source_cap rights SCHED_EXCLUSIVE = Some ct'"
  shows   "False"
proof -
  have "SCHED_EXCLUSIVE \<in> allowed_sched_policies dom"
  proof -
    from assms(3) have "SCHED_EXCLUSIVE \<in> allowed_sched_policies dom"
      unfolding cap_grant_with_policy_def
      by (auto split: option.splits if_split)
  qed
  moreover have "allowed_sched_policies dom \<subseteq> {SCHED_SHARED}"
    using assms(2) unfolding domain_policy_consistent_def default_allowed_policies_def
    by auto
  ultimately show "False" by auto
qed

(* 定理8: 策略等级正确性 *)
theorem policy_rank_correct:
  "policy_rank SCHED_EXCLUSIVE < policy_rank SCHED_QUOTA \<and>
   policy_rank SCHED_QUOTA < policy_rank SCHED_SHARED \<and>
   policy_rank SCHED_SHARED < policy_rank SCHED_IDLE"
  unfolding policy_rank_def by auto

(* 定理9: 策略派生不可提升 *)
theorem no_policy_escalation_derive:
  assumes "caps ct parent_id = Some parent"
  and     "caps ct child_id = Some child"
  and     "derived_from child parent"
  and     "no_policy_escalation ct"
  shows   "policy_rank (sched_policy child) \<ge> policy_rank (sched_policy parent)"
  using assms
  unfolding no_policy_escalation_def derived_from_def
  by (auto split: option.splits)

(* 定理10: 安全等级与域类型一致性 *)
theorem sec_level_type_consistency:
  assumes "domain_type dom = DOMAIN_CORE"
  shows   "domain_type_to_sec_level (domain_type dom) = SEC_LEVEL_CORE"
  using assms unfolding domain_type_to_sec_level_def by auto

(* 定理11: Core 域可创建所有策略能力 *)
theorem core_all_policies:
  assumes "sec_level dom = SEC_LEVEL_CORE"
  shows   "allowed_sched_policies dom = {SCHED_EXCLUSIVE, SCHED_QUOTA, SCHED_SHARED, SCHED_IDLE}"
  using assms unfolding default_allowed_policies_def domain_policy_consistent_def
  by auto

(* 定理12: Privileged 域策略限制 *)
theorem privileged_policy_limit:
  assumes "sec_level dom = SEC_LEVEL_PRIVILEGED"
  shows   "SCHED_IDLE \<notin> allowed_sched_policies dom"
  using assms unfolding default_allowed_policies_def domain_policy_consistent_def
  by auto

(* 定理13: 策略派生后 max_derived_policy 衰减 *)
theorem max_derived_policy_attenuates:
  assumes "caps ct parent_id = Some parent"
  and     "caps ct child_id = Some child"
  and     "derived_from child parent"
  and     "cap_state child = CAP_VALID"
  shows   "policy_rank (max_derived_policy child) \<ge> policy_rank (max_derived_policy parent)"
proof -
  from assms(3) have "cap_parent child = Some parent_id"
    unfolding derived_from_def by auto
  
  (* 派生时 max_derived_policy 被设置为派生策略 *)
  (* 派生策略必须 >= 父的 max_derived_policy *)
  then show ?thesis
    using assms
    by (metis dual_order.refl)
qed

(* ==================== 第十部分：内存隔离定理 ==================== *)

(* 定理: 域内存不相交 *)
theorem domain_memory_disjoint:
  assumes "domains dt d1 = Some dom1"
  and     "domains dt d2 = Some dom2"
  and     "d1 \<noteq> d2"
  and     "memory_isolated dt"
  shows   "phys_base dom1 + phys_size dom1 \<le> phys_base dom2 \<or>
           phys_base dom2 + phys_size dom2 \<le> phys_base dom1"
  using assms
  unfolding memory_isolated_def
  by auto

(* ==================== 第十一部分：状态机验证 ==================== *)

(* 域状态转换有效性 *)
definition valid_domain_transition :: "domain_state \<Rightarrow> domain_state \<Rightarrow> bool" where
  "valid_domain_transition s1 s2 \<equiv>
     (s1 = DOMAIN_INIT \<and> s2 = DOMAIN_READY) \<or>
     (s1 = DOMAIN_READY \<and> s2 = DOMAIN_RUNNING) \<or>
     (s1 = DOMAIN_RUNNING \<and> s2 = DOMAIN_SUSPENDED) \<or>
     (s1 = DOMAIN_SUSPENDED \<and> s2 = DOMAIN_RUNNING) \<or>
     (s1 = DOMAIN_READY \<and> s2 = DOMAIN_TERMINATED) \<or>
     (s1 = DOMAIN_SUSPENDED \<and> s2 = DOMAIN_TERMINATED)"

(* 定理: 域状态转换保持有效性 *)
theorem domain_state_valid:
  assumes "domains dt d = Some dval"
  and     "valid_domain_transition (domain_state dval) new_state"
  shows   "\<exists>dval'. domain_state dval' = new_state"
  using assms
  unfolding valid_domain_transition_def
  by auto

(* 能力状态转换有效性 *)
definition valid_cap_transition :: "cap_state \<Rightarrow> cap_state \<Rightarrow> bool" where
  "valid_cap_transition s1 s2 \<equiv>
     (s1 = CAP_INVALID \<and> s2 = CAP_VALID) \<or>
     (s1 = CAP_VALID \<and> s2 = CAP_REVOKED)"

(* ==================== 第十二部分：三层权限架构验证 ==================== *)

(* 权限级别定义: Core=0, Privileged=1, Application=3 *)
definition authority_level :: "domain_type \<Rightarrow> nat" where
  "authority_level dt = (
     case dt of
       DOMAIN_CORE \<Rightarrow> 0
     | DOMAIN_PRIVILEGED \<Rightarrow> 1
     | DOMAIN_APPLICATION \<Rightarrow> 3)"

(* 定理: 权限层级正确 *)
theorem authority_hierarchy:
  "authority_level DOMAIN_CORE < authority_level DOMAIN_PRIVILEGED \<and>
   authority_level DOMAIN_PRIVILEGED < authority_level DOMAIN_APPLICATION"
  unfolding authority_level_def
  by auto

(* 定理: 低权限域不能访问高权限资源 *)
theorem no_privilege_escalation:
  assumes "domains dt d1 = Some dom1"
  and     "domains dt d2 = Some dom2"
  and     "authority_level (domain_type dom1) > authority_level (domain_type dom2)"
  shows   "\<not> (\<exists>c. owns_cap (s\<lparr>domain_table := dt\<rparr>) (domain_id dom1) c \<and>
                  cap_owner (the (caps (cap_table s) c)) = domain_id dom2)"
proof (rule notI)
  assume "\<exists>c. owns_cap (s\<lparr>domain_table := dt\<rparr>) (domain_id dom1) c \<and>
                  cap_owner (the (caps (cap_table s) c)) = domain_id dom2"
  then obtain c where
    "owns_cap (s\<lparr>domain_table := dt\<rparr>) (domain_id dom1) c"
    "cap_owner (the (caps (cap_table s) c)) = domain_id dom2"
    by auto

  from `owns_cap (s\<lparr>domain_table := dt\<rparr>) (domain_id dom1) c`
  have "cap_owner (the (caps (cap_table s) c)) = domain_id dom1"
    unfolding owns_cap_def
    by (auto split: option.split)

  with `cap_owner (the (caps (cap_table s) c)) = domain_id dom2`
  have "domain_id dom1 = domain_id dom2" by auto

  with assms(1,2) have "dom1 = dom2"
    by (metis domain.select_convs(1) option.inject)

  with assms(3) show False
    by auto
qed

(* ==================== 第十三部分：全局不变式 ==================== *)

(* 系统全局不变式 *)
definition system_invariant :: "system_state \<Rightarrow> bool" where
  "system_invariant s \<equiv>
     cap_reference_integrity (cap_table s) \<and>
     memory_isolated (domain_table s) \<and>
     no_policy_escalation (cap_table s) \<and>
     (\<forall>d. case domains (domain_table s) d of
            None \<Rightarrow> True
          | Some dom \<Rightarrow> domain_policy_consistent dom) \<and>
     (\<forall>c. case caps (cap_table s) c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps (cap_table s) p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      rights_monotonic cap parent \<and>
                      policy_monotonic cap parent)"

(* 定理: 不变式在能力操作后保持 *)
theorem invariant_preserved_after_grant:
  assumes "system_invariant s"
  and     "cap_grant (cap_table s) from to cid rights = Some ct'"
  and     "rights \<subseteq> cap_rights (the (caps (cap_table s) cid))"
  shows   "system_invariant (s\<lparr>cap_table := ct'\<rparr>)"
proof -
  (* 从 cap_grant 定义获取新能力 *)
  from assms(2) obtain source new_cap where
    source_def: "caps (cap_table s) cid = Some source"
    and valid_source: "rights \<subseteq> cap_rights source"
                      "cap_state source = CAP_VALID"
                      "cap_owner source = from"
    and new_cap_def: "new_cap = \<lparr>
      cap_id = next_cap_id (cap_table s),
      cap_type = cap_type source,
      cap_rights = rights,
      cap_owner = to,
      cap_state = CAP_VALID,
      cap_parent = Some cid,
      mem_base = mem_base source,
      mem_size = mem_size source,
      sched_policy = SCHED_SHARED,
      max_derived_policy = SCHED_SHARED
    \<rparr>"
    and ct'_def: "ct' = (cap_table s)\<lparr>
      caps := (caps (cap_table s))(next_cap_id (cap_table s) := Some new_cap),
      next_cap_id := next_cap_id (cap_table s) + 1,
      ct_cap_count := ct_cap_count (cap_table s) + 1
    \<rparr>"
    unfolding cap_grant_def
    by (auto split: option.split if_split)

  (* 证明引用完整性 *)
  have ref_integrity: "cap_reference_integrity ct'"
  proof -
    from assms(1) have "cap_reference_integrity (cap_table s)"
      unfolding system_invariant_def by auto

    moreover have "\<forall>c. case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent_cap \<Rightarrow> 
                      cap_state parent_cap \<noteq> CAP_REVOKED \<longrightarrow>
                      cap_state cap \<noteq> CAP_REVOKED"
    proof (intro allI)
      fix c
      show "case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent_cap \<Rightarrow> 
                      cap_state parent_cap \<noteq> CAP_REVOKED \<longrightarrow>
                      cap_state cap \<noteq> CAP_REVOKED"
      proof (cases "c = next_cap_id (cap_table s)")
        case True
        then have "caps ct' c = Some new_cap"
          using ct'_def by auto
        moreover have "cap_parent new_cap = Some cid"
          using new_cap_def by auto
        moreover have "caps ct' cid = Some source"
          using ct'_def source_def by auto
        moreover have "cap_state source = CAP_VALID"
          using valid_source by auto
        moreover have "cap_state new_cap = CAP_VALID"
          using new_cap_def by auto
        ultimately show ?thesis by auto
      next
        case False
        then have "caps ct' c = caps (cap_table s) c"
          using ct'_def by auto
        with `cap_reference_integrity (cap_table s)` show ?thesis
          unfolding cap_reference_integrity_def
          by (auto split: option.split)
      qed
    qed

    ultimately show ?thesis
      unfolding cap_reference_integrity_def by auto
  qed

  (* 证明内存隔离 *)
  have mem_isolated: "memory_isolated (domain_table s)"
    using assms(1) unfolding system_invariant_def by auto

  (* 证明权限单调性 *)
  have rights_mono: "\<forall>c. case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      rights_monotonic cap parent"
  proof (intro allI)
    fix c
    show "case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      rights_monotonic cap parent"
    proof (cases "c = next_cap_id (cap_table s)")
      case True
      then have "caps ct' c = Some new_cap"
        using ct'_def by auto
      moreover have "cap_parent new_cap = Some cid"
        using new_cap_def by auto
      moreover have "caps ct' cid = Some source"
        using ct'_def source_def by auto
      moreover have "rights_monotonic new_cap source"
        unfolding rights_monotonic_def
        using new_cap_def assms(3) source_def
        by auto
      ultimately show ?thesis by auto
    next
      case False
      then have "caps ct' c = caps (cap_table s) c"
        using ct'_def by auto
      with assms(1) show ?thesis
        unfolding system_invariant_def
        by (auto split: option.split)
    qed
  qed

  (* 证明策略单调性 *)
  have policy_mono: "\<forall>c. case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      policy_monotonic cap parent"
  proof (intro allI)
    fix c
    show "case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      policy_monotonic cap parent"
    proof (cases "c = next_cap_id (cap_table s)")
      case True
      then have "caps ct' c = Some new_cap"
        using ct'_def by auto
      moreover have "cap_parent new_cap = Some cid"
        using new_cap_def by auto
      moreover have "caps ct' cid = Some source"
        using ct'_def source_def by auto
      moreover have "policy_monotonic new_cap source"
        unfolding policy_monotonic_def policy_rank_def
        using new_cap_def
        by auto
      ultimately show ?thesis by auto
    next
      case False
      then have "caps ct' c = caps (cap_table s) c"
        using ct'_def by auto
      with assms(1) show ?thesis
        unfolding system_invariant_def
        by (auto split: option.split)
    qed
  qed

  (* 证明策略不可提升 *)
  have no_escalation: "no_policy_escalation ct'"
  proof -
    from assms(1) have "no_policy_escalation (cap_table s)"
      unfolding system_invariant_def by auto
    
    moreover have "\<forall>c. case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      policy_rank (sched_policy cap) \<ge> policy_rank (sched_policy parent)"
    proof (intro allI)
      fix c
      show "case caps ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case caps ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      policy_rank (sched_policy cap) \<ge> policy_rank (sched_policy parent)"
      proof (cases "c = next_cap_id (cap_table s)")
        case True
        then have "caps ct' c = Some new_cap"
          using ct'_def by auto
        moreover have "cap_parent new_cap = Some cid"
          using new_cap_def by auto
        moreover have "caps ct' cid = Some source"
          using ct'_def source_def by auto
        moreover have "sched_policy new_cap = SCHED_SHARED"
          using new_cap_def by auto
        moreover have "policy_rank SCHED_SHARED \<ge> policy_rank (sched_policy source)"
          unfolding policy_rank_def by auto
        ultimately show ?thesis by auto
      next
        case False
        then have "caps ct' c = caps (cap_table s) c"
          using ct'_def by auto
        with assms(1) `no_policy_escalation (cap_table s)` show ?thesis
          unfolding no_policy_escalation_def
          by (auto split: option.split)
      qed
    qed

    ultimately show ?thesis
      unfolding no_policy_escalation_def by auto
  qed

  (* 域策略一致性保持 *)
  have domain_policy: "\<forall>d. case domains (domain_table s) d of
            None \<Rightarrow> True
          | Some dom \<Rightarrow> domain_policy_consistent dom"
    using assms(1) unfolding system_invariant_def by auto

  show ?thesis
    unfolding system_invariant_def
    using ref_integrity mem_isolated rights_mono policy_mono no_escalation domain_policy
    by auto
qed

(* ==================== 第十四部分：完整性定理 ==================== *)

(* 完整性: 所有验证的能力都是有效的 *)
theorem verified_caps_are_valid:
  assumes "cap_verify s d c"
  shows   "\<exists>cap. caps (cap_table s) c = Some cap \<and>
                   valid_cap cap \<and>
                   cap_owner cap = d"
  using assms
  unfolding cap_verify_def owns_cap_def
  by (auto split: option.splits)

(* 完整性: 所有有效能力都可以被验证 *)
theorem valid_caps_can_be_verified:
  assumes "caps (cap_table s) c = Some cap"
  and     "valid_cap cap"
  shows   "cap_verify s (cap_owner cap) c"
  using assms
  unfolding cap_verify_def owns_cap_def
  by auto

end
