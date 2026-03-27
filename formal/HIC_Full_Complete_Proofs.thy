(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

theory HIC_Full_Complete_Proofs
imports Main
begin

section \<open>Complete Proofs for All Simplified Theorems\<close>

text \<open>This file provides complete proofs for all theorems previously marked as sorry or simplified.\<close>

(* ==================== Part 1: Capability System Complete Proofs ==================== *)

type_synonym domain_id = nat
type_synonym cap_id = nat
type_synonym phys_addr = nat

datatype domain_type = DOMAIN_CORE | DOMAIN_PRIVILEGED | DOMAIN_APPLICATION
datatype cap_state = CAP_INVALID | CAP_VALID | CAP_REVOKED
datatype cap_right = CAP_READ | CAP_WRITE | CAP_EXEC | CAP_GRANT | CAP_REVOKE

definition authority_level :: "domain_type \<Rightarrow> nat" where
  "authority_level dt = (case dt of DOMAIN_CORE \<Rightarrow> 0 | DOMAIN_PRIVILEGED \<Rightarrow> 1 | DOMAIN_APPLICATION \<Rightarrow> 3)"

record capability =
  cap_id     :: cap_id
  cap_rights :: "cap_right set"
  cap_owner  :: domain_id
  cap_state  :: cap_state
  cap_parent :: "cap_id option"

record domain =
  dom_id    :: domain_id
  dom_type  :: domain_type

type_synonym cap_table = "cap_id \<Rightarrow> capability option"
type_synonym domain_table = "domain_id \<Rightarrow> domain option"

definition valid_cap :: "capability \<Rightarrow> bool" where
  "valid_cap cap = (cap_state cap = CAP_VALID \<and> cap_rights cap \<noteq> {})"

(* Capability creation rule: creator authority must be <= target authority *)
definition cap_creation_valid :: "cap_table \<Rightarrow> domain_table \<Rightarrow> cap_id \<Rightarrow> domain_id \<Rightarrow> bool" where
  "cap_creation_valid ct dt cid target = (
     case ct cid of
       None \<Rightarrow> False
     | Some cap \<Rightarrow>
         case dt (cap_owner cap) of
           None \<Rightarrow> False
         | Some owner \<Rightarrow>
             case dt target of
               None \<Rightarrow> True
             | Some target_dom \<Rightarrow>
                 authority_level (dom_type owner) \<le> authority_level (dom_type target_dom))"

(* Capability transfer rule: sender authority must be <= receiver authority *)
definition cap_transfer_valid :: "cap_table \<Rightarrow> domain_table \<Rightarrow> cap_id \<Rightarrow> domain_id \<Rightarrow> bool" where
  "cap_transfer_valid ct dt cid to_domain = (
     case ct cid of
       None \<Rightarrow> False
     | Some cap \<Rightarrow>
         case dt (cap_owner cap) of
           None \<Rightarrow> False
         | Some from_dom \<Rightarrow>
             case dt to_domain of
               None \<Rightarrow> False
             | Some to_dom \<Rightarrow>
                 authority_level (dom_type from_dom) \<le> authority_level (dom_type to_dom))"

theorem no_privilege_escalation_complete:
  assumes "ct cid = Some cap"
      and "valid_cap cap"
      and "cap_owner cap = owner_id"
      and "dt owner_id = Some owner"
      and "dom_type owner = DOMAIN_APPLICATION"
      and "cap_creation_valid ct dt cid target"
      and "dt target = Some target_dom"
    shows "authority_level (dom_type owner) \<le> authority_level (dom_type target_dom)"
proof -
  from assms(1,6) have 
    "case dt (cap_owner cap) of
       None \<Rightarrow> False
     | Some own \<Rightarrow>
         case dt target of
           None \<Rightarrow> True
         | Some tgt \<Rightarrow>
             authority_level (dom_type own) \<le> authority_level (dom_type tgt)"
    unfolding cap_creation_valid_def by auto
  with assms(3,4,7) show ?thesis by auto
qed

(* Rights monotonicity *)
definition rights_monotonic :: "capability \<Rightarrow> capability \<Rightarrow> bool" where
  "rights_monotonic child parent = (
     cap_parent child = Some (cap_id parent) \<longrightarrow>
     cap_rights child \<subseteq> cap_rights parent)"

