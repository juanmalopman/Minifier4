# Minifier v4.0 Modernization Roadmap

## ❓ Why v4.0?
v3 was a high-performance, multi-threaded tool, but having been originally *authored seven years ago*, it reflected the tooling and constraints of that era. The project relied on Visual Studio–specific workflows, a brittle custom-draw UI, and—while compiled as C++—was architected largely as C code, using C++ features opportunistically. This placed the codebase in an uncomfortable middle ground between the two languages, complicating maintenance and long-term evolution.

v4.0 is a clean rewrite targeting **strict C23 compliance**. It standardizes on explicit C memory semantics, moves the build system away from IDE lock-in, prioritizes code maintainability and brings unlimited input sizes over the rigid stack-based optimizations of v3.

**New Capabilities in v4:**
*   **Headless Mode:** Ability to run without initializing a GUI.
*   **Granular Configuration:** Expanded settings via both GUI and CLI.
*   **Mangling Control:** Support for explicit directives (e.g., `/*no_mangle*/`) for Android/JS interfaces.

## ⛔ Scope & Non-Goals
*   **Platform:** strictly **Windows 10 and newer**. While the core logic is standard C23, no effort will be made to port the application or build system to Linux/macOS.
*   **Syntax Validation:** The parser assumes valid HTML input. It will not perform "sanity checks" (e.g., missing brackets) or fix broken markup; garbage in, garbage out.
*   **Auto-Update:** No internal update checking mechanism is planned.
*   **Folding Logic:** "Above/Below the fold" separation is determined strictly by manual comments (e.g., `/*fold*/`), not by calculated layout analysis.

## 🚧 Phase 1: Infrastructure & UX (Current Focus)
*Goal: A native, High-DPI aware, dark-mode enabled interface.*
- [x] **Build System Migration:**
    - Decouple from `.sln` files.
    - Implement a compiler-agnostic CMake configuration (MSVC/Clang/GCC support).
- [x] **Modern GUI Architecture:**
    - Replace v3's "Custom Draw" with standard `Comctl32` themed controls.
    - Implement **Per-Monitor v2 DPI Awareness** for crisp rendering on 4K screens.
- [x] **Enhanced Input/Output:**
    - Integrate `IFileOpenDialog` (Common Item Dialog) for native folder/file selection.
    - Add explicit UI controls for Input File and Output Path selection.

## ⚙️ Phase 2: Core Logic (The Engine)
*Goal: Re-architecting the logic of v3 into structured, maintainable C.*
- [x] **Test Harness (TDD):**
    - Implement a data-driven test runner to verify inputs against expected outputs.
    - Establish a handwritten "Gold Standard" suite to prevent regression during the rewrite.
- [ ] **Memory Model Refactor:**
    - Transition from v3's static/stack buffers to **Dynamic Heap Allocation**.
    - Ensure scalability for large HTML files.
- [ ] **State Machine Decomposition:**
    - Replace v3's parsing with a more modular Finite State Machine.
    - *Goal:* Improve readability and reduce cyclomatic complexity.
- [ ] **Smart Mangling & Preservation:**
    - Reduce reliance on v3's javascript `/*class_next*/` style markers.
    - Implement `/*no_mangle*/` directive support for public API functions.
- [x] **CLI Hardening:**
    - Decouple the argument parser from the GUI entry point.
    - Implement a "Headless Mode" that skips GDI initialization entirely.

## 🚀 Phase 3: Workflow Integrations
*Goal: Quality of Life features not present in v3.*
- [ ] **UX / Help:**
    - Add a "Legend" or Help Dialog to explain supported keywords like `/*fold*/` or `/*no_mangle*/`.
- [ ] **External Tools Integration:**
    - Add "Open in Editor" button for the Rich Edit views.
    - Add "Preview in Browser" buttons.
- [ ] **Result Verification:**
    - Visual feedback on compression ratio (Original Size vs. Minified Size).