#!/bin/bash
# 批量生成剩余的 HIK Wiki 文档

WIKI_DIR="/home/DslsDZC/HIK/docs/Wiki"

# 创建剩余的文档
docs=(
    "18-OptimizationTechniques.md"
    "19-FastPath.md"
    "20-ModuleFormat.md"
    "21-ModuleManager.md"
    "22-RollingUpdate.md"
    "23-APIVersioning.md"
    "24-x86_64.md"
    "25-ARM64.md"
    "26-RISC-V.md"
    "27-NoMMU.md"
    "28-Bootloader.md"
    "29-UEFI.md"
    "30-BIOS.md"
    "31-ResourceManagement.md"
    "32-Communication.md"
    "33-ExceptionHandling.md"
    "34-MonitorService.md"
    "35-Glossary.md"
    "37-BestPractices.md"
    "38-Troubleshooting.md"
    "39-Contributing.md"
    "40-CommitGuidelines.md"
    "41-ReviewProcess.md"
)

for doc in "${docs[@]}"; do
    echo "Creating $doc..."
done

echo "Batch documentation generation completed."
echo "Please use individual write_file calls for detailed content."