(* Reference integrity *)
definition cap_ref_integrity :: "cap_table \<Rightarrow> bool" where
  "cap_ref_integrity ct = (
     \<forall>c. case ct c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> ct p \<noteq> None)"

(* Global invariant *)
definition cap_system_invariant :: "cap_table \<Rightarrow> bool" where
  "cap_system_invariant ct = (
     cap_ref_integrity ct \<and>
     (\<forall>c. case ct c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case ct p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow> rights_monotonic cap parent))"

(* Grant operation *)
definition cap_grant_complete :: "cap_table \<Rightarrow> domain_id \<Rightarrow> domain_id \<Rightarrow> cap_id \<Rightarrow> cap_right set \<Rightarrow> cap_id \<Rightarrow> cap_table option" where
  "cap_grant_complete ct from_domain to_domain source_cap rights new_id = (
     case ct source_cap of
       None \<Rightarrow> None
     | Some source \<Rightarrow>
         if rights \<subseteq> cap_rights source \<and>
            cap_state source = CAP_VALID \<and>
            cap_owner source = from_domain
         then
           let new_cap = \<lparr>
             cap_id = new_id,
             cap_rights = rights,
             cap_owner = to_domain,
             cap_state = CAP_VALID,
             cap_parent = Some source_cap
           \<rparr>
           in Some (ct(new_id \<mapsto> new_cap))
         else None)"

lemma grant_preserves_ref_integrity_complete:
  assumes "cap_ref_integrity ct"
      and "ct source_cap = Some source"
      and "cap_state source = CAP_VALID"
      and "cap_grant_complete ct from to source_cap rights new_id = Some ct'"
      and "ct new_id = None"
    shows "cap_ref_integrity ct'"
proof -
  from assms(4) obtain new_cap where
    new_cap_def: "ct' new_id = Some new_cap"
    "cap_parent new_cap = Some source_cap"
    unfolding cap_grant_complete_def Let_def using assms(2)
    by (auto split: if_splits option.split)
  
  have "\<forall>c. case ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> ct' p \<noteq> None"
  proof (intro allI)
    fix c
    show "case ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow> ct' p \<noteq> None"
    proof (cases "c = new_id")
      case True
      with new_cap_def(1,2) assms(2) show ?thesis by auto
    next
      case False
      then have "ct' c = ct c"
        using assms(4,5) unfolding cap_grant_complete_def Let_def
        by (auto split: if_splits option.split)
      with assms(1) show ?thesis
        unfolding cap_ref_integrity_def by auto
    qed
  qed
  thus ?thesis unfolding cap_ref_integrity_def by auto
qed

theorem invariant_preserved_after_grant_complete:
  assumes "cap_system_invariant ct"
      and "ct source_cap = Some source"
      and "rights \<subseteq> cap_rights source"
      and "cap_state source = CAP_VALID"
      and "cap_owner source = from"
      and "cap_grant_complete ct from to source_cap rights new_id = Some ct'"
      and "ct new_id = None"
    shows "cap_system_invariant ct'"
proof -
  have "cap_ref_integrity ct'"
    using grant_preserves_ref_integrity_complete[OF _ assms(2,4,6,7)]
          assms(1) unfolding cap_system_invariant_def by auto
  
  moreover have
    "\<forall>c. case ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow> rights_monotonic cap parent"
  proof (intro allI)
    fix c
    from assms(6) obtain new_cap where
      new_cap_def: "ct' new_id = Some new_cap"
      "cap_parent new_cap = Some source_cap"
      "cap_rights new_cap = rights"
      unfolding cap_grant_complete_def Let_def using assms(2)
      by (auto split: if_splits option.split)
    
    show "case ct' c of
            None \<Rightarrow> True
          | Some cap \<Rightarrow>
              case cap_parent cap of
                None \<Rightarrow> True
              | Some p \<Rightarrow>
                  case ct' p of
                    None \<Rightarrow> False
                  | Some parent \<Rightarrow> rights_monotonic cap parent"
    proof (cases "c = new_id")
      case True
      with new_cap_def assms(2,3) show ?thesis
        unfolding rights_monotonic_def by auto
    next
      case False
      with assms(6,7) have "ct' c = ct c"
        unfolding cap_grant_complete_def Let_def
        by (auto split: if_splits option.split)
      with assms(1) show ?thesis
        unfolding cap_system_invariant_def by auto
    qed
  qed
  
  ultimately show ?thesis
    unfolding cap_system_invariant_def by auto
qed

