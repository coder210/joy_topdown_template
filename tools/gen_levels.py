#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""为 joy_topdown_template 生成 5 关不同的 Tiled .lua 关卡(map)文件。

布局思路（契合"边杀边逃离" + 帧同步确定性）：
  - 64x64，外边界为墙('#', GID=1)，空地('.', GID=0)。
  - 左上角为主迷宫区(DFS 完美迷宫，1 格宽通道，保证全连通)。
  - 右下角为开阔出生/逃离广场，玩家在此出生。
  - 每关用不同 seed → 迷宫纹理明显不同。
  - 敌人撒布在迷宫上部/左侧通道，用 BFS 校验每个敌人都能从玩家出发可达，
    不可达则换 seed 重生成，保证可玩。
"""
import random, os

M = 64
OUT_DIR = r"D:\github\joy\joy_topdown_template\assets\joy_topdown_template\levels"

def gen_maze(seed):
    rng = random.Random(seed)
    grid = [['#' for _ in range(M)] for _ in range(M)]
    # 1) 主迷宫区域：仅覆盖左上 x in [1,47], y in [1,47].
    #    先给奇数格铺通道点，再 DFS 挖通(完美迷宫)。
    mx0, mx1, my0, my1 = 1, 47, 1, 47
    cells = []
    for y in range(my0, my1+1, 2):
        for x in range(mx0, mx1+1, 2):
            grid[y][x] = '.'
            cells.append((x, y))
    # DFS 递归分割生成连通迷宫
    in_maze = {(x, y): False for x, y in cells}
    start = cells[0]
    stack = [start]
    in_maze[start] = True
    while stack:
        x, y = stack[-1]
        neigh = []
        for dx, dy in ((2,0), (-2,0), (0,2), (0,-2)):
            nx, ny = x+dx, y+dy
            if mx0 <= nx <= mx1 and my0 <= ny <= my1 and (nx, ny) in in_maze and not in_maze[(nx, ny)]:
                neigh.append((nx, ny, (x+nx)//2, (y+ny)//2))
        if neigh:
            nx, ny, wx, wy = rng.choice(neigh)
            grid[wy][wx] = '.'      # 挖掉中间格
            in_maze[(nx, ny)] = True
            stack.append((nx, ny))
        else:
            stack.pop()
    # 2) 迷宫右下出口过渡带(与右下广场连接)
    for y in range(46, 54):
        for x in range(46, 54):
            grid[y][x] = '.'
    # 3) 右下出生/逃离广场(大空地)
    for y in range(54, 63):
        for x in range(48, 63):
            grid[y][x] = '.'
    # 4) 外边界墙
    for x in range(M):
        grid[0][x] = '#'
        grid[M-1][x] = '#'
    for y in range(M):
        grid[y][0] = '#'
        grid[y][M-1] = '#'
    return grid

def bfs_reachable(grid, src, dsts):
    from collections import deque
    q = deque([src]); seen = {src}
    while q:
        x, y = q.popleft()
        for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
            nx, ny = x+dx, y+dy
            if 0 <= nx < M and 0 <= ny < M and grid[ny][nx] == '.' and (nx, ny) not in seen:
                seen.add((nx, ny)); q.append((nx, ny))
    return all(d in seen for d in dsts)

def pick_enemies(rng, grid, n=8, player=(56,58)):
    # 在整个地图的空地格中分散撒布敌人：彼此留间距、避开出生广场，
    # 且让敌人尽量覆盖迷宫区(左上)。曼哈顿距离约束。
    cands = []
    for y in range(2, M - 2):
        for x in range(2, M - 2):
            if grid[y][x] != '.': 
                continue
            if abs(x - player[0]) + abs(y - player[1]) < 12:
                continue        # 距出生点太近
            # 越靠迷宫(左上)权重越高，先放迷宫侧敌人
            score = (46 - min(x, 46)) + (46 - min(y, 46))
            cands.append((score, x, y))
    cands.sort(reverse=True)
    sel = []
    for _, x, y in cands:
        if len(sel) >= n:
            break
        # 与已选敌人保持最小间距，尽量铺满迷宫纵深
        if all(abs(x - ox) + abs(y - oy) >= 9 for (ox, oy) in sel):
            sel.append((x, y))
    return sel

def build_ascii_grid(grid, player, enemies):
    # 实体字形：玩家格 P，敌人格 E，其余墙 #、空 .
    g = [row[:] for row in grid]
    for (ex, ey) in enemies:
        g[ey][ex] = 'E'
    g[player[1]][player[0]] = 'P'
    lines = []
    for r in range(M):
        lines.append(''.join(g[r]))
    return lines

def build_level_sources(groups):
    # groups: list of (lines) 每关 64 行
    entries = []
    for lines in groups:
        body = '\\n'.join(lines)
        entries.append('    "' + body + '"')
    return (
        'const int LEVEL_SOURCES_COUNT = 5;\n'
        'inline const char* LEVEL_SRCS[5] = {\n'
        + ',\n'.join(entries) +
        '\n};\n'
    )

def build_lua(grid, player, enemies):
    rows = []
    for r in range(M):
        row = ', '.join('1' if grid[r][c] == '#' else '0' for c in range(M))
        rows.append('          ' + row + ',')
    data_txt = '\n'.join(rows).rstrip(',')
    # spawn 对象(像素坐标 = 格*16+8)
    objs = [f'          {{ id = {i+1}, name = "enemy{i+1}", type = "enemy", shape = "point", x = {ex*16+8}.0, y = {ey*16+8}.0, width = 0, height = 0, visible = true }},'
            for i, (ex, ey) in enumerate(enemies)]
    objs.append(f'          {{ id = {len(enemies)+1}, name = "player", type = "player", shape = "point", x = {player[0]*16+8}.0, y = {player[1]*16+8}.0, width = 0, height = 0, visible = true }}')
    obj_txt = '\n'.join(objs)
    return f'''return {{
  version = 1.5,
  luaversion = 5.1,
  orientation = "orthogonal",
  renderorder = "right-down",
  width = 64,
  height = 64,
  tilewidth = 16,
  tileheight = 16,
  infinite = false,
  tilesets = {{
    {{
      name = "terrain",
      firstgid = 1,
      tilewidth = 16,
      tileheight = 16,
      spacing = 0,
      margin = 0,
      tilecount = 1,
      columns = 1
    }}
  }},
  layers = {{
    {{
      type = "tilelayer",
      name = "terrain",
      x = 0,
      y = 0,
      width = 64,
      height = 64,
      opacity = 1,
      visible = true,
      data = {{
{data_txt}
      }}
    }},
    {{
      type = "objectgroup",
      name = "spawn",
      x = 0,
      y = 0,
      opacity = 1,
      visible = true,
      objects = {{
{obj_txt}
      }}
    }}
  }}
}}
'''

def main():
    seeds = [3, 17, 42, 91, 137]
    player = (56, 58)
    groups = []
    for idx, seed in enumerate(seeds, start=1):
        for attempt in range(300):
            rng = random.Random(seed * (attempt + 1) + idx * 7)
            grid = gen_maze(rng.randint(1, 10 ** 6))
            grid[player[1]][player[0]] = '.'
            enemies = pick_enemies(rng, grid)
            if len(enemies) < 8:
                continue
            if bfs_reachable(grid, player, enemies):
                groups.append(build_ascii_grid(grid, player, enemies))
                text = build_lua(grid, player, enemies)
                path = os.path.join(OUT_DIR, f"level{idx}.lua")
                with open(path, 'w', encoding='utf-8') as f:
                    f.write(text)
                nwall = sum(1 for r in grid for c in r if c == '#')
                print(f"level{idx}: seed={seed} walls={nwall} enemies={enemies} player={player} OK -> {os.path.basename(path)}")
                break
        else:
            print(f"level{idx}: FAILED after 300 attempts")

    # level_sources.h 位于 .../joy_topdown_template/systems/ (从 levels 上三级)
    src_path = os.path.abspath(os.path.join(OUT_DIR, "..", "..", "..", "systems", "level_sources.h"))
    with open(src_path, 'w', encoding='utf-8') as f:
        f.write(build_level_sources(groups))
    print("wrote", src_path)
    print("done")

if __name__ == "__main__":
    main()