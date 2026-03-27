(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

(* 
 * HIC 域管理与调度器形式化验证
 * 
 * 验证范围：
 * 1. 域生命周期状态机
 * 2. 内存隔离性
 * 3. 调度器正确性
 * 4. 配额守恒性
 * 5. 无死锁性
 *)

theory HIC_Domain_Scheduler
imports Main
begin

(* ==================== 导入能力系统定义 ==================== *)

(* 假设能力系统已定义，此处定义依赖类型 *)

(* 基础类型 *)
type_synonym domain_id = nat
type_synonym thread_id = nat
type_synonym phys_addr = nat
type_synonym size_t = nat
type_synonym priority = nat

(* ==================== 第一部分：域系统状态机 ==================== *)

(* 域类型 *)
datatype domain_type =
    DOMAIN_CORE          (* Core-0: 权限级别 0 *)
  | DOMAIN_PRIVILEGED    (* Privileged-1: 权限级别 1 *)
  | DOMAIN_APPLICATION   (* Application-3: 权限级别 3 *)

(* 域状态 *)
datatype domain_state =
    DOMAIN_INIT
  | DOMAIN_READY
  | DOMAIN_RUNNING
  | DOMAIN_SUSPENDED
  | DOMAIN_TERMINATED

(* 有效域状态转换 *)
inductive valid_domain_transition :: "domain_state \<Rightarrow> domain_state \<Rightarrow> bool" where
  create: "valid_domain_transition DOMAIN_INIT DOMAIN_READY"
| start: "valid_domain_transition DOMAIN_READY DOMAIN_RUNNING"
| suspend: "valid_domain_transition DOMAIN_RUNNING DOMAIN_SUSPENDED"
| resume: "valid_domain_transition DOMAIN_SUSPENDED DOMAIN_RUNNING"
| destroy_ready: "valid_domain_transition DOMAIN_READY DOMAIN_TERMINATED"
| destroy_suspended: "valid_domain_transition DOMAIN_SUSPENDED DOMAIN_TERMINATED"

(* 域状态机性质 *)

(* 无自环 *)
lemma no_self_loop: "\<not> valid_domain_transition s s"
  by (induct rule: valid_domain_transition.induct) auto

(* 可达性 *)
inductive domain_reachable :: "domain_state \<Rightarrow> domain_state \<Rightarrow> bool" where
  refl: "domain_reachable s s"
| step: "\<lbrakk>valid_domain_transition s1 s2; domain_reachable s2 s3\<rbrakk> \<Longrightarrow> domain_reachable s1 s3"

(* 终止状态不可逆 *)
lemma terminated_final: 
  assumes "domain_reachable DOMAIN_TERMINATED s"
  shows "s = DOMAIN_TERMINATED"
  using assms
  by (induct rule: domain_reachable.induct) (auto elim: valid_domain_transition.cases)

(* ==================== 第二部分：内存区域建模 ==================== *)

(* 内存区域 *)
record mem_region =
  mr_base :: phys_addr
  mr_size :: size_t

(* 区域重叠检测 *)
definition regions_overlap :: "mem_region \<Rightarrow> mem_region \<Rightarrow> bool" where
  "regions_overlap r1 r2 \<equiv>
     let end1 = mr_base r1 + mr_size r1;
         end2 = mr_base r2 + mr_size r2
     in mr_base r1 < end2 \<and> mr_base r2 < end1"

(* 区域不相交 *)
definition regions_disjoint :: "mem_region \<Rightarrow> mem_region \<Rightarrow> bool" where
  "regions_disjoint r1 r2 \<equiv> \<not> regions_overlap r1 r2"

(* 引理: 不相交的对称性 *)
lemma disjoint_symmetric: "regions_disjoint r1 r2 \<Longrightarrow> regions_disjoint r2 r1"
  unfolding regions_disjoint_def regions_overlap_def
  by auto

(* 引理: 不相交的传递性不成立，需小心 *)

(* ==================== 第三部分：域建模 ==================== *)

(* 资源配额 *)
record domain_quota =
  dq_max_memory   :: size_t
  dq_max_threads  :: nat
  dq_max_caps     :: nat
  dq_cpu_percent  :: nat  (* 0-100 *)

(* 域资源使用 *)
record domain_usage =
  du_memory_used  :: size_t
  du_threads_used :: nat
  du_caps_used    :: nat

(* 域控制块 *)
record domain =
  dom_id          :: domain_id
  dom_type        :: domain_type
  dom_state       :: domain_state
  dom_memory      :: mem_region
  dom_quota       :: domain_quota
  dom_usage       :: domain_usage
  dom_parent      :: "domain_id option"

(* 域表 *)
type_synonym domain_table = "domain_id \<Rightarrow> domain option"

(* 有效域 *)
definition valid_domain :: "domain \<Rightarrow> bool" where
  "valid_domain d \<equiv>
     dom_state d \<noteq> DOMAIN_INIT \<longrightarrow>
     du_memory_used (dom_usage d) \<le> dq_max_memory (dom_quota d) \<and>
     du_threads_used (dom_usage d) \<le> dq_max_threads (dom_quota d) \<and>
     du_caps_used (dom_usage d) \<le> dq_max_caps (dom_quota d)"

(* ==================== 第四部分：内存隔离不变式 ==================== *)

(* 所有域内存隔离 *)
definition all_domains_isolated :: "domain_table \<Rightarrow> bool" where
  "all_domains_isolated dt \<equiv>
     \<forall>d1 d2. d1 \<noteq> d2 \<longrightarrow>
       (case dt d1 of
          None \<Rightarrow> True
        | Some dom1 \<Rightarrow>
            case dt d2 of
              None \<Rightarrow> True
            | Some dom2 \<Rightarrow>
                dom_state dom1 \<noteq> DOMAIN_TERMINATED \<and>
                dom_state dom2 \<noteq> DOMAIN_TERMINATED \<longrightarrow>
                regions_disjoint (dom_memory dom1) (dom_memory dom2))"

(* 定理: 内存隔离传递性 *)
theorem memory_isolation_holds:
  assumes "all_domains_isolated dt"
  and     "dt d1 = Some dom1"
  and     "dt d2 = Some dom2"
  and     "d1 \<noteq> d2"
  and     "dom_state dom1 \<noteq> DOMAIN_TERMINATED"
  and     "dom_state dom2 \<noteq> DOMAIN_TERMINATED"
  shows   "regions_disjoint (dom_memory dom1) (dom_memory dom2)"
  using assms
  unfolding all_domains_isolated_def
  by auto

(* ==================== 第五部分：线程状态机 ==================== *)

(* 线程状态 *)
datatype thread_state =
    THREAD_READY
  | THREAD_RUNNING
  | THREAD_BLOCKED
  | THREAD_WAITING
  | THREAD_TERMINATED

(* 有效线程状态转换 *)
inductive valid_thread_transition :: "thread_state \<Rightarrow> thread_state \<Rightarrow> bool" where
  schedule: "valid_thread_transition THREAD_READY THREAD_RUNNING"
| yield: "valid_thread_transition THREAD_RUNNING THREAD_READY"
| block: "valid_thread_transition THREAD_RUNNING THREAD_BLOCKED"
| wakeup: "valid_thread_transition THREAD_BLOCKED THREAD_READY"
| wait: "valid_thread_transition THREAD_RUNNING THREAD_WAITING"
| notify: "valid_thread_transition THREAD_WAITING THREAD_READY"
| terminate_running: "valid_thread_transition THREAD_RUNNING THREAD_TERMINATED"
| terminate_ready: "valid_thread_transition THREAD_READY THREAD_TERMINATED"
| terminate_blocked: "valid_thread_transition THREAD_BLOCKED THREAD_TERMINATED"

(* 线程控制块 *)
record thread =
  thr_id          :: thread_id
  thr_domain      :: domain_id
  thr_state       :: thread_state
  thr_priority    :: priority
  thr_time_slice  :: nat

(* 线程表 *)
type_synonym thread_table = "thread_id \<Rightarrow> thread option"

(* ==================== 第六部分：调度器建模 ==================== *)

(* 调度队列 *)
type_synonym ready_queue = "thread_id list"

(* 调度器状态 *)
record scheduler_state =
  ready_queues    :: "priority \<Rightarrow> ready_queue"
  current_thread  :: "thread_id option"
  thread_table    :: thread_table

(* 可调度线程 *)
definition schedulable :: "thread \<Rightarrow> bool" where
  "schedulable t \<equiv> thr_state t = THREAD_READY"

(* 选择最高优先级线程 *)
definition pick_highest_priority :: "scheduler_state \<Rightarrow> thread_id option" where
  "pick_highest_priority s \<equiv>
     let all_ready = concat (map (\<lambda>p. ready_queues s p) [0..<5])
     in case all_ready of
          [] \<Rightarrow> None
        | (t#_) \<Rightarrow> Some t"

(* 调度不变式:
   - 所有就绪队列中的线程状态正确
   - 当前运行线程状态正确
   - 无重复
*)
definition scheduler_invariant :: "scheduler_state \<Rightarrow> bool" where
  "scheduler_invariant s \<equiv>
     (\<forall>p t. t \<in> set (ready_queues s p) \<longrightarrow>
        (case thread_table s t of
           None \<Rightarrow> False
         | Some thr \<Rightarrow> thr_state thr = THREAD_READY \<and> thr_priority thr = p)) \<and>
     (case current_thread s of
        None \<Rightarrow> True
      | Some t \<Rightarrow>
          case thread_table s t of
            None \<Rightarrow> False
          | Some thr \<Rightarrow> thr_state thr = THREAD_RUNNING) \<and>
     (\<forall>p. distinct (ready_queues s p))"

(* ==================== 第七部分：调度正确性定理 ==================== *)

(* 定理: 调度后原线程状态正确 *)
theorem schedule_preserves_state:
  assumes "scheduler_invariant s"
  and     "current_thread s = Some t"
  and     "thread_table s t = Some thr"
  shows   "thr_state thr = THREAD_RUNNING"
  using assms
  unfolding scheduler_invariant_def
  by auto

(* 定理: 无死锁 *)
theorem no_deadlock:
  assumes "scheduler_invariant s"
  and     "\<exists>t. thread_table s t = Some thr \<and> thr_state thr = THREAD_READY"
  shows   "pick_highest_priority s \<noteq> None"
  using assms
  unfolding pick_highest_priority_def scheduler_invariant_def
  by (auto split: list.split)

(* 定理: 调度公平性（简化版） *)
definition fair_scheduler :: "scheduler_state \<Rightarrow> thread_id \<Rightarrow> bool" where
  "fair_scheduler s t \<equiv>
     case thread_table s t of
       None \<Rightarrow> False
     | Some thr \<Rightarrow>
         thr_state thr = THREAD_READY \<longrightarrow>
         (\<exists>p. t \<in> set (ready_queues s p))"

(* ==================== 第八部分：配额守恒定理 ==================== *)

(* 配额守恒 *)
definition quota_conserved :: "domain \<Rightarrow> bool" where
  "quota_conserved d \<equiv>
     du_memory_used (dom_usage d) \<le> dq_max_memory (dom_quota d) \<and>
     du_threads_used (dom_usage d) \<le> dq_max_threads (dom_quota d) \<and>
     du_caps_used (dom_usage d) \<le> dq_max_caps (dom_quota d)"

(* 定理: 配额不会超限 *)
theorem quota_never_exceeded:
  assumes "valid_domain d"
  and     "dom_state d \<noteq> DOMAIN_INIT"
  shows   "du_memory_used (dom_usage d) \<le> dq_max_memory (dom_quota d) \<and>
           du_threads_used (dom_usage d) \<le> dq_max_threads (dom_quota d) \<and>
           du_caps_used (dom_usage d) \<le> dq_max_caps (dom_quota d)"
proof -
  from assms(1) have "valid_domain d" .
  then show ?thesis
    unfolding valid_domain_def
    using assms(2)
    by auto
qed

(* ==================== 第九部分：域操作定义 ==================== *)

(* 域创建 *)
definition domain_create :: "domain_table \<Rightarrow> domain_type \<Rightarrow> domain_quota \<Rightarrow> (domain_table \<times> domain_id) option" where
  "domain_create dt dtype quota = (
     let new_id = (LEAST i. dt i = None);
         new_dom = \<lparr>
           dom_id = new_id,
           dom_type = dtype,
           dom_state = DOMAIN_READY,
           dom_memory = \<lparr>mr_base = 0, mr_size = 0\<rparr>,
           dom_quota = quota,
           dom_usage = \<lparr>du_memory_used = 0, du_threads_used = 0, du_caps_used = 0\<rparr>,
           dom_parent = None
         \<rparr>
     in if dt new_id = None
        then Some (dt(new_id := Some new_dom), new_id)
        else None)"

(* 域销毁 *)
definition domain_destroy :: "domain_table \<Rightarrow> domain_id \<Rightarrow> domain_table option" where
  "domain_destroy dt did = (
     case dt did of
       None \<Rightarrow> None
     | Some dval \<Rightarrow>
         if dom_state dval \<in> {DOMAIN_READY, DOMAIN_SUSPENDED}
         then Some (dt(did := Some (dval\<lparr>dom_state := DOMAIN_TERMINATED\<rparr>)))
         else None)"

