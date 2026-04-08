/**
 * wasm 内存管理工具
 * 负责在 Node.js 和 wasm 模块之间读写内存
 */

let wasmMemory: WebAssembly.Memory | null = null;
let wasmExports: WebAssembly.Exports | null = null;

/**
 * 初始化内存引用
 */
export function initMemory(memory: WebAssembly.Memory, exports: WebAssembly.Exports): void {
  wasmMemory = memory;
  wasmExports = exports;
}

/**
 * 获取内存实例
 */
export function getMemory(): WebAssembly.Memory | null {
  return wasmMemory;
}

/**
 * 获取导出实例
 */
export function getExports(): WebAssembly.Exports | null {
  return wasmExports;
}

/**
 * 在 wasm 内存中分配空间
 * @param size 需要分配的字节数
 * @returns 内存地址指针
 */
export function malloc(size: number): number {
  if (!wasmExports?.malloc) {
    throw new Error('wasm malloc not available');
  }
  return (wasmExports.malloc as (size: number) => number)(size);
}

/**
 * 释放 wasm 内存
 * @param ptr 内存地址指针
 */
export function free(ptr: number): void {
  if (!wasmExports?.free) {
    throw new Error('wasm free not available');
  }
  (wasmExports.free as (ptr: number) => void)(ptr);
}

/**
 * 写入字符串到 wasm 内存
 * @param str 要写入的字符串
 * @returns 包含指针和长度的对象 {ptr, length}
 */
export function writeString(str: string): { ptr: number; length: number } {
  const encoder = new TextEncoder();
  const data = encoder.encode(str);
  const ptr = malloc(data.length);
  
  if (!wasmMemory) {
    throw new Error('wasm memory not initialized');
  }
  
  const memory = new Uint8Array(wasmMemory.buffer);
  memory.set(data, ptr);
  
  return { ptr, length: data.length };
}

/**
 * 从 wasm 内存读取字符串
 * @param ptr 内存地址指针
 * @param length 要读取的长度
 * @returns 读取到的字符串
 */
export function readString(ptr: number, length: number): string {
  if (!wasmMemory) {
    throw new Error('wasm memory not initialized');
  }
  
  const memory = new Uint8Array(wasmMemory.buffer);
  const data = memory.slice(ptr, ptr + length);
  return new TextDecoder().decode(data);
}

/**
 * 从 wasm 内存读取整数数组（JSON 格式）
 * @param ptr 内存地址指针
 * @param maxLength 最大读取长度
 * @returns 解析后的数组
 */
export function readArray<T>(ptr: number, maxLength: number): T[] {
  const jsonStr = readString(ptr, maxLength);
  try {
    return JSON.parse(jsonStr) as T[];
  } catch {
    return [];
  }
}

/**
 * 写入 JSON 对象到 wasm 内存
 * @param obj 要写入的对象
 * @returns 包含指针和长度的对象 {ptr, length}
 */
export function writeJSON(obj: unknown): { ptr: number; length: number } {
  const jsonStr = JSON.stringify(obj);
  return writeString(jsonStr);
}

/**
 * 从 wasm 内存读取 JSON 对象
 * @param ptr 内存地址指针
 * @param maxLength 最大读取长度
 * @returns 解析后的对象
 */
export function readJSON<T>(ptr: number, maxLength: number): T | null {
  const jsonStr = readString(ptr, maxLength);
  try {
    return JSON.parse(jsonStr) as T;
  } catch {
    return null;
  }
}

/**
 * 清理内存引用（用于测试重置）
 */
export function reset(): void {
  wasmMemory = null;
  wasmExports = null;
}
