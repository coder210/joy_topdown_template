# -*- coding: utf-8 -*-
"""只恢复旧版蛇形迷宫为第1关，level2..5 保持现状。"""
import os, re, shutil
base = r"D:\github\joy\joy_topdown_template"
old_h = r"D:\github\joy\build\Release\_old_levels\old_level_sources.h"
cur_h = os.path.join(base, "systems", "level_sources.h")

old_txt = open(old_h, encoding="utf-8-sig").read()
cur_txt = open(cur_h, encoding="utf-8").read()

def strings(block):
    return re.findall(r'"((?:[^"\\]|\\.)*)"', block, re.S)

# 旧版第1个字符串：取 LEVEL_SRCS[5] 块的第一个字符串
old_block = re.search(r'LEVEL_SRCS\s*\[\s*5\s*\]\s*=\s*\{(.*?)\};', old_txt, re.S)
assert old_block, "old level_sources: block not found"
old_strs = strings(old_block.group(1))
assert len(old_strs) == 5, f"expected 5 old strings, got {len(old_strs)}"
lv1 = old_strs[0]

# 当前 LEVEL_SRCS[5] 块
cs = re.search(r'LEVEL_SRCS\s*\[\s*5\s*\]\s*=\s*\{(.*?)\};', cur_txt, re.S)
assert cs, "current level_sources: block not found"
cur_strs = strings(cs.group(1))
assert len(cur_strs) == 5, f"expected 5 strings, got {len(cur_strs)}"
cur_strs[0] = lv1   # 仅替换第1关

new_block = 'LEVEL_SRCS[5] = {\n    ' + ',\n    '.join('"' + s + '"' for s in cur_strs) + '\n};'
cur_txt = cur_txt[:cs.start()] + new_block + cur_txt[cs.end():]
open(cur_h, "w", encoding="utf-8").write(cur_txt)

# 同步旧版 level1.lua（数据同源）
src_lua = r"D:\github\joy\build\Release\_old_levels\level1.lua"
dst_lua = os.path.join(base, "assets", "joy_topdown_template", "levels", "level1.lua")
shutil.copy(src_lua, dst_lua)

print(f"OK: level1 restored (len={len(lv1)}), lua copied -> {dst_lua}, other levels untouched")