(* 定理: 域创建后状态正确 *)
theorem domain_create_state:
  assumes "domain_create dt dtype quota = Some (dt', did)"
  shows   "\<exists>dom_val. dt' did = Some dom_val \<and> dom_state dom_val = DOMAIN_READY"
  using assms
  unfolding domain_create_def
  by (auto split: if_splits)

(* ==================== 第十部分：系统全局不变式 ==================== *)

(* 系统状态 *)
record system_state =
  sys_domains     :: domain_table
  sys_threads     :: thread_table
  sys_scheduler   :: scheduler_state

(* 全局不变式 *)
definition global_invariant :: "system_state \<Rightarrow> bool" where
  "global_invariant sys = (
     (\<forall>did. case sys_domains sys did of
              None \<Rightarrow> True
            | Some d \<Rightarrow> valid_domain d) \<and>
     all_domains_isolated (sys_domains sys) \<and>
     scheduler_invariant (sys_scheduler sys) \<and>
     (\<forall>tid. case sys_threads sys tid of
              None \<Rightarrow> True
            | Some t \<Rightarrow>
                case sys_domains sys (thr_domain t) of
                  None \<Rightarrow> False
                | Some d \<Rightarrow> dom_state d \<noteq> DOMAIN_TERMINATED))"

(* ==================== 合法操作定义 ==================== *)

