///|
/// session-compaction 模块 - MoonBit 实现
/// 原始 TypeScript 实现参考: lib/adapter-utils/session-compaction.ts
pub enum NativeContextManagement {
  Confirmed
  Likely
  Unknown
  None
} derive(Debug, Eq)

///|
pub(all) struct SessionCompactionPolicy {
  enabled : Bool
  max_session_runs : Int
  max_raw_input_tokens : Int
  max_session_age_hours : Int
} derive(Debug, Eq)

///|
pub(all) struct AdapterSessionManagement {
  supports_session_resume : Bool
  native_context_management : NativeContextManagement
  default_session_compaction : SessionCompactionPolicy
} derive(Debug, Eq)

///|
pub(all) struct ResolvedSessionCompactionPolicy {
  policy : SessionCompactionPolicy
  adapter_session_management : AdapterSessionManagement?
  explicit_override : SessionCompactionPolicy
  source : String
} derive(Debug, Eq)

///|
/// 默认策略
fn default_policy() -> SessionCompactionPolicy {
  SessionCompactionPolicy::{
    enabled: true,
    max_session_runs: 200,
    max_raw_input_tokens: 2000000,
    max_session_age_hours: 72,
  }
}

///|
/// 适配器管理的策略（阈值为 0）
fn adapter_managed_policy() -> SessionCompactionPolicy {
  SessionCompactionPolicy::{
    enabled: true,
    max_session_runs: 0,
    max_raw_input_tokens: 0,
    max_session_age_hours: 0,
  }
}

///|
/// 获取适配器会话管理配置
pub fn get_adapter_session_management(
  adapter_type : String,
) -> AdapterSessionManagement? {
  match adapter_type {
    "claude_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Confirmed,
        default_session_compaction: adapter_managed_policy(),
      })
    "codex_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Confirmed,
        default_session_compaction: adapter_managed_policy(),
      })
    "hermes_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Confirmed,
        default_session_compaction: adapter_managed_policy(),
      })
    "cursor" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Unknown,
        default_session_compaction: default_policy(),
      })
    "gemini_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Unknown,
        default_session_compaction: default_policy(),
      })
    "opencode_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Unknown,
        default_session_compaction: default_policy(),
      })
    "pi_local" =>
      Some(AdapterSessionManagement::{
        supports_session_resume: true,
        native_context_management: NativeContextManagement::Unknown,
        default_session_compaction: default_policy(),
      })
    _ => None
  }
}

///|
/// 检查是否是 legacy sessioned 适配器
fn is_legacy_sessioned_adapter(adapter_type : String) -> Bool {
  match adapter_type {
    "claude_local"
    | "codex_local"
    | "cursor"
    | "gemini_local"
    | "hermes_local"
    | "opencode_local"
    | "pi_local" => true
    _ => false
  }
}

///|
/// 解析策略
pub fn resolve_session_compaction_policy(
  adapter_type : String,
  explicit_enabled : Bool?,
  explicit_max_session_runs : Int?,
  explicit_max_raw_input_tokens : Int?,
  explicit_max_session_age_hours : Int?,
) -> ResolvedSessionCompactionPolicy {
  let adapter_mgmt = get_adapter_session_management(adapter_type)

  // 确定基础策略
  let base_policy = match adapter_mgmt {
    Some(m) => m.default_session_compaction
    None => {
      let fallback = is_legacy_sessioned_adapter(adapter_type)
      SessionCompactionPolicy::{
        enabled: fallback,
        max_session_runs: default_policy().max_session_runs,
        max_raw_input_tokens: default_policy().max_raw_input_tokens,
        max_session_age_hours: default_policy().max_session_age_hours,
      }
    }
  }

  // 应用显式覆盖
  let has_explicit = explicit_enabled.is_some() ||
    explicit_max_session_runs.is_some() ||
    explicit_max_raw_input_tokens.is_some() ||
    explicit_max_session_age_hours.is_some()

  let final_policy = SessionCompactionPolicy::{
    enabled: match explicit_enabled {
      Some(v) => v
      None => base_policy.enabled
    },
    max_session_runs: match explicit_max_session_runs {
      Some(v) => v
      None => base_policy.max_session_runs
    },
    max_raw_input_tokens: match explicit_max_raw_input_tokens {
      Some(v) => v
      None => base_policy.max_raw_input_tokens
    },
    max_session_age_hours: match explicit_max_session_age_hours {
      Some(v) => v
      None => base_policy.max_session_age_hours
    },
  }

  let source = if has_explicit {
    "agent_override"
  } else if adapter_mgmt.is_some() {
    "adapter_default"
  } else {
    "legacy_fallback"
  }

  let explicit_ov = SessionCompactionPolicy::{
    enabled: match explicit_enabled {
      Some(v) => v
      None => default_policy().enabled
    },
    max_session_runs: match explicit_max_session_runs {
      Some(v) => v
      None => default_policy().max_session_runs
    },
    max_raw_input_tokens: match explicit_max_raw_input_tokens {
      Some(v) => v
      None => default_policy().max_raw_input_tokens
    },
    max_session_age_hours: match explicit_max_session_age_hours {
      Some(v) => v
      None => default_policy().max_session_age_hours
    },
  }

  ResolvedSessionCompactionPolicy::{
    policy: final_policy,
    adapter_session_management: adapter_mgmt,
    explicit_override: explicit_ov,
    source,
  }
}

///|
/// 检查是否有会话压缩阈值
pub fn has_session_compaction_thresholds(
  policy : SessionCompactionPolicy,
) -> Bool {
  policy.max_session_runs > 0 ||
  policy.max_raw_input_tokens > 0 ||
  policy.max_session_age_hours > 0
}
