#!/usr/bin/env python3
"""
静态汇编堆栈平衡检查器
- 改进函数识别：基于全局标签和顶级标签（不以点开头）
- 支持通过节（.text）限制指令解析
- 处理 ret imm16
- 动态堆栈分配检测与用户注释支持
- 路径敏感分析，报告每个返回路径的不平衡
"""

import re
import sys
import argparse
import subprocess
import os
from collections import deque

# ------------------------------------------------------------
# 环境检测
# ------------------------------------------------------------
def detect_env(lines):
    bits = 64
    syntax = 'att'
    for line_num, raw_line in enumerate(lines, 1):
        clean = re.sub(r'[;#].*$', '', raw_line).strip()
        if not clean:
            continue
        if re.search(r'\[bits\s+64\]|bits\s+64|\.code64|USE64', clean, re.I):
            bits = 64
        if re.search(r'\[bits\s+32\]|bits\s+32|\.code32|USE32', clean, re.I):
            bits = 32
        if re.search(r'\brax\b|\brbx\b|\brcx\b|\brdx\b|\brsp\b|\rr[0-9]+\b', clean, re.I):
            bits = 64
        elif re.search(r'\beax\b|\bebx\b|\becx\b|\bedx\b|\besp\b', clean, re.I):
            bits = 32
        if re.search(r'^\s*push[lq]?\s+\$', clean) or re.search(r'^\s*mov[lq]?\s+[^,]*%', clean):
            syntax = 'att'
        elif re.search(r'^\s*push\s+[a-z]+', clean, re.I) and not re.search(r'[%$]', clean):
            syntax = 'intel'
    return {'bits': bits, 'syntax': syntax}

def preprocess_file(filename):
    try:
        cmd = ['gcc', '-E', '-x', 'assembler-with-cpp', filename]
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return [line for line in result.stdout.splitlines() if not line.startswith('#')]
    except (subprocess.CalledProcessError, FileNotFoundError):
        with open(filename, 'r') as f:
            return f.readlines()

def get_stack_effect(bits):
    unit = 8 if bits == 64 else 4
    return {
        'push':   -unit,
        'pop':    +unit,
        'call':   -unit,
        'ret':    +unit,
        'iretq':  +24,
        'sysret': +8,
        'sysretq':+8,
        'leave':  +unit,
    }

def parse_ret_imm(operands, syntax='att'):
    if not operands:
        return 0
    if syntax == 'intel':
        match = re.match(r'^\s*(\d+)\s*$', operands)
    else:
        match = re.match(r'^\s*\$(\d+)\s*$', operands)
    return int(match.group(1)) if match else 0

def imm_effect(mnemonic, operands, syntax='att'):
    if syntax == 'intel':
        add_pat = re.compile(r'^\s*add\s+rsp\s*,\s*([+-]?0x[0-9a-f]+|\d+)', re.I)
        sub_pat = re.compile(r'^\s*sub\s+rsp\s*,\s*([+-]?0x[0-9a-f]+|\d+)', re.I)
    else:
        add_pat = re.compile(r'^\s*add[lq]?\s+\$([+-]?0x[0-9a-f]+|\d+)\s*,\s*%rsp', re.I)
        sub_pat = re.compile(r'^\s*sub[lq]?\s+\$([+-]?0x[0-9a-f]+|\d+)\s*,\s*%rsp', re.I)
    if mnemonic == 'add':
        match = add_pat.match(operands)
        if match:
            return int(match.group(1), 0)
    elif mnemonic == 'sub':
        match = sub_pat.match(operands)
        if match:
            return -int(match.group(1), 0)
    return 0

def is_dynamic_rsp_adjustment(mnemonic, operands, syntax):
    if syntax == 'intel':
        pat = re.compile(r'^\s*(add|sub)\s+rsp\s*,\s*([a-z][a-z0-9]*)', re.I)
    else:
        pat = re.compile(r'^\s*(add|sub)[lq]?\s+([^,]+),\s*%rsp', re.I)
    match = pat.match(operands)
    if match:
        second = match.group(2).strip()
        if re.match(r'^[a-z][a-z0-9]*$', second, re.I) and not second.startswith('$'):
            return True
    return False