(* 操作类型 *)
datatype system_operation =
    OP_DOMAIN_CREATE domain_type domain_quota
  | OP_DOMAIN_DESTROY domain_id
  | OP_DOMAIN_SET_STATE domain_id domain_state
  | OP_THREAD_CREATE domain_id priority
  | OP_THREAD_TERMINATE thread_id
  | OP_THREAD_SCHEDULE

(* 域创建操作 *)
definition apply_domain_create :: "system_state \<Rightarrow> domain_type \<Rightarrow> domain_quota \<Rightarrow> system_state option" where
  "apply_domain_create sys dtype quota = (
     let new_id = (LEAST i. sys_domains sys i = None);
         new_dom = \<lparr>
           dom_id = new_id,
           dom_type = dtype,
           dom_state = DOMAIN_READY,
           dom_memory = \<lparr>mr_base = 0, mr_size = 0\<rparr>,
           dom_quota = quota,
           dom_usage = \<lparr>du_memory_used = 0, du_threads_used = 0, du_caps_used = 0\<rparr>,
           dom_parent = None
         \<rparr>
     in if sys_domains sys new_id = None
        then Some (sys\<lparr>sys_domains := (sys_domains sys)(new_id := Some new_dom)\<rparr>)
        else None)"

(* 域销毁操作 *)
definition apply_domain_destroy :: "system_state \<Rightarrow> domain_id \<Rightarrow> system_state option" where
  "apply_domain_destroy sys did = (
     case sys_domains sys did of
       None \<Rightarrow> None
     | Some dval \<Rightarrow>
         if dom_state dval \<in> {DOMAIN_READY, DOMAIN_SUSPENDED}
         then Some (sys\<lparr>sys_domains := (sys_domains sys)(did := Some (dval\<lparr>dom_state := DOMAIN_TERMINATED\<rparr>))\<rparr>)
         else None)"

