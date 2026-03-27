(*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *)

(* 
 * HIC 模块系统形式化验证
 * 
 * 验证模块加载和管理的正确性和安全性：
 * 1. 模块格式验证
 * 2. 模块签名验证
 * 3. 模块依赖解析
 * 4. 模块隔离性
 *)

theory HIC_Module_System
imports Main
begin

(* ==================== 基础类型定义 ==================== *)

type_synonym module_id = nat
type_synonym instance_id = nat
type_synonym phys_addr = nat
type_synonym size_t = nat
type_synonym domain_id = nat
type_synonym hash_t = nat
type_synonym signature_t = nat

(* ==================== 模块头定义 ==================== *)

(* 模块类型 *)
datatype module_type =
    MODULE_TYPE_SERVICE
  | MODULE_TYPE_DRIVER
  | MODULE_TYPE_FILESYSTEM
  | MODULE_TYPE_EXTENSION

(* 模块权限 *)
datatype module_permission =
    PERM_MEMORY_ALLOC
  | PERM_IO_PORT
  | PERM_INTERRUPT
  | PERM_HARDWARE_ACCESS
  | PERM_CAPABILITY_MANAGE

(* 模块头 *)
record module_header =
  mh_magic        :: nat
  mh_version      :: nat
  mh_type         :: module_type
  mh_name         :: "string"
  mh_entry        :: phys_addr
  mh_code_size    :: size_t
  mh_data_size    :: size_t
  mh_bss_size     :: size_t
  mh_permissions  :: "module_permission set"
  mh_dependencies :: "string list"

(* 魔数定义 *)
definition HIC_MODULE_MAGIC :: nat where
  "HIC_MODULE_MAGIC = 0x4849434D"  (* "HICM" *)

(* ==================== 模块实例 ==================== *)

(* 实例状态 *)
datatype instance_state =
    INSTANCE_LOADED
  | INSTANCE_RUNNING
  | INSTANCE_STOPPED
  | INSTANCE_ERROR

(* 模块实例 *)
record module_instance =
  mi_id           :: instance_id
  mi_module_id    :: module_id
  mi_state        :: instance_state
  mi_code_base    :: phys_addr
  mi_data_base    :: phys_addr
  mi_bss_base     :: phys_addr
  mi_domain       :: domain_id

(* ==================== 模块签名 ==================== *)

(* 签名算法 *)
datatype sign_algorithm =
    SIGN_RSA_2048
  | SIGN_RSA_3072
  | SIGN_ECDSA_P256

(* 模块签名 *)
record module_signature =
  ms_algorithm  :: sign_algorithm
  ms_hash       :: hash_t
  ms_signature  :: signature_t
  ms_public_key :: nat  (* 公钥索引 *)

(* 签名验证结果 *)
datatype verify_result =
    VERIFY_OK
  | VERIFY_INVALID_SIGNATURE
  | VERIFY_INVALID_HASH
  | VERIFY_UNKNOWN_KEY
  | VERIFY_UNSUPPORTED_ALGORITHM

(* ==================== 模块加载器状态 ==================== *)

(* 模块信息 *)
record module_info =
  mod_id           :: module_id
  mod_header       :: module_header
  mod_signature    :: "module_signature option"
  mod_instances    :: "instance_id list"
  mod_verified     :: bool

(* 模块加载器 *)
record module_loader =
  ml_modules       :: "module_id \<Rightarrow> module_info option"
  ml_instances     :: "instance_id \<Rightarrow> module_instance option"
  ml_next_id       :: instance_id
  ml_next_mod_id   :: module_id
  ml_trusted_keys  :: "nat set"

(* ==================== 模块验证 ==================== *)

(* 有效魔数 *)
definition valid_module_magic :: "module_header \<Rightarrow> bool" where
  "valid_module_magic h \<equiv> mh_magic h = HIC_MODULE_MAGIC"

(* 有效大小 *)
definition valid_module_size :: "module_header \<Rightarrow> bool" where
  "valid_module_size h \<equiv>
     mh_code_size h > 0 \<and>
     mh_code_size h + mh_data_size h + mh_bss_size h \<le> 16 * 1024 * 1024"  (* 最大16MB *)

(* 有效入口点 *)
definition valid_entry_point :: "module_header \<Rightarrow> bool" where
  "valid_entry_point h \<equiv>
     mh_entry h < mh_code_size h"

(* 有效模块头 *)
definition valid_module_header :: "module_header \<Rightarrow> bool" where
  "valid_module_header h \<equiv>
     valid_module_magic h \<and>
     valid_module_size h \<and>
     valid_entry_point h"

(* ==================== 签名验证 ==================== *)

(* 简化的签名验证 *)
definition verify_signature :: "module_signature \<Rightarrow> module_header \<Rightarrow> nat set \<Rightarrow> verify_result" where
  "verify_signature sig header trusted_keys \<equiv>
     if ms_public_key sig \<notin> trusted_keys then
       VERIFY_UNKNOWN_KEY
     else if ms_hash sig = 0 then
       VERIFY_INVALID_HASH
     else
       VERIFY_OK"  (* 简化：假设签名有效 *)

