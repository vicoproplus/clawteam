/**
 * wasm 模块加载器
 * 负责加载 MoonBit 编译的 wasm-gc 模块
 */

import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { initMemory } from './memory.js';

const __dirname = dirname(fileURLToPath(import.meta.url));

let wasmInstance: WebAssembly.Instance | null = null;
let wasmExports: WebAssembly.Exports | null = null;

/**
 * 初始化 wasm 模块
 * 在应用启动时调用
 * @returns 是否加载成功
 */
export async function initWasm(wasmPath?: string): Promise<boolean> {
  try {
    const resolvedPath = wasmPath ?? 
      join(__dirname, '../target/wasm-gc/release/bundle/paperclip-wasm.wasm');
    
    // 检查文件是否存在
    try {
      const wasmBuffer = readFileSync(resolvedPath);
      
      // 导入对象（根据 MoonBit 模块需求配置）
      const importObject: WebAssembly.Imports = {
        env: {
          // 如果需要 FFI，在这里添加函数
        }
      };
      
      // 实例化 wasm 模块
      const { instance } = await WebAssembly.instantiate(wasmBuffer, importObject);
      wasmInstance = instance;
      wasmExports = instance.exports;
      
      // 初始化内存管理
      initMemory(instance.exports.memory as WebAssembly.Memory, instance.exports);
      
      console.log('[wasm] Paperclip modules loaded successfully');
      return true;
    } catch (fileError) {
      // 文件不存在或读取失败
      console.warn('[wasm] wasm file not found, falling back to TypeScript:', (fileError as Error).message);
      return false;
    }
  } catch (err) {
    console.warn('[wasm] Failed to load wasm module:', (err as Error).message);
    return false;
  }
}

/**
 * 获取 wasm 导出实例
 * @returns 导出实例或 null（如果未加载）
 */
export function getWasmExports(): WebAssembly.Exports | null {
  return wasmExports;
}

/**
 * 获取 wasm 实例
 * @returns wasm 实例或 null
 */
export function getWasmInstance(): WebAssembly.Instance | null {
  return wasmInstance;
}

/**
 * 检查 wasm 模块是否已加载
 */
export function isWasmLoaded(): boolean {
  return wasmInstance !== null && wasmExports !== null;
}

/**
 * 调用 wasm 函数（带安全检查）
 * @param name 函数名
 * @param args 参数
 * @returns 函数返回值或 null
 */
export function callWasmFunction<T extends unknown[], R>(
  name: string,
  ...args: T
): R | null {
  if (!wasmExports) {
    return null;
  }
  
  const fn = wasmExports[name];
  if (typeof fn !== 'function') {
    return null;
  }
  
  return (fn as (...args: T) => R)(...args);
}

/**
 * 重置 wasm 模块（用于测试）
 */
export function resetWasm(): void {
  wasmInstance = null;
  wasmExports = null;
}