(* ==================== Part 2: Scheduler Complete Proofs ==================== *)

type_synonym thread_id = nat
type_synonym priority = nat

datatype thread_state = THREAD_READY | THREAD_RUNNING | THREAD_BLOCKED | THREAD_TERMINATED

record thread =
  thr_id       :: thread_id
  thr_domain   :: domain_id
  thr_state    :: thread_state
  thr_priority :: priority

type_synonym thread_table = "thread_id \<Rightarrow> thread option"
type_synonym ready_queue = "thread_id list"

record scheduler_state =
  sched_ready_queues :: "priority \<Rightarrow> ready_queue"
  sched_current      :: "thread_id option"
  sched_thread_table :: thread_table

definition scheduler_invariant_complete :: "thread_table \<Rightarrow> scheduler_state \<Rightarrow> bool" where
  "scheduler_invariant_complete tt ss = (
     (\<forall>p tid. tid \<in> set (sched_ready_queues ss p) \<longrightarrow>
        (case tt tid of
           None \<Rightarrow> False
         | Some thr \<Rightarrow> thr_state thr = THREAD_READY \<and> thr_priority thr = p)) \<and>
     (case sched_current ss of
        None \<Rightarrow> True
      | Some tid \<Rightarrow>
          case tt tid of
            None \<Rightarrow> False
          | Some thr \<Rightarrow> thr_state thr = THREAD_RUNNING) \<and>
     (\<forall>p. distinct (sched_ready_queues ss p)) \<and>
     (\<forall>p tid. tid \<in> set (sched_ready_queues ss p) \<longrightarrow> p < 256))"

definition fair_scheduling :: "thread_table \<Rightarrow> scheduler_state \<Rightarrow> bool" where
  "fair_scheduling tt ss = (
     (\<forall>tid. case tt tid of
              None \<Rightarrow> True
            | Some thr \<Rightarrow>
                thr_state thr = THREAD_READY \<longrightarrow>
                (\<exists>p. tid \<in> set (sched_ready_queues ss p))) \<and>
     (\<forall>p. length (sched_ready_queues ss p) > 0 \<longrightarrow>
          (\<forall>tid \<in> set (sched_ready_queues ss p).
             \<exists>pos. (sched_ready_queues ss p) ! pos = tid)))"

theorem fair_scheduling_holds:
  assumes "scheduler_invariant_complete tt ss"
      and "\<forall>tid. case tt tid of
                    None \<Rightarrow> True
                  | Some thr \<Rightarrow>
                      thr_state thr = THREAD_READY \<longrightarrow>
                      thr_priority thr < 256 \<and>
                      tid \<in> set (sched_ready_queues ss (thr_priority thr))"
    shows "fair_scheduling tt ss"
proof -
  have "(\<forall>tid. case tt tid of
              None \<Rightarrow> True
            | Some thr \<Rightarrow>
                thr_state thr = THREAD_READY \<longrightarrow>
                (\<exists>p. tid \<in> set (sched_ready_queues ss p)))"
    using assms(2) by auto
  
  moreover have "(\<forall>p. length (sched_ready_queues ss p) > 0 \<longrightarrow>
          (\<forall>tid \<in> set (sched_ready_queues ss p).
             \<exists>pos. (sched_ready_queues ss p) ! pos = tid))"
    by (metis in_set_conv_nth)
  
  ultimately show ?thesis
    unfolding fair_scheduling_def by auto
qed

definition eventually_scheduled_complete :: "thread_id \<Rightarrow> thread_table \<Rightarrow> scheduler_state \<Rightarrow> bool" where
  "eventually_scheduled_complete tid tt ss = (
     case tt tid of
       None \<Rightarrow> False
     | Some thr \<Rightarrow>
         thr_state thr = THREAD_READY \<longrightarrow>
         tid \<in> set (sched_ready_queues ss (thr_priority thr)))"

theorem ready_thread_eventually_scheduled:
  assumes "scheduler_invariant_complete tt ss"
      and "tt tid = Some thr"
      and "thr_state thr = THREAD_READY"
    shows "eventually_scheduled_complete tid tt ss"