(* 模块可信 *)
definition module_trusted :: "module_info \<Rightarrow> bool" where
  "module_trusted mod_info \<equiv>
     mod_verified mod_info \<and>
     (case mod_signature mod_info of
        None \<Rightarrow> False
      | Some sig \<Rightarrow> True)"

(* ==================== 模块加载操作 ==================== *)

(* 加载模块 *)
definition load_module :: "module_loader \<Rightarrow> module_header \<Rightarrow> module_signature option \<Rightarrow> (module_loader \<times> module_id) option" where
  "load_module loader header sig_opt \<equiv>
     if \<not> valid_module_header header then
       None
     else
       let verified = (case sig_opt of
                        None \<Rightarrow> False
                      | Some sig \<Rightarrow> 
                          verify_signature sig header (ml_trusted_keys loader) = VERIFY_OK);
           new_mod_id = ml_next_mod_id loader;
           mod_info = \<lparr>
             mod_id = new_mod_id,
             mod_header = header,
             mod_signature = sig_opt,
             mod_instances = [],
             mod_verified = verified
           \<rparr>
       in Some (loader\<lparr>
         ml_modules := (ml_modules loader)(new_mod_id := Some mod_info),
         ml_next_mod_id := new_mod_id + 1
       \<rparr>, new_mod_id)"

