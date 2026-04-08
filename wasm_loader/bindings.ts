/**
 * wasm 函数类型声明
 * 与 MoonBit 导出签名对应
 */

/**
 * wasm 模块导出的函数类型
 * 注意：这些类型需要根据实际 MoonBit 编译结果调整
 */
export interface PaperclipWasmExports extends WebAssembly.Exports {
  // 内存管理
  malloc: (size: number) => number;
  free: (ptr: number) => void;
  
  // routine_variables 模块
  extract_variables: (ptr: number, len: number) => number;
  interpolate: (tplPtr: number, tplLen: number, varsPtr: number, varsLen: number) => number;
  is_valid_variable_name: (ptr: number, len: number) => number;
  sync_variables_with_template: (tplPtr: number, tplLen: number, existingPtr: number, existingLen: number) => number;
  
  // project_mentions 模块
  normalize_hex_color: (ptr: number, len: number) => number;
  build_project_mention_href: (idPtr: number, idLen: number, colorPtr: number, colorLen: number) => number;
  parse_project_mention_href: (ptr: number, len: number) => number;
  build_agent_mention_href: (idPtr: number, idLen: number, iconPtr: number, iconLen: number) => number;
  parse_agent_mention_href: (ptr: number, len: number) => number;
  build_skill_mention_href: (idPtr: number, idLen: number, slugPtr: number, slugLen: number) => number;
  parse_skill_mention_href: (ptr: number, len: number) => number;
  normalize_agent_icon: (ptr: number, len: number) => number;
  normalize_skill_slug: (ptr: number, len: number) => number;
  
  // log_redaction 模块
  redact_home_path_user_segments: (ptr: number, len: number) => number;
  
  // session_compaction 模块
  get_adapter_session_management: (ptr: number, len: number) => number;
  resolve_session_compaction_policy: (
    adapterPtr: number, adapterLen: number,
    enabledFlag: number,  // 0 = None, 1 = Some(true), 2 = Some(false)
    maxSessionRuns: number,
    maxRawInputTokens: number,
    maxSessionAgeHours: number
  ) => number;
  has_session_compaction_thresholds: (
    enabled: number,
    maxSessionRuns: number,
    maxRawInputTokens: number,
    maxSessionAgeHours: number
  ) => number;
  
  // 内存（由 WASM GC 自动管理）
  memory: WebAssembly.Memory;
}

/**
 * 检查 wasm 导出是否包含预期的函数
 */
export function validateWasmExports(exports: WebAssembly.Exports): boolean {
  const required = [
    'extract_variables',
    'interpolate',
    'normalize_hex_color',
    'redact_home_path_user_segments',
    'resolve_session_compaction_policy',
    'malloc',
    'free',
    'memory'
  ];
  
  return required.every(name => typeof exports[name] === 'function' || name === 'memory');
}