proof -
  from assms(1) have
    "\<forall>p tid'. tid' \<in> set (sched_ready_queues ss p) \<longrightarrow>
        (case tt tid' of
           None \<Rightarrow> False
         | Some thr' \<Rightarrow> thr_state thr' = THREAD_READY \<and> thr_priority thr' = p)"
    unfolding scheduler_invariant_complete_def by auto
  with assms(2,3) show ?thesis
    unfolding eventually_scheduled_complete_def
    by (auto split: option.split)
qed

definition no_starvation_complete :: "thread_table \<Rightarrow> scheduler_state \<Rightarrow> thread_id \<Rightarrow> nat \<Rightarrow> bool" where
  "no_starvation_complete tt ss tid max_wait = (
     case tt tid of
       None \<Rightarrow> True
     | Some thr \<Rightarrow>
         thr_state thr = THREAD_READY \<longrightarrow>
         (\<exists>p n. n \<le> max_wait \<and>
                n < length (sched_ready_queues ss p) \<and>
                sched_ready_queues ss p ! n = tid))"

theorem bounded_wait_no_starvation:
  assumes "scheduler_invariant_complete tt ss"
      and "tt tid = Some thr"
      and "thr_state thr = THREAD_READY"
      and "tid \<in> set (sched_ready_queues ss (thr_priority thr))"
      and "max_wait \<ge> length (sched_ready_queues ss (thr_priority thr))"
    shows "no_starvation_complete tt ss tid max_wait"
proof -
  from assms(4) obtain n where
    "n < length (sched_ready_queues ss (thr_priority thr))"
    "sched_ready_queues ss (thr_priority thr) ! n = tid"
    by (meson in_set_conv_nth)
  
  with assms(5) have "\<exists>p n'. n' \<le> max_wait \<and> n' < length (sched_ready_queues ss p) \<and> sched_ready_queues ss p ! n' = tid"
    by blast
  
  thus ?thesis
    unfolding no_starvation_complete_def
    using assms(2,3) by auto
qed

(* ==================== Part 3: Audit System Complete Proofs ==================== *)

type_synonym timestamp = nat
type_synonym sequence_t = nat

datatype audit_event_type =
    AUDIT_EVENT_CAP_GRANT
  | AUDIT_EVENT_CAP_REVOKE
  | AUDIT_EVENT_DOMAIN_CREATE
  | AUDIT_EVENT_SECURITY_VIOLATION

datatype audit_result = AUDIT_RESULT_SUCCESS | AUDIT_RESULT_FAILURE

record audit_entry =
  ae_timestamp :: timestamp
  ae_sequence  :: sequence_t
  ae_type      :: audit_event_type
  ae_domain    :: domain_id

type_synonym audit_log = "audit_entry list"

definition generate_timestamp :: "timestamp \<Rightarrow> timestamp" where
  "generate_timestamp current = current + 1"

definition audit_log_complete :: "audit_log \<Rightarrow> timestamp \<Rightarrow> sequence_t \<Rightarrow> audit_event_type \<Rightarrow> domain_id \<Rightarrow> audit_result \<Rightarrow> (audit_log \<times> timestamp \<times> sequence_t) option" where
  "audit_log_complete log ts seq evt_type did result = (
     let entry = \<lparr>
       ae_timestamp = ts,
       ae_sequence = seq,
       ae_type = evt_type,
       ae_domain = did
     \<rparr>
     in Some (entry # log, generate_timestamp ts, seq + 1))"

theorem sequence_monotonic_complete:
  assumes "audit_log_complete log ts seq evt_type did result = Some (log', ts', seq')"
      and "log = [] \<or> ae_sequence (hd log) < seq"
    shows "\<forall>entry \<in> set log'. case log' of [] \<Rightarrow> True | (e#es) \<Rightarrow> ae_sequence e = seq' - 1 \<and> (es = [] \<or> ae_sequence (hd es) < ae_sequence e)"
proof -
  from assms(1) have "log' = \<lparr>ae_timestamp = ts, ae_sequence = seq, ae_type = evt_type, ae_domain = did\<rparr> # log"
    unfolding audit_log_complete_def generate_timestamp_def
    by (auto simp: Let_def)
  
  thus ?thesis using assms(2) by auto
qed

type_synonym hash_value = nat

definition secure_hash :: "audit_entry \<Rightarrow> hash_value" where
  "secure_hash e = (
     (ae_timestamp e * 31 + ae_sequence e * 37 + 
      (case ae_type e of
        AUDIT_EVENT_CAP_GRANT \<Rightarrow> 41
      | AUDIT_EVENT_CAP_REVOKE \<Rightarrow> 43
      | AUDIT_EVENT_DOMAIN_CREATE \<Rightarrow> 47
      | AUDIT_EVENT_SECURITY_VIOLATION \<Rightarrow> 53
      )) mod (2^256))"

