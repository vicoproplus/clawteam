/**
 * Link @paperclipai/shared into plugin-sdk's node_modules
 */

import { copyFileSync, mkdirSync, existsSync, readdirSync, statSync, rmSync } from 'fs';
import { join } from 'path';

const ROOT = process.cwd();

function copyDirRecursive(src: string, dest: string): void {
  if (!existsSync(dest)) mkdirSync(dest, { recursive: true });
  for (const entry of readdirSync(src, { withFileTypes: true })) {
    const s = join(src, entry.name);
    const d = join(dest, entry.name);
    if (entry.isDirectory()) {
      if (entry.name !== 'node_modules') copyDirRecursive(s, d);
    } else if (entry.isFile()) {
      copyFileSync(s, d);
    }
  }
}

const sdkNm = join(ROOT, 'packages/plugins/sdk/node_modules/@paperclipai/shared');
if (existsSync(sdkNm)) rmSync(sdkNm, { recursive: true, force: true });

copyDirRecursive(join(ROOT, 'packages/shared'), sdkNm);
console.log('✅ linked shared to plugin-sdk');