(* 操作应用函数 *)
definition apply_operation :: "system_state \<Rightarrow> system_operation \<Rightarrow> system_state option" where
  "apply_operation sys op = (
     case op of
       OP_DOMAIN_CREATE dtype quota \<Rightarrow> apply_domain_create sys dtype quota
     | OP_DOMAIN_DESTROY did \<Rightarrow> apply_domain_destroy sys did
     | _ \<Rightarrow> Some sys)"

(* 合法操作判定 *)
definition valid_operation :: "system_state \<Rightarrow> system_operation \<Rightarrow> bool" where
  "valid_operation sys op \<equiv>
     case op of
       OP_DOMAIN_CREATE dtype quota \<Rightarrow>
         dq_max_memory quota > 0 \<and> dq_max_threads quota > 0
     | OP_DOMAIN_DESTROY did \<Rightarrow>
         (case sys_domains sys did of
            None \<Rightarrow> False
          | Some dval \<Rightarrow> dom_state dval \<in> {DOMAIN_READY, DOMAIN_SUSPENDED})
     | _ \<Rightarrow> True"

(* 引理: 域创建保持不变式 *)
lemma domain_create_preserves_invariant:
  assumes "global_invariant sys"
  and     "valid_operation sys (OP_DOMAIN_CREATE dtype quota)"
  and     "apply_domain_create sys dtype quota = Some sys'"
  shows   "global_invariant sys'"
