/**
 * wasm 初始化入口
 * 在应用启动时调用
 */

import { initWasm, isWasmLoaded } from './loader.js';

/**
 * 初始化所有 wasm 模块
 * 应在应用启动早期调用
 */
export async function initializeWasmModules(wasmPath?: string): Promise<void> {
  const success = await initWasm(wasmPath);
  
  if (!success) {
    console.warn('[wasm] Modules not loaded, using TypeScript fallback for all operations');
  }
}

/**
 * 检查 wasm 模块是否已加载
 */
export { isWasmLoaded };

/**
 * 获取 wasm 加载状态信息
 */
export function getWasmStatus(): { loaded: boolean; modules: string[] } {
  return {
    loaded: isWasmLoaded(),
    modules: [
      'routine_variables',
      'project_mentions',
      'log_redaction',
      'session_compaction'
    ]
  };
}
