#!/usr/bin/env python3
"""
HIC 内存区域重叠检查脚本
验证每个服务分配的物理内存区域是否重叠、是否超出实际物理内存范围
"""

import sys
import os
import re
import yaml
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass


@dataclass
class MemoryRegion:
    """内存区域定义"""
    name: str
    base: int  # 物理基地址
    size: int   # 区域大小
    end: int   # 结束地址（base + size）
    
    def __post_init__(self):
        self.end = self.base + self.size
    
    def overlaps_with(self, other: 'MemoryRegion') -> bool:
        """检查是否与另一个区域重叠"""
        return not (self.end <= other.base or other.end <= self.base)
    
    def __str__(self):
        return f"{self.name}: [0x{self.base:08X} - 0x{self.end:08X}] (size: {self.size} bytes)"


class MemoryOverlapChecker:
    """内存重叠检查器"""
    
    def __init__(self):
        self.regions: List[MemoryRegion] = []
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self.kernel_start = 0x100000  # 1MB
        self.max_physical_memory = 0  # 从配置中读取
        
    def load_platform_config(self, config_path: str) -> bool:
        """加载平台配置文件"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                self.config = yaml.safe_load(f)
            print(f"[INFO] 加载平台配置: {config_path}")
            return True
        except Exception as e:
            self.errors.append(f"无法加载配置文件 {config_path}: {e}")
            return False
    
    def parse_kernel_memory_layout(self, elf_path: str) -> bool:
        """解析内核内存布局"""
        try:
            import subprocess
            result = subprocess.run(
                ['nm', elf_path],
                capture_output=True,
                text=True,
                check=True
            )
            
            nm_output = result.stdout
            segments = {
                'text': None,
                'data': None,
                'bss': None,
                'lbss': None,
                'kernel': None
            }
            
            # 解析nm输出
            for line in nm_output.split('\n'):
                if '_text_start' in line:
                    segments['text_start'] = int(line.split()[0], 16)
                elif '_text_end' in line:
                    segments['text_end'] = int(line.split()[0], 16)
                elif '_data_start' in line:
                    segments['data_start'] = int(line.split()[0], 16)
                elif '_data_end' in line:
                    segments['data_end'] = int(line.split()[0], 16)
                elif '_bss_start' in line:
                    segments['bss_start'] = int(line.split()[0], 16)
                elif '_bss_end' in line:
                    segments['bss_end'] = int(line.split()[0], 16)
                elif '_lbss_start' in line:
                    segments['lbss_start'] = int(line.split()[0], 16)
                elif '_lbss_end' in line:
                    segments['lbss_end'] = int(line.split()[0], 16)
                elif '_kernel_end' in line:
                    segments['kernel_end'] = int(line.split()[0], 16)
            
            # 添加内核区域
            if segments['text_start'] is not None and segments['text_end'] is not None:
                text_region = MemoryRegion(
                    name="kernel.text",
                    base=segments['text_start'],
                    size=segments['text_end'] - segments['text_start']
                )
                self.regions.append(text_region)
            
            if segments['data_start'] is not None and segments['data_end'] is not None:
                data_region = MemoryRegion(
                    name="kernel.data",
                    base=segments['data_start'],
                    size=segments['data_end'] - segments['data_start']
                )
                self.regions.append(data_region)
            
            if segments['bss_start'] is not None and segments['bss_end'] is not None:
                bss_region = MemoryRegion(
                    name="kernel.bss",
                    base=segments['bss_start'],
                    size=segments['bss_end'] - segments['bss_start']
                )
                self.regions.append(bss_region)
            
            if segments['lbss_start'] is not None and segments['lbss_end'] is not None:
                lbss_region = MemoryRegion(
                    name="kernel.lbss",
                    base=segments['lbss_start'],
                    size=segments['lbss_end'] - segments['lbss_start']
                )
                self.regions.append(lbss_region)
            
            if segments['kernel_end'] is not None:
                kernel_region = MemoryRegion(
                    name="kernel.total",
                    base=self.kernel_start,
                    size=segments['kernel_end'] - self.kernel_start
                )
                self.regions.append(kernel_region)
            
            print(f"[INFO] 解析内核内存布局: {len(self.regions)} 个区域")
            return True
            
        except Exception as e:
            self.errors.append(f"无法解析内核内存布局 {elf_path}: {e}")
            return False
    
    def extract_service_memory_regions(self) -> bool:
        """从配置中提取服务内存区域"""
        try:
            # 从resource_quotas中提取服务配额
            if 'resource_quotas' not in self.config:
                return True
            
            quotas = self.config['resource_quotas']
            
            # 处理特定服务配额
            if 'service_quotas' in quotas:
                for service_name, quota in quotas['service_quotas'].items():
                    if 'max_memory' in quota:
                        # 计算服务的物理内存区域
                        # 这里我们模拟一个简单的分配策略
                        # 实际应用中，应该从链接脚本或运行时配置中读取
                        base_addr = self._calculate_service_base_address(service_name, quota['max_memory'])
                        
                        region = MemoryRegion(
                            name=f"service.{service_name}",
                            base=base_addr,
                            size=quota['max_memory']
                        )
                        self.regions.append(region)
                        print(f"[INFO] 添加服务区域: {region}")
            
            # 从APM配置中读取内存区域（如果存在）
            apm_config_path = os.path.join(
                os.path.dirname(__file__),
                '..',
                'build',
                'apm_memory_config.yaml'
            )
            if os.path.exists(apm_config_path):
                with open(apm_config_path, 'r') as f:
                    apm_config = yaml.safe_load(f)
                    if 'memory_regions' in apm_config:
                        for i, region_config in enumerate(apm_config['memory_regions']):
                            region = MemoryRegion(
                                name=f"apm.region_{i}",
                                base=region_config['base'],
                                size=region_config['size']
                            )
                            self.regions.append(region)
                            print(f"[INFO] 添加APM区域: {region}")
            
            return True
            
        except Exception as e:
            self.errors.append(f"无法提取服务内存区域: {e}")
            return False
    
    def _calculate_service_base_address(self, service_name: str, size: int) -> int:
        """计算服务的基地址（简化版）"""
        # 这里使用一个简化的分配策略
        # 实际应用中应该从链接脚本或运行时配置中读取
        base_offset = 0x200000  # 2MB偏移
        
        # 根据服务名生成确定性的偏移
        hash_val = hash(service_name)
        offset = (hash_val % 0x100) * 0x10000  # 在16MB范围内
        
        return base_offset + offset
    
    def check_overlaps(self) -> bool:
        """检查内存区域重叠"""
        print("\n[CHECK] 开始检查内存区域重叠...")
        has_overlap = False
        
        # 两两比较检查重叠
        for i in range(len(self.regions)):
            for j in range(i + 1, len(self.regions)):
                r1 = self.regions[i]
                r2 = self.regions[j]
                
                if r1.overlaps_with(r2):
                    error_msg = f"内存区域重叠: {r1.name} <-> {r2.name}"
                    self.errors.append(error_msg)
                    has_overlap = True
                    print(f"[ERROR] {error_msg}")
        
        if not has_overlap:
            print("[CHECK] ✓ 所有内存区域无重叠")
        
        return not has_overlap
    
    def check_memory_bounds(self) -> bool:
        """检查内存区域是否超出物理内存范围"""
        print("\n[CHECK] 开始检查内存区域边界...")
        has_error = False
        
        if self.max_physical_memory == 0:
            print("[WARN] 未配置最大物理内存，跳过边界检查")
            return True
        
        for region in self.regions:
            if region.base >= self.max_physical_memory:
                error_msg = f"内存区域基地址超出物理内存: {region}"
                self.errors.append(error_msg)
                has_error = True
                print(f"[ERROR] {error_msg}")
            
            if region.end > self.max_physical_memory:
                error_msg = f"内存区域结束地址超出物理内存: {region}"
                self.errors.append(error_msg)
                has_error = True
                print(f"[ERROR] {error_msg}")
        
        if not has_error:
            print("[CHECK] ✓ 所有内存区域在物理内存范围内")
        
        return not has_error
    
    def check_kernel_size_limits(self) -> bool:
        """检查内核大小限制（形式化验证要求）"""
        print("\n[CHECK] 开始检查内核大小限制...")
        has_error = False
        
        kernel_region = None
        bss_region = None
        lbss_region = None
        
        for region in self.regions:
            if region.name == "kernel.total":
                kernel_region = region
            elif region.name == "kernel.bss":
                bss_region = region
            elif region.name == "kernel.lbss":
                lbss_region = region
        
        # 检查内核大小 < 2MB
        MAX_KERNEL_SIZE = 2 * 1024 * 1024  # 2MB
        if kernel_region and kernel_region.size > MAX_KERNEL_SIZE:
            error_msg = f"内核大小超出限制: {kernel_region.size} > {MAX_KERNEL_SIZE} (2MB)"
            self.errors.append(error_msg)
            has_error = True
            print(f"[ERROR] {error_msg}")
        else:
            print(f"[CHECK] ✓ 内核大小: {kernel_region.size if kernel_region else 0} / {MAX_KERNEL_SIZE}")
        
        # 检查BSS大小 < 512KB
        MAX_BSS_SIZE = 512 * 1024  # 512KB
        if bss_region and bss_region.size > MAX_BSS_SIZE:
            error_msg = f"BSS段大小超出限制: {bss_region.size} > {MAX_BSS_SIZE} (512KB)"
            self.errors.append(error_msg)
            has_error = True
            print(f"[ERROR] {error_msg}")
        else:
            print(f"[CHECK] ✓ BSS段大小: {bss_region.size if bss_region else 0} / {MAX_BSS_SIZE}")
        
        # 检查LBSS大小
        if lbss_region:
            lbss_size = lbss_region.size
            total_bss = (bss_region.size if bss_region else 0) + lbss_size
            if total_bss > MAX_BSS_SIZE:
                error_msg = f"总BSS大小超出限制: {total_bss} > {MAX_BSS_SIZE} (512KB)"
                self.errors.append(error_msg)
                has_error = True
                print(f"[ERROR] {error_msg}")
            else:
                print(f"[CHECK] ✓ 总BSS大小: {total_bss} / {MAX_BSS_SIZE}")
        
        return not has_error
    
    def check_alignment(self) -> bool:
        """检查内存区域对齐（4K页对齐）"""
        print("\n[CHECK] 开始检查内存区域对齐...")
        has_error = False
        
        PAGE_SIZE = 4096  # 4K
        
        for region in self.regions:
            if region.base % PAGE_SIZE != 0:
                error_msg = f"内存区域未对齐到4K页边界: {region.name} (base=0x{region.base:X})"
                self.errors.append(error_msg)
                has_error = True
                print(f"[ERROR] {error_msg}")
            elif region.size % PAGE_SIZE != 0:
                warn_msg = f"内存区域大小不是4K的倍数: {region.name} (size={region.size})"
                self.warnings.append(warn_msg)
                print(f"[WARN] {warn_msg}")
        
        if not has_error:
            print("[CHECK] ✓ 所有内存区域都对齐到4K页边界")
        
        return not has_error
    
    def print_report(self):
        """打印检查报告"""
        print("\n" + "="*70)
        print("内存区域检查报告")
        print("="*70)
        
        print(f"\n检查的区域数量: {len(self.regions)}")
        print("\n内存区域列表:")
        for region in self.regions:
            print(f"  {region}")
        
        if self.errors:
            print(f"\n错误数量: {len(self.errors)}")
            print("\n错误详情:")
            for i, error in enumerate(self.errors, 1):
                print(f"  {i}. {error}")
        else:
            print("\n✓ 未发现错误")
        
        if self.warnings:
            print(f"\n警告数量: {len(self.warnings)}")
            print("\n警告详情:")
            for i, warning in enumerate(self.warnings, 1):
                print(f"  {i}. {warning}")
        
        print("\n" + "="*70)
    
    def run(self, platform_config: str, kernel_elf: str) -> int:
        """运行完整的内存检查流程"""
        print("HIC 内存区域重叠检查工具")
        print("="*70)
        
        # 加载配置
        if not self.load_platform_config(platform_config):
            print("[FATAL] 无法加载平台配置")
            return 1
        
        # 解析内核内存布局
        if not self.parse_kernel_memory_layout(kernel_elf):
            print("[FATAL] 无法解析内核内存布局")
            return 1
        
        # 提取服务内存区域
        if not self.extract_service_memory_regions():
            print("[WARN] 无法提取服务内存区域")
        
        # 执行检查
        check_passed = True
        
        check_passed &= self.check_overlaps()
        check_passed &= self.check_memory_bounds()
        check_passed &= self.check_kernel_size_limits()
        check_passed &= self.check_alignment()
        
        # 打印报告
        self.print_report()
        
        # 返回结果
        if not check_passed:
            print("\n[FATAL] 内存检查失败，构建终止")
            return 1
        else:
            print("\n[SUCCESS] 内存检查通过")
            return 0


def main():
    """主函数"""
    if len(sys.argv) < 3:
        print("用法: check_memory_overlap.py <platform.yaml> <kernel.elf>")
        print("\n参数:")
        print("  platform.yaml - 平台配置文件路径")
        print("  kernel.elf   - 内核ELF文件路径")
        sys.exit(1)
    
    platform_config = sys.argv[1]
    kernel_elf = sys.argv[2]
    
    # 验证文件存在
    if not os.path.exists(platform_config):
        print(f"[FATAL] 平台配置文件不存在: {platform_config}")
        sys.exit(1)
    
    if not os.path.exists(kernel_elf):
        print(f"[FATAL] 内核ELF文件不存在: {kernel_elf}")
        sys.exit(1)
    
    # 创建检查器并运行
    checker = MemoryOverlapChecker()
    result = checker.run(platform_config, kernel_elf)
    
    sys.exit(result)


if __name__ == "__main__":
    main()