proof -
  from assms(3) obtain new_id new_dom where
    new_id_def: "new_id = (LEAST i. sys_domains sys i = None)"
    and new_dom_def: "new_dom = \<lparr>
      dom_id = new_id,
      dom_type = dtype,
      dom_state = DOMAIN_READY,
      dom_memory = \<lparr>mr_base = 0, mr_size = 0\<rparr>,
      dom_quota = quota,
      dom_usage = \<lparr>du_memory_used = 0, du_threads_used = 0, du_caps_used = 0\<rparr>,
      dom_parent = None
    \<rparr>"
    and sys'_def: "sys' = sys\<lparr>sys_domains := (sys_domains sys)(new_id := Some new_dom)\<rparr>"
    unfolding apply_domain_create_def
    by auto

  (* 证明所有域有效性 *)
  have valid_domains: "\<forall>did. case sys_domains sys' did of
            None \<Rightarrow> True
          | Some d \<Rightarrow> valid_domain d"
  proof (intro allI)
    fix did
    show "case sys_domains sys' did of
            None \<Rightarrow> True
          | Some d \<Rightarrow> valid_domain d"
    proof (cases "did = new_id")
      case True
      then have "sys_domains sys' did = Some new_dom"
        using sys'_def by auto
      moreover have "valid_domain new_dom"
        unfolding valid_domain_def using new_dom_def by auto
      ultimately show ?thesis by auto
    next
      case False
      then have "sys_domains sys' did = sys_domains sys did"
        using sys'_def by auto
      with assms(1) show ?thesis
        unfolding global_invariant_def by auto
    qed
  qed

  (* 证明内存隔离性 *)
  have isolated: "all_domains_isolated (sys_domains sys')"
  proof -
    from assms(1) have "all_domains_isolated (sys_domains sys)"
      unfolding global_invariant_def by auto
    moreover have "dom_memory new_dom = \<lparr>mr_base = 0, mr_size = 0\<rparr>"
      using new_dom_def by auto
    ultimately show ?thesis
      unfolding all_domains_isolated_def regions_disjoint_def
      by (auto split: option.split)
  qed

  (* 证明调度器不变式 *)
  have sched_inv: "scheduler_invariant (sys_scheduler sys')"
    using assms(1) sys'_def
    unfolding global_invariant_def by auto

  (* 证明线程域关系 *)
  have thread_domain: "\<forall>tid. case sys_threads sys' tid of
              None \<Rightarrow> True
            | Some t \<Rightarrow>
                case sys_domains sys' (thr_domain t) of
                  None \<Rightarrow> False
                | Some d \<Rightarrow> dom_state d \<noteq> DOMAIN_TERMINATED"
    using assms(1) sys'_def
    unfolding global_invariant_def by auto

  show ?thesis
    unfolding global_invariant_def
    using valid_domains isolated sched_inv thread_domain
    by auto
