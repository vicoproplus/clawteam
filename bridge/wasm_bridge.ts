/**
 * wasm 桥接层
 * 提供统一的 API，自动在 wasm 和 TypeScript 实现之间切换
 * wasm 优先，失败时 fallback 到 TypeScript
 */

import { getWasmExports } from '../wasm_loader/loader.js';
import { writeString, readString, writeJSON, readJSON } from '../wasm_loader/memory.js';

// ============================================================================
// 导入原始 TypeScript 实现（本地 lib/ 副本）
// ============================================================================

// routine-variables 的 TypeScript 实现
async function tsExtractVariables(template: string): Promise<string[]> {
  const mod = await import('../lib/shared/routine-variables.js');
  return mod.extractRoutineVariableNames(template);
}

async function tsInterpolate(template: string, values: Record<string, unknown>): Promise<string | null> {
  const mod = await import('../lib/shared/routine-variables.js');
  return mod.interpolateRoutineTemplate(template, values);
}

// project-mentions 的 TypeScript 实现
async function tsNormalizeHexColor(input: string | null | undefined): Promise<string | null> {
  if (!input) return null;
  const trimmed = input.trim();
  if (!trimmed) return null;
  const hexRe = /^#?[0-9a-f]{6}$/i;
  const shortRe = /^#?[0-9a-f]{3}$/i;
  if (hexRe.test(trimmed)) {
    return trimmed.startsWith('#') ? trimmed.toLowerCase() : `#${trimmed.toLowerCase()}`;
  }
  if (shortRe.test(trimmed)) {
    const raw = trimmed.startsWith('#') ? trimmed.slice(1) : trimmed;
    return `#${raw[0]}${raw[0]}${raw[1]}${raw[1]}${raw[2]}${raw[2]}`.toLowerCase();
  }
  return null;
}

async function tsBuildProjectMentionHref(projectId: string, color?: string | null): Promise<string> {
  const mod = await import('../lib/shared/project-mentions.js');
  return mod.buildProjectMentionHref(projectId, color);
}

// log-redaction 的 TypeScript 实现
async function tsRedactHomePath(text: string): Promise<string> {
  const mod = await import('../lib/adapter-utils/log-redaction.js');
  return mod.redactHomePathUserSegments(text);
}

// ============================================================================
// wasm 桥接函数
// ============================================================================

/**
 * routine-variables 桥接
 */
export const routineVariables = {
  /**
   * 提取模板中的变量名
   */
  async extractVariables(template: string): Promise<string[]> {
    const wasm = getWasmExports();
    if (wasm?.extract_variables) {
      try {
        const { ptr, length } = writeString(template);
        const resultPtr = wasm.extract_variables(ptr, length) as number;
        const result = readString(resultPtr, 4096);
        wasm.free(ptr);
        wasm.free(resultPtr);
        return JSON.parse(result) as string[];
      } catch (err) {
        console.warn('[wasm] extract_variables failed, falling back to TS:', err);
      }
    }
    return tsExtractVariables(template);
  },

  /**
   * 执行模板插值
   */
  async interpolate(template: string, values: Record<string, unknown>): Promise<string | null> {
    const wasm = getWasmExports();
    if (wasm?.interpolate) {
      try {
        const tplData = writeString(template);
        const valuesData = writeJSON(values);
        const resultPtr = wasm.interpolate(
          tplData.ptr, tplData.length,
          valuesData.ptr, valuesData.length
        ) as number;
        const result = readString(resultPtr, 8192);
        wasm.free(tplData.ptr);
        wasm.free(valuesData.ptr);
        wasm.free(resultPtr);
        return result;
      } catch (err) {
        console.warn('[wasm] interpolate failed, falling back to TS:', err);
      }
    }
    return tsInterpolate(template, values);
  }
};

/**
 * project-mentions 桥接
 */
