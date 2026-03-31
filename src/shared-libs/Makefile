# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# HIC 共享库构建 Makefile
# 构建所有兼容层共享库：libchal, libaal, libimal, libpfl

.PHONY: all clean libchal libaal libimal libpfl

# 默认构建所有库
all: libchal libaal libimal libpfl

# 各库构建
libchal:
	$(MAKE) -C libchal

libaal:
	$(MAKE) -C libaal

libimal:
	$(MAKE) -C libimal

libpfl:
	$(MAKE) -C libpfl

# 清理
clean:
	$(MAKE) -C libchal clean
	$(MAKE) -C libaal clean
	$(MAKE) -C libimal clean
	$(MAKE) -C libpfl clean

# 安装到输出目录
install: all
	@echo "Installing shared libraries..."
	@mkdir -p $(OUT_DIR)
	@cp -v libchal/*.hiclib $(OUT_DIR)/ 2>/dev/null || true
	@cp -v libaal/*.hiclib $(OUT_DIR)/ 2>/dev/null || true
	@cp -v libimal/*.hiclib $(OUT_DIR)/ 2>/dev/null || true
	@cp -v libpfl/*.hiclib $(OUT_DIR)/ 2>/dev/null || true