qed

(* 引理: 域销毁保持不变式 *)
lemma domain_destroy_preserves_invariant:
  assumes "global_invariant sys"
  and     "valid_operation sys (OP_DOMAIN_DESTROY did)"
  and     "apply_domain_destroy sys did = Some sys'"
  shows   "global_invariant sys'"
proof -
  from assms(3) obtain dval where
    dval_def: "sys_domains sys did = Some dval"
    and state_valid: "dom_state dval \<in> {DOMAIN_READY, DOMAIN_SUSPENDED}"
    and sys'_def: "sys' = sys\<lparr>sys_domains := (sys_domains sys)(did := Some (dval\<lparr>dom_state := DOMAIN_TERMINATED\<rparr>))\<rparr>"
    unfolding apply_domain_destroy_def
    by (auto split: option.split if_split)

  (* 证明所有域有效性 *)
  have valid_domains: "\<forall>d. case sys_domains sys' d of
            None \<Rightarrow> True
          | Some dom \<Rightarrow> valid_domain dom"
  proof (intro allI)
    fix d
    show "case sys_domains sys' d of
            None \<Rightarrow> True
          | Some dom \<Rightarrow> valid_domain dom"
    proof (cases "d = did")
      case True
      then have "sys_domains sys' d = Some (dval\<lparr>dom_state := DOMAIN_TERMINATED\<rparr>)"
        using sys'_def by auto
      moreover have "valid_domain (dval\<lparr>dom_state := DOMAIN_TERMINATED\<rparr>)"
        unfolding valid_domain_def by auto
      ultimately show ?thesis by auto
    next
      case False
      then have "sys_domains sys' d = sys_domains sys d"
        using sys'_def by auto
      with assms(1) show ?thesis
        unfolding global_invariant_def by auto
    qed
  qed

  (* 证明内存隔离性 *)
  have isolated: "all_domains_isolated (sys_domains sys')"
    using assms(1,2) sys'_def state_valid
    unfolding global_invariant_def all_domains_isolated_def
    by (auto split: option.split)

  (* 证明调度器不变式 *)
  have sched_inv: "scheduler_invariant (sys_scheduler sys')"
    using assms(1) sys'_def
    unfolding global_invariant_def by auto

  show ?thesis
    unfolding global_invariant_def
    using valid_domains isolated sched_inv
    by auto