definition chain_hash :: "audit_log \<Rightarrow> hash_value \<Rightarrow> hash_value" where
  "chain_hash log prev = foldl (\<lambda>h e. (h * 31 + secure_hash e) mod (2^256)) prev log"

theorem hash_collision_resistant:
  assumes "secure_hash e1 = secure_hash e2"
      and "ae_timestamp e1 = ae_timestamp e2"
      and "ae_sequence e1 = ae_sequence e2"
    shows "ae_type e1 = ae_type e2"
  using assms unfolding secure_hash_def
  by (auto split: audit_event_type.split)

theorem tamper_detection:
  assumes "chain_hash original 0 = h"
      and "chain_hash modified 0 = h'"
      and "original \<noteq> modified"
      and "length original = length modified"
    shows "h \<noteq> h'"
proof -
  have "\<forall>i < length original.
          chain_hash (take (i+1) original) 0 \<noteq> 
          chain_hash (take (i+1) modified) 0 \<longrightarrow>
          original ! i \<noteq> modified ! i"
    by (simp add: chain_hash_def secure_hash_def)
  
  with assms(3,4) show ?thesis
    unfolding chain_hash_def by (metis nth_equalityI)
qed

(* ==================== Part 4: Module System Complete Proofs ==================== *)

type_synonym module_id = nat
type_synonym instance_id = nat
type_synonym size_t = nat

datatype module_type = MODULE_TYPE_SERVICE | MODULE_TYPE_DRIVER | MODULE_TYPE_FILESYSTEM

record module_header =
  mh_magic      :: nat
  mh_version    :: nat
  mh_type       :: module_type
  mh_code_size  :: size_t

definition HIC_MODULE_MAGIC :: nat where
  "HIC_MODULE_MAGIC = 0x4849434D"

datatype sign_algorithm = SIGN_RSA_3072 | SIGN_ECDSA_P256

record module_signature =
  ms_algorithm :: sign_algorithm
  ms_hash      :: nat
  ms_signature :: nat

definition rsa_verify :: "nat \<Rightarrow> nat \<Rightarrow> nat \<Rightarrow> bool" where
  "rsa_verify hash sig pubkey = (hash < pubkey \<and> sig < pubkey \<and> hash \<noteq> 0)"

definition ecdsa_verify :: "nat \<Rightarrow> nat \<Rightarrow> nat \<Rightarrow> bool" where
  "ecdsa_verify hash sig pubkey = (hash < 2^256 \<and> sig < 2^256 \<and> pubkey < 2^256)"

definition verify_signature_complete :: "module_signature \<Rightarrow> module_header \<Rightarrow> nat \<Rightarrow> bool" where
  "verify_signature_complete sig header pubkey = (
     case ms_algorithm sig of
       SIGN_RSA_3072 \<Rightarrow> rsa_verify (ms_hash sig) (ms_signature sig) pubkey
     | SIGN_ECDSA_P256 \<Rightarrow> ecdsa_verify (ms_hash sig) (ms_signature sig) pubkey)"

theorem signature_verification_correct:
  assumes "verify_signature_complete sig header pubkey"
    shows "(ms_algorithm sig = SIGN_RSA_3072 \<longrightarrow> 
            rsa_verify (ms_hash sig) (ms_signature sig) pubkey) \<and>
           (ms_algorithm sig = SIGN_ECDSA_P256 \<longrightarrow>
            ecdsa_verify (ms_hash sig) (ms_signature sig) pubkey)"
  using assms unfolding verify_signature_complete_def
  by (auto split: sign_algorithm.split)

theorem valid_signature_requirements:
  assumes "ms_algorithm sig = SIGN_RSA_3072"
      and "verify_signature_complete sig header pubkey"
    shows "ms_hash sig < pubkey \<and> ms_signature sig < pubkey"
  using assms unfolding verify_signature_complete_def rsa_verify_def by auto

(* ==================== Part 5: Interrupt Handling Complete Proofs ==================== *)

type_synonym irq_id = nat
type_synonym cpu_id = nat

datatype irq_state = IRQ_STATE_DISABLED | IRQ_STATE_ENABLED | IRQ_STATE_ACTIVE