def extract_annotations(line):
    ann = {}
    call_conv_match = re.search(r'#\s*CALL_CONV:\s*(\w+)', line, re.I)
    if call_conv_match:
        ann['call_conv'] = call_conv_match.group(1).lower()
    targets_match = re.search(r'#\s*TARGETS:\s*([a-zA-Z0-9_,\s]+)', line, re.I)
    if targets_match:
        targets = [t.strip() for t in targets_match.group(1).split(',')]
        ann['targets'] = targets
    fixed_size_match = re.search(r'#\s*FIXED_SIZE:\s*(\d+)', line, re.I)
    if fixed_size_match:
        ann['fixed_size'] = int(fixed_size_match.group(1))
    return ann

class BasicBlock:
    def __init__(self, start_line, label=None):
        self.start_line = start_line
        self.label = label
        self.instructions = []      # (行号, 原始行, 助记符, 操作数, 注解)
        self.successors = []
        self.end_with = None

    def add_inst(self, line_num, raw_line, mnemonic, operands, ann):
        self.instructions.append((line_num, raw_line, mnemonic, operands, ann))

def parse_asm(lines, env):
    # 预处理行：保留原始行用于注解，同时去除注释用于识别
    clean_lines = []
    for i, raw in enumerate(lines):
        line_no_comment = re.sub(r'[;#].*$', '', raw).strip()
        clean_lines.append((i+1, raw, line_no_comment))

    blocks = []
    label_to_block = {}
    global_labels = set()
    current_section = None
    data_directives = {'.byte', '.word', '.long', '.quad', '.ascii', '.asciz', '.string', '.zero'}

    current_block = None
    i = 0
    while i < len(clean_lines):
        line_num, raw_line, line = clean_lines[i]
        ann = extract_annotations(raw_line)

        if not line:
            i += 1
            continue

        # 节切换
        if line.startswith('.section') or line in ('.text', '.data', '.bss', '.rodata'):
            sec_match = re.match(r'^\.(?:section\s+)?(\S+)', line)
            if sec_match:
                current_section = sec_match.group(1)
            elif line == '.text':
                current_section = 'text'
            else:
                current_section = line[1:]
            # 节切换时结束当前块
            if current_block and current_block.instructions:
                current_block = None
            i += 1
            continue

        # 只在代码节中解析指令（简单判断节名包含 text）
        if not current_section or 'text' not in current_section.lower():
            i += 1
            continue

        # 全局声明
        if line.startswith('.global') or line.startswith('.globl'):
            parts = line.split()
            if len(parts) >= 2:
                for sym in parts[1].split(','):
                    global_labels.add(sym.strip())
            i += 1
            continue

        # 标签行
        label_match = re.match(r'^([a-zA-Z_][a-zA-Z0-9_]*):\s*$', line)
        if label_match:
            label = label_match.group(1)
            # 结束当前块，开始新块
            current_block = BasicBlock(line_num, label)
            blocks.append(current_block)
            label_to_block[label] = len(blocks) - 1
            i += 1
            continue

        # 数据定义行跳过
        if any(line.startswith(d) for d in data_directives):
            i += 1
            continue

        # 指令行
        if env['syntax'] == 'intel':
            instr_match = re.match(r'^\s*([a-zA-Z][a-zA-Z0-9]*)\s+(.*)$', line)
        else:
            instr_match = re.match(r'^\s*([a-zA-Z][a-zA-Z0-9]*[lq]?)\s+(.*)$', line)

        if instr_match:
            mnemonic = instr_match.group(1).lower()
            operands = instr_match.group(2).strip()

            if current_block is None:
                current_block = BasicBlock(line_num, None)
                blocks.append(current_block)

            current_block.add_inst(line_num, raw_line, mnemonic, operands, ann)

            # 判断是否结束当前块
            if mnemonic in ('jmp', 'ret', 'iret', 'iretq', 'sysret', 'sysretq') or \
               (mnemonic.startswith('j') and mnemonic != 'jmp' and len(mnemonic) > 1):
                current_block.end_with = mnemonic
                current_block = None
        else:
            # 其他伪指令忽略
            pass

        i += 1

    # 构建后继关系
    for idx, block in enumerate(blocks):
        if not block.instructions:
            continue
        last_inst = block.instructions[-1]
        last_mnemonic = last_inst[2]
        operands = last_inst[3]
        ann = last_inst[4]

        if last_mnemonic in ('ret', 'iret', 'iretq', 'sysret', 'sysretq'):
            block.successors = []
        elif last_mnemonic == 'jmp':
            target = re.sub(r'^[\*\%]*', '', operands.strip())
            if target in label_to_block:
                block.successors = [label_to_block[target]]
            elif 'targets' in ann:
                succs = []
                for t in ann['targets']:
                    if t in label_to_block:
                        succs.append(label_to_block[t])
                if succs:
                    block.successors = succs
                else:
                    print(f"警告: 第 {last_inst[0]} 行用户指定的目标均未找到，路径终止", file=sys.stderr)
                    block.successors = []
            else:
                print(f"警告: 第 {last_inst[0]} 行间接跳转目标未知，路径终止", file=sys.stderr)
                block.successors = []
        elif last_mnemonic.startswith('j') and last_mnemonic != 'jmp':
            target = re.sub(r'^[\*\%]*', '', operands.strip())
            fallthrough_idx = idx + 1 if idx + 1 < len(blocks) else None
            succs = []
            if target in label_to_block:
                succs.append(label_to_block[target])
            elif 'targets' in ann:
                for t in ann['targets']:
                    if t in label_to_block:
                        succs.append(label_to_block[t])
                if not succs:
                    print(f"警告: 第 {last_inst[0]} 行条件跳转目标未知，仅考虑 fallthrough", file=sys.stderr)
            else:
                print(f"警告: 第 {last_inst[0]} 行条件跳转目标未知，仅考虑 fallthrough", file=sys.stderr)
            if fallthrough_idx is not None:
                succs.append(fallthrough_idx)
            block.successors = succs
        elif last_mnemonic == 'call':
            if idx + 1 < len(blocks):
                block.successors = [idx + 1]
            else:
                block.successors = []
            if '*' in operands or '%' in operands:
                if 'targets' not in ann:
                    print(f"警告: 第 {last_inst[0]} 行间接调用，假设返回后继续", file=sys.stderr)
        else:
            if idx + 1 < len(blocks):
                block.successors = [idx + 1]
            else:
                block.successors = []

    # 函数识别：以全局标签和顶级非局部标签作为函数入口
    # 收集所有可能为入口的标签（全局标签或不以点开头且非局部）
    candidate_labels = set()
    for label in label_to_block:
        if label in global_labels or (not label.startswith('.') and not label.startswith('.')):
            candidate_labels.add(label)

    functions = []
    for label in candidate_labels:
        entry_idx = label_to_block[label]
        # 收集属于该函数的块：从入口出发，遇到另一个候选标签的块停止
        visited = set()
        stack = [entry_idx]
        while stack:
            bidx = stack.pop()
            if bidx in visited:
                continue
            block = blocks[bidx]
            # 如果当前块有标签且是另一个候选入口，且不是当前函数，则停止（不进入）
            if block.label and block.label in candidate_labels and block.label != label:
                continue
            visited.add(bidx)
            for succ in block.successors:
                stack.append(succ)
        # 创建函数对象
        func = type('Function', (), {})()
        func.name = label
        func.entry_block = entry_idx
        func.blocks = visited
        func.call_convention = 'cdecl'  # 默认
        # 检查入口块是否有调用约定注解
        first_inst = blocks[entry_idx].instructions[0] if blocks[entry_idx].instructions else None
        if first_inst and 'call_conv' in first_inst[4]:
            func.call_convention = first_inst[4]['call_conv']
        functions.append(func)

    return blocks, functions, global_labels

