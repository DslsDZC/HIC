HIK Kernel
=========

The HIK (Hierarchical Isolation Kernel) is a formally verified microkernel with a
three-tier privilege architecture, mathematical proof of security properties,
and runtime invariant checking.

Quick Start
-----------

* Report a bug: See docs/Wiki/FAQ.md
* Get the latest version: https://github.com/your-org/HIK
* Build the kernel: See docs/Wiki/03-QuickStart.md
* View architecture: See docs/Wiki/02-Architecture.md

Essential Documentation
-----------------------

All users should be familiar with:

* Building requirements: docs/Wiki/05-DevelopmentEnvironment.md
* Code standards: docs/Wiki/06-CodingStandards.md
* License: See LICENSE
* Mathematical proofs: src/Core-0/math_proofs.tex

Documentation is available in the docs/ directory.


Who Are You?
============

Find your role below:

* New Kernel Developer - Getting started with HIK development
* Academic Researcher - Studying formal verification and security proofs
* Security Expert - Analyzing capability system and isolation guarantees
* System Administrator - Configuring and troubleshooting
* Privileged Service Developer - Building Privileged-1 layer services
* Application Developer - Creating Application-3 layer applications
* AI Coding Assistant - LLMs and AI-powered development tools


For Specific Users
==================

New Kernel Developer
--------------------

Welcome! Start your HIK development journey here:

* Getting Started: docs/Wiki/03-QuickStart.md
* Build System: docs/Wiki/04-BuildSystem.md
* Core-0 Implementation: docs/Wiki/08-Core0.md
* Capability System: docs/Wiki/11-CapabilitySystem.md
* Security Mechanisms: docs/Wiki/13-SecurityMechanisms.md
* Development Environment: docs/Wiki/05-DevelopmentEnvironment.md

Academic Researcher
-------------------

Explore HIK's formal verification and security proofs:

* Three-Layer Architecture: docs/Wiki/02-Architecture.md
* Mathematical Proofs: src/Core-0/math_proofs.tex
* Formal Verification: src/Core-0/formal_verification.c
* Capability Theory: docs/Wiki/11-CapabilitySystem.md
* Security Guarantees: docs/Wiki/13-SecurityMechanisms.md
* Architecture Abstraction: docs/ARCH_ABSTRACTION.md
* Layer Structure: docs/LAYER_STRUCTURE.md

Security Expert
---------------

Security documentation and verification guides:

* Security Mechanisms: docs/Wiki/13-SecurityMechanisms.md
* Capability System: docs/Wiki/11-CapabilitySystem.md
* Formal Verification: src/Core-0/formal_verification.c
* Audit System: src/Core-0/audit.c
* Domain Isolation: src/Core-0/domain.c
* Compliance Report: docs/COMPLIANCE_REPORT.md

System Administrator
--------------------

Configure, tune, and troubleshoot HIK systems:

* Quick Start: docs/Wiki/03-QuickStart.md
* Build System: docs/Wiki/04-BuildSystem.md
* Troubleshooting: docs/Wiki/FAQ.md
* Boot Configuration: docs/TD/引导加载程序.md
* BIOS/UEFI Support: docs/TD/bios.md, docs/TD/uefi.md

Privileged Service Developer
---------------------------

Build Privileged-1 layer services:

* Service Guide: src/Core-0/examples/PRIVILEGED_SERVICE_GUIDE.md
* Example Service: src/Core-0/examples/example_service.c
* Domain Switching: src/Core-0/domain_switch.c
* IPC Communication: src/Core-0/syscall.c
* Capability Usage: src/Core-0/capability.c

Application Developer
----------------------

Create Application-3 layer applications:

* System Calls: src/Core-0/syscall.c
* Capability System: docs/Wiki/11-CapabilitySystem.md
* API Reference: docs/Wiki/11-CapabilitySystem.md
* Development Guide: docs/Wiki/05-DevelopmentEnvironment.md

AI Coding Assistant
-------------------

CRITICAL: If you are an LLM or AI-powered coding assistant, you MUST read and
follow the HIK coding standards and formal verification requirements before
contributing:

* Coding Standards: docs/Wiki/06-CodingStandards.md
* Formal Verification: src/Core-0/formal_verification.c
* Architecture Compliance: docs/COMPLIANCE_REPORT.md
* Mathematical Proofs: src/Core-0/math_proofs.tex

This documentation contains essential requirements about code completeness,
formal verification compliance, and mathematical proof adherence that all AI
tools must follow.


Core Documentation
=================

* Project Overview: docs/Wiki/01-Overview.md
* Architecture Design: docs/Wiki/02-Architecture.md
* Quick Start Guide: docs/Wiki/03-QuickStart.md
* Build System: docs/Wiki/04-BuildSystem.md
* Development Environment: docs/Wiki/05-DevelopmentEnvironment.md
* Coding Standards: docs/Wiki/06-CodingStandards.md
* Testing Guide: docs/Wiki/07-Testing.md


Advanced Topics
===============

* Formal Verification: src/Core-0/formal_verification.c, src/Core-0/math_proofs.tex
* Capability System: docs/Wiki/11-CapabilitySystem.md
* Security Mechanisms: docs/Wiki/13-SecurityMechanisms.md
* Core-0 Layer: docs/Wiki/08-Core0.md
* Directory Structure: docs/DIRECTORY_STRUCTURE.md


Communication and Support
=========================

* Documentation: docs/Wiki/index.md
* FAQ: docs/Wiki/FAQ.md
* Build Scripts: scripts/build_system.py, scripts/build_system.sh
* License: LICENSE (GPL-2.0)