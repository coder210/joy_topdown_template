#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将 Tiled 的 Lua 导出（.lua，return {...} 表字面量）转换为 Tiled JSON 导出（.json）。

Tiled 的 Lua 与 JSON 导出结构同构：表用 {}，数组与对象都用 {}，因此按"首元素是否为
key = value"判定该表是对象还是数组即可无损转写。转换后由 jtilemap 的 cJSON 后端解析，
行为与原 Lua 后端完全一致（字段一一对应，包括对象的 shape 字符串）。

用法：python tools/lua_to_json.py <in.lua> [out.json]
若不指定 out.json，则在输入同目录生成同名 .json。
"""
import sys
import json
import os


def tokenize(s):
    tokens = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c in ' \t\r\n':
            i += 1
            continue
        # 注释 -- 到行尾
        if c == '-' and i + 1 < n and s[i + 1] == '-':
            j = s.find('\n', i)
            if j == -1:
                j = n
            i = j
            continue
        if c == '{':
            tokens.append(('LBRACE', None)); i += 1; continue
        if c == '}':
            tokens.append(('RBRACE', None)); i += 1; continue
        if c == ',':
            tokens.append(('COMMA', None)); i += 1; continue
        if c == '=':
            tokens.append(('EQ', None)); i += 1; continue
        if c == '[':
            tokens.append(('LBRACKET', None)); i += 1; continue
        if c == ']':
            tokens.append(('RBRACKET', None)); i += 1; continue
        if c == '"' or c == "'":
            delim = c
            j = i + 1
            buf = []
            while j < n and s[j] != delim:
                if s[j] == '\\' and j + 1 < n:
                    nx = s[j + 1]
                    buf.append({'n': '\n', 't': '\t', 'r': '\r',
                                '\\': '\\', '"': '"', "'": "'", '/': '/'}.get(nx, nx))
                    j += 2
                else:
                    buf.append(s[j])
                    j += 1
            tokens.append(('STRING', ''.join(buf)))
            i = j + 1
            continue
        if c.isdigit() or (c == '-' and i + 1 < n and s[i + 1].isdigit()):
            j = i + 1
            while j < n and (s[j].isdigit() or s[j] == '.' or s[j] in 'eE'):
                j += 1
            # 指数符号后的正负号
            if j < n and s[j] in '+-' and j > i + 1 and s[j - 1] in 'eE':
                j += 1
                while j < n and s[j].isdigit():
                    j += 1
            lit = s[i:j]
            val = float(lit) if ('.' in lit or 'e' in lit or 'E' in lit) else int(lit)
            tokens.append(('NUMBER', val))
            i = j
            continue
        if c.isalpha() or c == '_':
            j = i + 1
            while j < n and (s[j].isalnum() or s[j] == '_'):
                j += 1
            tokens.append(('IDENT', s[i:j]))
            i = j
            continue
        # 其它字符（Tiled 导出里不应出现）直接跳过
        i += 1
    return tokens


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def advance(self):
        t = self.peek()
        self.i += 1
        return t

    def parse_value(self):
        t = self.peek()
        if t[0] is None:
            raise ValueError("意外结束")
        if t[0] == 'LBRACE':
            return self.parse_table()
        if t[0] == 'STRING':
            self.advance()
            return t[1]
        if t[0] == 'NUMBER':
            self.advance()
            return t[1]
        if t[0] == 'IDENT':
            self.advance()
            if t[1] == 'true':
                return True
            if t[1] == 'false':
                return False
            if t[1] == 'nil':
                return None
            return t[1]  # 其它裸标识符按字符串处理
        raise ValueError("无法解析的 token: %r" % (t,))

    def parse_table(self):
        self.advance()  # 消费 {
        la = self.peek()
        is_object = False
        if la[0] == 'RBRACE':
            is_object = True
        elif la[0] in ('IDENT', 'STRING') and self.i + 1 < len(self.toks) \
                and self.toks[self.i + 1][0] == 'EQ':
            is_object = True

        if is_object:
            obj = {}
            while True:
                t = self.peek()
                if t[0] == 'RBRACE':
                    self.advance()
                    break
                kt = self.advance()
                if kt[0] == 'IDENT':
                    key = kt[1]
                elif kt[0] == 'STRING':
                    key = kt[1]
                else:
                    raise ValueError("非法 key: %r" % (kt,))
                # 容忍 key 后直接跟 '[' 下标（Tiled 偶发）的情况：当作字符串 key
                if self.peek()[0] == 'EQ':
                    self.advance()
                else:
                    # key 后没有 '='，退化为数组处理（极少出现）
                    # 把已读的 key 当作数组首个元素处理
                    return self._recover_as_array(kt)
                val = self.parse_value()
                obj[key] = val
                nt = self.peek()
                if nt[0] == 'COMMA':
                    self.advance()
                    continue
                # 否则（RBRACE 或下一个 key）继续循环
        else:
            obj = []
            while True:
                t = self.peek()
                if t[0] == 'RBRACE':
                    self.advance()
                    break
                obj.append(self.parse_value())
                nt = self.peek()
                if nt[0] == 'COMMA':
                    self.advance()
                    continue
        return obj

    def _recover_as_array(self, first_key_token):
        # 极端兜底：遇到 key 后无 '='，把已读内容当作数组元素（本转换器理论上不会触发）
        arr = []
        if first_key_token[0] == 'STRING':
            arr.append(first_key_token[1])
        elif first_key_token[0] == 'IDENT':
            w = first_key_token[1]
            if w == 'true':
                arr.append(True)
            elif w == 'false':
                arr.append(False)
            elif w == 'nil':
                arr.append(None)
            else:
                arr.append(w)
        while True:
            t = self.peek()
            if t[0] == 'RBRACE':
                self.advance()
                break
            arr.append(self.parse_value())
            nt = self.peek()
            if nt[0] == 'COMMA':
                self.advance()
                continue
        return arr


def convert(text):
    toks = tokenize(text)
    # 跳过开头的 return 关键字（若有）
    if toks and toks[0][0] == 'IDENT' and toks[0][1] == 'return':
        toks = toks[1:]
    p = Parser(toks)
    return p.parse_value()


def main():
    if len(sys.argv) < 2:
        print("usage: lua_to_json.py <in.lua> [out.json]")
        return 2
    in_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.splitext(in_path)[0] + ".json"
    with open(in_path, "r", encoding="utf-8") as f:
        text = f.read()
    data = convert(text)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")

    # 基本健全性检查
    assert isinstance(data, dict), "顶层应为对象"
    for k in ("width", "height", "tilewidth", "tileheight"):
        assert k in data, "缺少字段 %s" % k
    layers = data.get("layers", [])
    assert isinstance(layers, list), "layers 应为数组"
    n_data = sum(1 for ly in layers
                 if isinstance(ly, dict) and ly.get("type") == "tilelayer"
                 and isinstance(ly.get("data"), list))
    print("OK %s -> %s  (map %dx%d, layers=%d, tilelayers=%d)"
          % (in_path, out_path, data.get("width"), data.get("height"),
             len(layers), n_data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