export const projectMentions = {
  /**
   * 规范化十六进制颜色值
   */
  async normalizeHexColor(input: string | null | undefined): Promise<string | null> {
    if (!input) return null;
    
    const wasm = getWasmExports();
    if (wasm?.normalize_hex_color) {
      try {
        const { ptr, length } = writeString(input);
        const resultPtr = wasm.normalize_hex_color(ptr, length) as number;
        
        // 读取结果（MoonBit Option 类型：null 指针表示 None）
        if (resultPtr === 0) {
          wasm.free(ptr);
          return null;
        }
        
        const result = readString(resultPtr, 64);
        wasm.free(ptr);
        wasm.free(resultPtr);
        return result;
      } catch (err) {
        console.warn('[wasm] normalize_hex_color failed, falling back to TS:', err);
      }
    }
    return tsNormalizeHexColor(input);
  },

  /**
   * 构建 project mention href
   */
  async buildProjectMentionHref(projectId: string, color?: string | null): Promise<string> {
    const wasm = getWasmExports();
    if (wasm?.build_project_mention_href) {
      try {
        const idData = writeString(projectId);
        const colorData = color ? writeString(color) : { ptr: 0, length: 0 };
        
        const resultPtr = wasm.build_project_mention_href(
          idData.ptr, idData.length,
          colorData.ptr, colorData.length
        ) as number;
        
        const result = readString(resultPtr, 1024);
        wasm.free(idData.ptr);
        if (colorData.ptr) wasm.free(colorData.ptr);
        wasm.free(resultPtr);
        return result;
      } catch (err) {
        console.warn('[wasm] build_project_mention_href failed, falling back to TS:', err);
      }
    }
    return tsBuildProjectMentionHref(projectId, color);
  }
};

/**
 * log-redaction 桥接
 */
export const logRedaction = {
  /**
   * 脱敏文本中的家庭路径
   */
  async redactHomePath(text: string): Promise<string> {
    const wasm = getWasmExports();
    if (wasm?.redact_home_path_user_segments) {
      try {
        const { ptr, length } = writeString(text);
        const resultPtr = wasm.redact_home_path_user_segments(ptr, length) as number;
        const result = readString(resultPtr, text.length * 3);  // 脱敏后可能更长
        wasm.free(ptr);
        wasm.free(resultPtr);
        return result;
      } catch (err) {
        console.warn('[wasm] redact_home_path failed, falling back to TS:', err);
      }
    }
    return tsRedactHomePath(text);
  }
};

/**
 * session-compaction 桥接
 */
export const sessionCompaction = {
  /**
   * 解析会话压缩策略
   */
  async resolvePolicy(
    adapterType: string,
    options?: {
      enabled?: boolean;
      maxSessionRuns?: number;
      maxRawInputTokens?: number;
      maxSessionAgeHours?: number;
    }
  ): Promise<unknown> {
    const wasm = getWasmExports();
    if (wasm?.resolve_session_compaction_policy) {
      try {
        const adapterData = writeString(adapterType);
        
        // 编码 Option[Bool]
        const enabledFlag = options?.enabled === undefined ? 0 : (options.enabled ? 1 : 2);
        
        const resultPtr = wasm.resolve_session_compaction_policy(
          adapterData.ptr, adapterData.length,
          enabledFlag,
          options?.maxSessionRuns ?? -1,
          options?.maxRawInputTokens ?? -1,
          options?.maxSessionAgeHours ?? -1
        ) as number;
        
        const result = readJSON(resultPtr, 4096);
        wasm.free(adapterData.ptr);
        wasm.free(resultPtr);
        return result;
      } catch (err) {
        console.warn('[wasm] resolve_policy failed, falling back to TS:', err);
      }
    }
    
    // Fallback: 返回模拟数据
    return {
      policy: {
        enabled: options?.enabled ?? true,
        maxSessionRuns: options?.maxSessionRuns ?? 200,
        maxRawInputTokens: options?.maxRawInputTokens ?? 2000000,
        maxSessionAgeHours: options?.maxSessionAgeHours ?? 72
      },
      source: options ? 'agent_override' : 'adapter_default'
    };
  }
};
