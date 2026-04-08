/// session_compaction 模块测试

///|
test "get_adapter_session_management claude_local" {
  let result = get_adapter_session_management("claude_local")
  assert_true(result.is_some())
  let m = result.unwrap()
  assert_true(m.supports_session_resume)
  // 用模式匹配验证 enum 值
  match m.native_context_management {
    NativeContextManagement::Confirmed => assert_true(true)
    _ => assert_true(false)
  }
}

///|
test "get_adapter_session_management unknown adapter" {
  let result = get_adapter_session_management("unknown_adapter")
  @debug.assert_eq(result, None)
}

///|
test "resolve_session_compaction_policy claude adapter" {
  let result = resolve_session_compaction_policy(
    "claude_local",
    None,
    None,
    None,
    None,
  )
  @debug.assert_eq(result.source, "adapter_default")
  // claude 是 adapter_managed，阈值为 0
  @debug.assert_eq(result.policy.max_session_runs, 0)
  @debug.assert_eq(result.policy.max_raw_input_tokens, 0)
}

///|
test "resolve_session_compaction_policy cursor adapter" {
  let result = resolve_session_compaction_policy(
    "cursor",
    None,
    None,
    None,
    None,
  )
  @debug.assert_eq(result.source, "adapter_default")
  // cursor 使用 default_policy
  @debug.assert_eq(result.policy.max_session_runs, 200)
  @debug.assert_eq(result.policy.max_raw_input_tokens, 2000000)
}

///|
test "resolve_session_compaction_policy unknown adapter (legacy)" {
  let result = resolve_session_compaction_policy(
    "unknown_adapter",
    None,
    None,
    None,
    None,
  )
  @debug.assert_eq(result.source, "legacy_fallback")
  @debug.assert_eq(result.policy.enabled, false) // legacy 但不在 legacy 列表中
}

///|
test "resolve_session_compaction_policy with explicit override" {
  let result = resolve_session_compaction_policy(
    "cursor",
    Some(true),
    Some(100),
    None,
    None,
  )
  @debug.assert_eq(result.source, "agent_override")
  @debug.assert_eq(result.policy.max_session_runs, 100)
  @debug.assert_eq(result.policy.enabled, true)
}

///|
test "has_session_compaction_thresholds with thresholds" {
  let policy = resolve_session_compaction_policy(
      "cursor",
      None,
      None,
      None,
      None,
    ).policy
  assert_true(has_session_compaction_thresholds(policy))
}

///|
test "has_session_compaction_thresholds no thresholds" {
  let policy = resolve_session_compaction_policy(
      "claude_local",
      None,
      None,
      None,
      None,
    ).policy
  assert_true(!has_session_compaction_thresholds(policy)) // claude 阈值为 0
}
