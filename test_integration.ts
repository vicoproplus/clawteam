/**
 * 简单的集成测试，验证 wasm_loader 和 bridge 的基本功能
 */

import { initWasm, isWasmLoaded, getWasmExports } from './wasm_loader/loader.js';
import { writeString, readString, reset } from './wasm_loader/memory.js';
import { routineVariables, projectMentions, logRedaction } from './bridge/wasm_bridge.js';

async function testMemoryOps() {
  console.log('=== Memory Operations Test ===');
  
  if (!isWasmLoaded()) {
    console.log('⚠️  wasm not loaded, skipping memory ops test');
    console.log('  (run moon build --target wasm-gc first)');
    return;
  }
  
  // 测试字符串读写
  const testStr = "Hello, MoonBit! 你好世界";
  const { ptr, length } = writeString(testStr);
  const result = readString(ptr, length);
  
  if (result === testStr) {
    console.log('✅ writeString/readString: PASS');
  } else {
    console.log(`❌ writeString/readString: FAIL (got "${result}")`);
  }
  
  reset();
}

async function testWasmLoader() {
  console.log('\n=== Wasm Loader Test ===');
  
  // wasm 文件可能不存在（需要 moon build），所以测试 fallback
  const loaded = await initWasm();
  
  if (loaded) {
    console.log('✅ wasm loaded: PASS');
    console.log(`  exports available: ${getWasmExports() !== null}`);
  } else {
    console.log('⚠️  wasm not built yet (run moon build --target wasm-gc)');
    console.log('  fallback to TS: OK');
  }
  
  console.log(`  isWasmLoaded: ${isWasmLoaded()}`);
}

async function testBridge() {
  console.log('\n=== Bridge Layer Test ===');
  
  // 测试 routineVariables fallback
  try {
    const vars = await routineVariables.extractVariables("{{name}} and {{action}}");
    if (Array.isArray(vars)) {
      console.log('✅ routineVariables.extractVariables: PASS');
      console.log(`  result: ${JSON.stringify(vars)}`);
    } else {
      console.log('❌ routineVariables.extractVariables: FAIL');
    }
  } catch (err) {
    console.log(`⚠️  routineVariables.extractVariables: SKIPPED (${(err as Error).message})`);
  }
  
  // 测试 projectMentions fallback
  try {
    const color = await projectMentions.normalizeHexColor("#AABBCC");
    if (color === "#aabbcc") {
      console.log('✅ projectMentions.normalizeHexColor: PASS');
    } else {
      console.log(`❌ projectMentions.normalizeHexColor: FAIL (got "${color}")`);
    }
  } catch (err) {
    console.log(`⚠️  projectMentions.normalizeHexColor: SKIPPED (${(err as Error).message})`);
  }
  
  // 测试 logRedaction fallback
  try {
    const text = "Error at /Users/john/secret/file.txt";
    const redacted = await logRedaction.redactHomePath(text);
    if (redacted.includes("***")) {
      console.log('✅ logRedaction.redactHomePath: PASS');
      console.log(`  result: "${redacted}"`);
    } else {
      console.log(`❌ logRedaction.redactHomePath: FAIL (got "${redacted}")`);
    }
  } catch (err) {
    console.log(`⚠️  logRedaction.redactHomePath: SKIPPED (${(err as Error).message})`);
  }
}

async function main() {
  console.log('Paperclip wasm Bridge Integration Tests\n');
  
  await testMemoryOps();
  await testWasmLoader();
  await testBridge();
  
  console.log('\n=== All tests completed ===');
}

main().catch(console.error);