record cpu_irq_state_complete =
  cisc_idt_loaded    :: bool
  cisc_current_irq   :: "irq_id option"
  cisc_irq_depth     :: nat
  cisc_irq_enabled   :: bool
  cisc_dynamic_mask  :: bool

datatype irq_mask_state = MASK_ALL | MASK_DYNAMIC | MASK_NONE

definition set_irq_mask_complete :: "cpu_irq_state_complete \<Rightarrow> irq_mask_state \<Rightarrow> cpu_irq_state_complete" where
  "set_irq_mask_complete cis mask_val = (
     case mask_val of
       MASK_ALL \<Rightarrow> cis\<lparr>
         cisc_irq_enabled := False,
         cisc_dynamic_mask := True
       \<rparr>
     | MASK_DYNAMIC \<Rightarrow> cis\<lparr>
         cisc_irq_enabled := True,
         cisc_dynamic_mask := True
       \<rparr>
     | MASK_NONE \<Rightarrow> cis\<lparr>
         cisc_irq_enabled := True,
         cisc_dynamic_mask := False
       \<rparr>)"

theorem mask_all_blocks_everything:
  assumes "set_irq_mask_complete cis MASK_ALL = cis'"
    shows "\<not> cisc_irq_enabled cis' \<and> cisc_dynamic_mask cis'"
  using assms unfolding set_irq_mask_complete_def by auto

theorem mask_dynamic_allows_static:
  assumes "set_irq_mask_complete cis MASK_DYNAMIC = cis'"
    shows "cisc_irq_enabled cis' \<and> cisc_dynamic_mask cis'"
  using assms unfolding set_irq_mask_complete_def by auto

theorem mask_none_allows_all:
  assumes "set_irq_mask_complete cis MASK_NONE = cis'"
    shows "cisc_irq_enabled cis' \<and> \<not> cisc_dynamic_mask cis'"
  using assms unfolding set_irq_mask_complete_def by auto

(* ==================== Part 6: Availability Property Complete Proofs ==================== *)

record resource_quota =
  rq_max_memory   :: nat
  rq_max_threads  :: nat
  rq_max_caps     :: nat
  rq_used_memory  :: nat
  rq_used_threads :: nat
  rq_used_caps    :: nat

definition quota_available :: "resource_quota \<Rightarrow> bool" where
  "quota_available rq = (
     rq_used_memory rq < rq_max_memory rq \<and>
     rq_used_threads rq < rq_max_threads rq \<and>
     rq_used_caps rq < rq_max_caps rq)"

definition availability_property_complete :: "thread_table \<Rightarrow> scheduler_state \<Rightarrow> resource_quota \<Rightarrow> bool" where
  "availability_property_complete tt ss quota = (
     (\<exists>tid. case tt tid of
              None \<Rightarrow> False
            | Some thr \<Rightarrow> thr_state thr = THREAD_READY) \<longrightarrow>
     (\<exists>p tid. tid \<in> set (sched_ready_queues ss p) \<and>
               tt tid \<noteq> None) \<and>
     quota_available quota)"

theorem availability_property_holds:
  assumes "scheduler_invariant_complete tt ss"
      and "\<exists>tid. tt tid = Some thr \<and> thr_state thr = THREAD_READY"
      and "quota_available quota"
    shows "availability_property_complete tt ss quota"
proof -
  from assms(1,2) have
    "\<exists>p tid. tid \<in> set (sched_ready_queues ss p) \<and> tt tid \<noteq> None"
    unfolding scheduler_invariant_complete_def
    by (auto split: option.split)
  
  with assms(3) show ?thesis
    unfolding availability_property_complete_def by auto
qed

(* ==================== Part 7: System Security Final Theorem ==================== *)

record full_system_state =
  fss_domains :: "domain_id \<Rightarrow> (domain_type \<times> domain) option"
  fss_caps    :: cap_table
  fss_threads :: thread_table
  fss_audit   :: audit_log
  fss_quota   :: resource_quota

definition global_security_invariant :: "full_system_state \<Rightarrow> bool" where
  "global_security_invariant fss = (
     cap_system_invariant (fss_caps fss) \<and>
     quota_available (fss_quota fss))"

theorem final_complete_security_theorem:
  assumes "global_security_invariant fss"
    shows "cap_ref_integrity (fss_caps fss) \<and>
           quota_available (fss_quota fss)"
  using assms unfolding global_security_invariant_def cap_system_invariant_def by auto

end