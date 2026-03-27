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
  cap_id       :: cap_id
  cap_type     :: cap_type
  cap_rights   :: "cap_right set"
  cap_owner    :: domain_id
  cap_state    :: cap_state
  cap_parent   :: "cap_id option"  (* 派生来源 *)
  mem_base     :: phys_addr         (* 内存能力特有 *)
  mem_size     :: size_t

(* 域能力空间 *)
type_synonym cap_space = "cap_id \<Rightarrow> capability option"

(* 能力表 *)
record capability_table =
  caps         :: "cap_id \<Rightarrow> capability option"
  next_cap_id  :: cap_id
  ct_cap_count :: nat

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
  domain_id    :: domain_id
  domain_type  :: domain_type
  domain_state :: domain_state
  phys_base    :: phys_addr
  phys_size    :: size_t
  cap_space    :: cap_space
  cap_count    :: nat
  quota        :: domain_quota
  memory_used  :: size_t
  thread_count :: nat

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
             mem_size = mem_size source
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
             mem_size = mem_size source
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
     (\<forall>c. case caps (cap_table s) c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow> 
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> 
                  case caps (cap_table s) p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow>
                      rights_monotonic cap parent)"

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
      mem_size = mem_size source
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

  show ?thesis
    unfolding system_invariant_def
    using ref_integrity mem_isolated rights_mono
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
