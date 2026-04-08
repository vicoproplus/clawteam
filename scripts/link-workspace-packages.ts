/**
 * 手动链接 workspace 包到 node_modules
 * 用于不支持符号链接的文件系统
 */

import { copyFileSync, mkdirSync, existsSync, readdirSync, statSync } from 'fs';
import { join, relative } from 'path';

const ROOT = process.cwd();

// Workspace packages to link
const WORKSPACE_PACKAGES = [
  { name: '@paperclipai/shared', src: 'packages/shared' },
  { name: '@paperclipai/db', src: 'packages/db' },
  { name: '@paperclipai/adapter-utils', src: 'packages/adapter-utils' },
  { name: '@paperclipai/adapter-claude-local', src: 'packages/adapters/claude-local' },
  { name: '@paperclipai/adapter-codex-local', src: 'packages/adapters/codex-local' },
  { name: '@paperclipai/adapter-cursor-local', src: 'packages/adapters/cursor-local' },
  { name: '@paperclipai/adapter-gemini-local', src: 'packages/adapters/gemini-local' },
  { name: '@paperclipai/adapter-openclaw-gateway', src: 'packages/adapters/openclaw-gateway' },
  { name: '@paperclipai/adapter-opencode-local', src: 'packages/adapters/opencode-local' },
  { name: '@paperclipai/adapter-pi-local', src: 'packages/adapters/pi-local' },
  { name: '@paperclipai/plugin-sdk', src: 'packages/plugins/sdk' },
];

// Consumers that need workspace packages
const CONSUMERS = ['server', 'ui', 'cli'];

function copyDirRecursive(src: string, dest: string): void {
  if (!existsSync(dest)) {
    mkdirSync(dest, { recursive: true });
  }
  
  const entries = readdirSync(src, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = join(src, entry.name);
    const destPath = join(dest, entry.name);
    
    if (entry.isDirectory()) {
      if (entry.name !== 'node_modules') {
        copyDirRecursive(srcPath, destPath);
      }
    } else {
      copyFileSync(srcPath, destPath);
    }
  }
}

function linkWorkspacePackages(): void {
  console.log('Linking workspace packages...\n');
  
  for (const consumer of CONSUMERS) {
    const consumerRoot = join(ROOT, consumer);
    const nmScoped = join(consumerRoot, 'node_modules', '@paperclipai');
    
    if (!existsSync(consumerRoot)) {
      console.log(`⚠️  Skipping ${consumer} (directory not found)`);
      continue;
    }
    
    console.log(`📦 ${consumer}/node_modules/@paperclipai/:`);
    
    if (!existsSync(nmScoped)) {
      mkdirSync(nmScoped, { recursive: true });
    }
    
    for (const pkg of WORKSPACE_PACKAGES) {
      const srcPath = join(ROOT, pkg.src);
      const destPath = join(nmScoped, pkg.name.replace('@paperclipai/', ''));
      
      if (!existsSync(srcPath)) {
        console.log(`  ❌ ${pkg.name} (source not found)`);
        continue;
      }
      
      try {
        // Remove existing directory
        if (existsSync(destPath)) {
          require('fs').rmSync(destPath, { recursive: true, force: true });
        }
        
        copyDirRecursive(srcPath, destPath);
        console.log(`  ✅ ${pkg.name}`);
      } catch (err) {
        console.log(`  ❌ ${pkg.name} (${(err as Error).message})`);
      }
    }
    console.log('');
  }
  
  console.log('✅ Workspace packages linked!');
}

linkWorkspacePackages();