def get_error_vectors():
    env_val = os.environ.get('HIC_ERROR_VECTORS')
    if env_val:
        try:
            return set(int(x.strip()) for x in env_val.split(','))
        except ValueError:
            print("警告: HIC_ERROR_VECTORS 格式无效，使用默认值", file=sys.stderr)
    return {8, 10, 11, 12, 13, 14, 17, 30}

def get_isr_initial_offset(label, bits, error_vectors):
    match = re.match(r'isr_(\d+)', label)
    if not match:
        return 0
    vec = int(match.group(1))
    base = 24 if bits == 64 else 12
    if vec in error_vectors:
        return -(base + 8)
    else:
        return -base

class StackState:
    def __init__(self, offset=0):
        self.offset = offset
    def apply(self, effect):
        return StackState(self.offset + effect)
    def __eq__(self, other):
        return self.offset == other.offset
    def __hash__(self):
        return hash(self.offset)

def analyze_function(func, blocks, stack_effect, syntax, bits, error_vectors):
    start_idx = func.entry_block
    initial_offset = get_isr_initial_offset(func.name, bits, error_vectors)
    initial_state = StackState(initial_offset)

    worklist = deque()
    worklist.append((start_idx, initial_state, [start_idx]))
    visited = set()
    errors = []

    while worklist:
        block_idx, state, path = worklist.popleft()
        key = (block_idx, state)
        if key in visited:
            continue
        visited.add(key)

        block = blocks[block_idx]
        current_state = state

        for (line_num, raw_line, mnemonic, operands, ann) in block.instructions:
            if mnemonic in ('add', 'sub') and is_dynamic_rsp_adjustment(mnemonic, operands, syntax):
                if 'fixed_size' in ann:
                    eff = -ann['fixed_size'] if mnemonic == 'sub' else ann['fixed_size']
                    current_state = current_state.apply(eff)
                else:
                    print(f"警告: 函数 {func.name} 第 {line_num} 行动态调整堆栈指针，无法精确分析，跳过此函数", file=sys.stderr)
                    return []
            elif mnemonic in stack_effect:
                eff = stack_effect[mnemonic]
                if mnemonic == 'ret':
                    imm = parse_ret_imm(operands, syntax)
                    eff += imm
                current_state = current_state.apply(eff)
            elif mnemonic in ('add', 'sub'):
                eff = imm_effect(mnemonic, operands, syntax)
                if eff != 0:
                    current_state = current_state.apply(eff)

        last_inst = block.instructions[-1] if block.instructions else None
        if last_inst:
            last_mnemonic = last_inst[2]
            if last_mnemonic in ('ret', 'iret', 'iretq', 'sysret', 'sysretq'):
                if current_state.offset != 0:
                    path_str = ' -> '.join(str(p) for p in path)
                    errors.append(f"函数 {func.name}: 路径 [{path_str}] 到达返回指令时堆栈不平衡 (偏移 {current_state.offset})")
            elif block.successors:
                for succ in block.successors:
                    worklist.append((succ, current_state, path + [succ]))
        # 无后继且非返回则忽略

    return errors

def main():
    parser = argparse.ArgumentParser(description='静态汇编堆栈平衡检查器')
    parser.add_argument('file', help='汇编源文件')
    args = parser.parse_args()

    lines = preprocess_file(args.file)
    env = detect_env(lines)
    print(f"检测到环境: {env['bits']}-bit, {env['syntax']} 语法", file=sys.stderr)

    blocks, functions, global_labels = parse_asm(lines, env)
    if not functions:
        print("未发现任何函数定义。", file=sys.stderr)
        sys.exit(0)

    error_vectors = get_error_vectors()
    stack_effect = get_stack_effect(env['bits'])

    all_errors = []
    for func in functions:
        # 可选：只分析全局函数或所有函数
        if func.name not in global_labels and func.name.startswith('.'):
            continue
        errors = analyze_function(func, blocks, stack_effect, env['syntax'], env['bits'], error_vectors)
        all_errors.extend(errors)

    if all_errors:
        for err in all_errors:
            print(err)
        sys.exit(1)
    else:
        print(f"{args.file}: 所有函数堆栈平衡检查通过。")
        sys.exit(0)

if __name__ == '__main__':
    main()
