#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查翻译文件完整性"""

import yaml
from pathlib import Path
from collections import defaultdict

SCRIPT_DIR = Path(__file__).parent
TRANSLATIONS_DIR = SCRIPT_DIR / "translations"

# 基础翻译（中文）
BASE_LANG = "zh_CN"
REQUIRED_KEYS = {
    "title", "build", "clean", "install", "help", "about", "exit",
    "language", "languages", "theme", "preset", "presets",
    "scheduler", "memory", "security", "performance", "features",
    "smp", "mmx", "sse", "sse2", "sse3", "ssse3", "sse4_1", "sse4_2",
    "avx", "avx2", "aes", "rdrand", "kaslr", "smep", "smap",
    "max_domains", "max_capabilities", "max_threads"
}

def load_translations():
    """加载所有翻译文件"""
    translations = {}
    for lang_file in TRANSLATIONS_DIR.glob("*.yaml"):
        lang_code = lang_file.stem
        with open(lang_file, 'r', encoding='utf-8') as f:
            translations[lang_code] = yaml.safe_load(f)
    return translations

def check_translations():
    """检查翻译完整性"""
    translations = load_translations()

    print("=" * 60)
    print("翻译文件完整性检查")
    print("=" * 60)

    if BASE_LANG not in translations:
        print(f"✗ 错误: 未找到基础语言文件 {BASE_LANG}.yaml")
        return False

    base_keys = set(translations[BASE_LANG].keys())

    # 检查每个语言文件
    for lang_code, lang_data in translations.items():
        print(f"\n检查 {lang_code}...")

        lang_keys = set(lang_data.keys())

        # 检查缺失的键
        missing = base_keys - lang_keys
        if missing:
            print(f"  ✗ 缺失 {len(missing)} 个翻译键:")
            for key in sorted(missing):
                print(f"    - {key}")
        else:
            print(f"  ✓ 所有翻译键完整")

        # 检查多余的键
        extra = lang_keys - base_keys
        if extra:
            print(f"  ⚠ 多余 {len(extra)} 个翻译键:")
            for key in sorted(extra):
                print(f"    - {key}")

        # 检查 languages 字段格式
        if "languages" in lang_data:
            languages = lang_data["languages"]
            print(f"  语言选项:")
            for code, name in languages.items():
                print(f"    {code}: {name}")

    print("\n" + "=" * 60)
    print("检查完成")
    print("=" * 60)

    return True

if __name__ == "__main__":
    check_translations()