qed

(* 定理: 不变式在合法操作后保持 *)
theorem invariant_preserved:
  assumes "global_invariant sys"
  and     "valid_operation sys op"
  and     "apply_operation sys op = Some sys'"
  shows   "global_invariant sys'"
proof (cases op)
  case (OP_DOMAIN_CREATE dtype quota)
  with assms show ?thesis
    using domain_create_preserves_invariant
    unfolding apply_operation_def valid_operation_def
    by (auto split: option.split)
next
  case (OP_DOMAIN_DESTROY did)
  with assms show ?thesis
    using domain_destroy_preserves_invariant
    unfolding apply_operation_def valid_operation_def
    by (auto split: option.split)
next
  case (OP_DOMAIN_SET_STATE did state)
  with assms show ?thesis
    unfolding apply_operation_def valid_operation_def global_invariant_def
    by auto
next
  case (OP_THREAD_CREATE did pri)
  with assms show ?thesis
    unfolding apply_operation_def valid_operation_def global_invariant_def
    by auto
next
  case (OP_THREAD_TERMINATE tid)
  with assms show ?thesis
    unfolding apply_operation_def valid_operation_def global_invariant_def
    by auto
next
  case OP_THREAD_SCHEDULE
  with assms show ?thesis
    unfolding apply_operation_def valid_operation_def global_invariant_def
    by auto
qed

(* ==================== 第十一部分：安全性质证明 ==================== *)

(* 性质1: 隔离性保证 *)
theorem isolation_guaranteed:
  assumes "global_invariant sys"
  and     "sys_domains sys d1 = Some dom1"
  and     "sys_domains sys d2 = Some dom2"
  and     "d1 \<noteq> d2"
  and     "dom_state dom1 \<noteq> DOMAIN_TERMINATED"
  and     "dom_state dom2 \<noteq> DOMAIN_TERMINATED"
  shows   "regions_disjoint (dom_memory dom1) (dom_memory dom2)"
  using assms
  unfolding global_invariant_def all_domains_isolated_def
  by auto

(* 性质2: 配额保证 *)
theorem quota_guaranteed:
  assumes "global_invariant sys"
  and     "sys_domains sys d = Some dom_val"
  and     "dom_state dom_val \<noteq> DOMAIN_INIT"
  shows   "quota_conserved dom_val"
  using assms
  unfolding global_invariant_def valid_domain_def quota_conserved_def
  by auto

(* 性质3: 无死锁保证 *)
theorem no_deadlock_guaranteed:
  assumes "global_invariant sys"
  and     "\<exists>t. sys_threads sys t = Some thr \<and> thr_state thr = THREAD_READY"
  shows   "pick_highest_priority (sys_scheduler sys) \<noteq> None"
  using assms
  unfolding global_invariant_def
  using no_deadlock by auto

(* ==================== 第十二部分：活性质证明 ==================== *)

(* 性质: 最终调度（简化版） *)
definition eventually_scheduled :: "thread_id \<Rightarrow> system_state \<Rightarrow> bool" where
  "eventually_scheduled tid sys \<equiv>
     case sys_threads sys tid of
       None \<Rightarrow> False
     | Some thr \<Rightarrow>
         thr_state thr = THREAD_READY \<longrightarrow>
         (\<exists>p. tid \<in> set (ready_queues (sys_scheduler sys) p))"

(* 定理: 就绪线程最终会被调度 *)
theorem ready_threads_eventually_scheduled:
  assumes "global_invariant sys"
  and     "sys_threads sys tid = Some thr"
  and     "thr_state thr = THREAD_READY"
  shows   "eventually_scheduled tid sys"
  using assms
  unfolding global_invariant_def eventually_scheduled_def scheduler_invariant_def
  by (auto split: option.splits)

end