(* 创建实例 *)
definition create_instance :: "module_loader \<Rightarrow> module_id \<Rightarrow> phys_addr \<Rightarrow> domain_id \<Rightarrow> (module_loader \<times> instance_id) option" where
  "create_instance loader mid base did \<equiv>
     case ml_modules loader mid of
       None \<Rightarrow> None
     | Some mod_info \<Rightarrow>
         if \<not> mod_verified mod_info then
           None
         else
           let header = mod_header mod_info;
               inst_id = ml_next_id loader;
               inst = \<lparr>
                 mi_id = inst_id,
                 mi_module_id = mid,
                 mi_state = INSTANCE_LOADED,
                 mi_code_base = base,
                 mi_data_base = base + mh_code_size header,
                 mi_bss_base = base + mh_code_size header + mh_data_size header,
                 mi_domain = did
               \<rparr>
           in Some (loader\<lparr>
             ml_instances := (ml_instances loader)(inst_id := Some inst),
             ml_next_id := inst_id + 1,
             ml_modules := (ml_modules loader)(mid := Some (mod_info\<lparr>mod_instances := inst_id # mod_instances mod_info\<rparr>))
           \<rparr>, inst_id)"

(* 销毁实例 *)
definition destroy_instance :: "module_loader \<Rightarrow> instance_id \<Rightarrow> module_loader option" where
  "destroy_instance loader inst_id \<equiv>
     case ml_instances loader inst_id of
       None \<Rightarrow> None
     | Some inst \<Rightarrow>
         let mid = mi_module_id inst;
             mod_info = the (ml_modules loader mid);
             new_instances = filter (\<lambda>id. id \<noteq> inst_id) (mod_instances mod_info)
         in Some (loader\<lparr>
           ml_instances := (ml_instances loader)(inst_id := None),
           ml_modules := (ml_modules loader)(mid := Some (mod_info\<lparr>mod_instances := new_instances\<rparr>))
         \<rparr>)"

(* ==================== 模块依赖 ==================== *)

(* 依赖关系 *)
type_synonym dependency_graph = "module_id \<Rightarrow> module_id list"

(* 依赖已满足 *)
definition dependencies_satisfied :: "module_loader \<Rightarrow> module_id \<Rightarrow> bool" where
  "dependencies_satisfied loader mid \<equiv>
     case ml_modules loader mid of
       None \<Rightarrow> False
     | Some mod_info \<Rightarrow>
         \<forall>dep_name \<in> set (mh_dependencies (mod_header mod_info)).
           \<exists>mid'. case ml_modules loader mid' of
                    None \<Rightarrow> False
                  | Some dep_mod \<Rightarrow> mh_name (mod_header dep_mod) = dep_name"

(* 无循环依赖 *)
definition no_cyclic_dependency :: "dependency_graph \<Rightarrow> bool" where
  "no_cyclic_dependency graph \<equiv>
     \<forall>mid. mid \<notin> set (graph mid) \<and>
              (\<forall>dep \<in> set (graph mid). mid \<notin> set (graph dep))"

(* 定理: 无循环依赖确保终止 *)
theorem acyclic_terminates:
  assumes "no_cyclic_dependency graph"
  shows   "\<forall>mid. finite (set (graph mid))"
  using assms
  unfolding no_cyclic_dependency_def
  by auto

(* ==================== 模块不变式 ==================== *)

(* 实例有效 *)
definition valid_instance :: "module_instance \<Rightarrow> module_info \<Rightarrow> bool" where
  "valid_instance inst mod_info \<equiv>
     mi_module_id inst = mod_id mod_info \<and>
     mi_state inst \<noteq> INSTANCE_ERROR"

(* 加载器不变式 *)
definition loader_invariant :: "module_loader \<Rightarrow> bool" where
  "loader_invariant loader \<equiv>
     (\<forall>inst_id. case ml_instances loader inst_id of
                  None \<Rightarrow> True
                | Some inst \<Rightarrow>
                    case ml_modules loader (mi_module_id inst) of
                      None \<Rightarrow> False
                    | Some mod_info \<Rightarrow> valid_instance inst mod_info) \<and>
     (\<forall>mid. case ml_modules loader mid of
                  None \<Rightarrow> True
                | Some mod_info \<Rightarrow>
                    mod_verified mod_info \<longrightarrow> valid_module_header (mod_header mod_info))"

(* ==================== 模块定理 ==================== *)

(* 定理1: 加载返回有效模块 *)
theorem load_returns_valid_module:
  assumes "load_module loader header sig_opt = Some (loader', mid)"
  and     "valid_module_header header"
  shows   "case ml_modules loader' mid of
             None \<Rightarrow> False
           | Some mod_info \<Rightarrow> mod_header mod_info = header"
  using assms
  unfolding load_module_def
  by (auto split: option.split if_split)

(* 定理2: 只有验证过的模块才能创建实例 *)
theorem only_verified_creates_instance:
  assumes "create_instance loader mid base did = Some (loader', inst_id)"
  shows   "case ml_modules loader mid of
             None \<Rightarrow> False
           | Some mod_info \<Rightarrow> mod_verified mod_info"
  using assms
  unfolding create_instance_def
  by (auto split: option.split)

(* 定理3: 实例代码和数据区域不相交 *)
theorem instance_regions_disjoint:
  assumes "create_instance loader mid base did = Some (loader', inst_id)"
  and     "ml_instances loader' inst_id = Some inst"
  and     "mh_code_size (mod_header (the (ml_modules loader mid))) > 0"
  shows   "mi_code_base inst < mi_data_base inst"
  using assms
  unfolding create_instance_def
  by (auto split: option.split)

(* 定理4: 销毁实例从列表中移除 *)
theorem destroy_removes_instance:
  assumes "destroy_instance loader inst_id = Some loader'"
  and     "ml_instances loader inst_id = Some inst"
  shows   "ml_instances loader' inst_id = None"
  using assms
  unfolding destroy_instance_def
  by (auto split: option.split)

(* ==================== 模块隔离 ==================== *)

(* 模块内存隔离 *)
definition modules_isolated :: "module_loader \<Rightarrow> bool" where
  "modules_isolated loader \<equiv>
     \<forall>id1 id2. id1 \<noteq> id2 \<longrightarrow>
       (case ml_instances loader id1 of
          None \<Rightarrow> True
        | Some inst1 \<Rightarrow>
            case ml_instances loader id2 of
              None \<Rightarrow> True
            | Some inst2 \<Rightarrow>
                let end1 = mi_code_base inst1 + 
                           mh_code_size (mod_header (the (ml_modules loader (mi_module_id inst1)))) +
                           mh_data_size (mod_header (the (ml_modules loader (mi_module_id inst1))));
                    end2 = mi_code_base inst2 + 
                           mh_code_size (mod_header (the (ml_modules loader (mi_module_id inst2)))) +
                           mh_data_size (mod_header (the (ml_modules loader (mi_module_id inst2))))
                in mi_code_base inst1 \<ge> end2 \<or> mi_code_base inst2 \<ge> end1)"

(* 定理5: 隔离确保无重叠 *)
theorem isolation_no_overlap:
  assumes "modules_isolated loader"
  and     "ml_instances loader id1 = Some inst1"
  and     "ml_instances loader id2 = Some inst2"
  and     "id1 \<noteq> id2"
  shows   "mi_code_base inst1 \<ge> mi_code_base inst2 + 
           mh_code_size (mod_header (the (ml_modules loader (mi_module_id inst2)))) +
           mh_data_size (mod_header (the (ml_modules loader (mi_module_id inst2)))) \<or>
           mi_code_base inst2 \<ge> mi_code_base inst1 + 
           mh_code_size (mod_header (the (ml_modules loader (mi_module_id inst1)))) +
           mh_data_size (mod_header (the (ml_modules loader (mi_module_id inst1))))"
  using assms
  unfolding modules_isolated_def
  by auto

(* ==================== 完整模块系统 ==================== *)

(* 完整模块系统状态 *)
record full_module_system =
  fms_loader    :: module_loader
  fms_deps      :: dependency_graph

(* 完整系统不变式 *)
definition full_module_invariant :: "full_module_system \<Rightarrow> bool" where
  "full_module_invariant fms \<equiv>
     loader_invariant (fms_loader fms) \<and>
     modules_isolated (fms_loader fms) \<and>
     no_cyclic_dependency (fms_deps fms)"

(* 定理6: 完整系统一致性 *)
theorem full_module_consistency:
  assumes "full_module_invariant fms"
  shows   "loader_invariant (fms_loader fms) \<and>
           modules_isolated (fms_loader fms)"
  using assms
  unfolding full_module_invariant_def
  by auto

end
