## Minifier4: The Monolithic Web Performance Optimizer

[![Windows Only](https://img.shields.io/badge/Platform-Windows%20Only-blue?style=flat-square)](https://github.com/juanmalopman/Minifier4)
[![Written in C](https://img.shields.io/badge/Language-C-333333?style=flat-square)](https://github.com/juanmalopman/Minifier4)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A **monolithic, all-in-one solution** built entirely in **C** to transform and optimize static HTML documents for maximum web performance, with a special focus on **First Contentful Paint (FCP)** and **Google's Core Web Vitals**. It performs complex, multi-layered optimizations in **milliseconds**.

---

### ✨ Features

Minifier4 combines numerous web performance best practices into a single, **lightweight** executable. It offers a powerful **native WinAPI GUI** for ease of use and a **robust command-line interface** for automation, making complex optimizations simple and fast.

*   **Blazing Fast & Multi-Threaded:** Written in C, Minifier4 utilizes multi-threading to achieve transformation and compilation in **milliseconds**.
*   **Monolithic All-in-One Solution:** A single executable with **zero external dependencies** or build tools (no Node.js, Webpack, etc.) required.
*   **Dual Interface (GUI & CLI):** A **native WinAPI graphical interface** is available for intuitive usage, while the full functionality remains **command-line addressable** for build scripts and CI/CD pipelines.
*   **Declarative Critical CSS Generation:** CSS splitting is triggered by a simple `<!-- above-the-fold -->` comment in your HTML, automatically extracting and inlining the Critical CSS into the `<head>` while deferring the rest.
*   **Resource Splitting & Deferred Loading:** Splits the remaining (under-the-fold) CSS and JavaScript, automatically applying deferred loading techniques to prevent render-blocking.
*   **Full Minification & Compression:** Compresses HTML, inlined CSS, and inlined JavaScript by removing comments, collapsing whitespace, and applying structural optimizations.
*   **Resource Inlining:** Joins and inlines the critical parts of external CSS and JavaScript files directly into the HTML to minimize HTTP requests.

---

### 💻 Platform & Requirements

| Detail | Description |
| :--- | :--- |
| **Platform** | **Windows ONLY** (Native executable) |
| **Dependencies** | None (Standalone Executable) |
| **Language** | C |

---

### 🚀 Getting Started

#### Installation

1.  Download the latest executable (`Minifier4.exe`) from the [**Releases page**](https://github.com/juanmalopman/Minifier4/releases).
2.  *(Optional but Recommended)* Add the directory containing `Minifier4.exe` to your Windows System PATH environment variable for easy CLI access.

#### Usage Options

Minifier4 provides two ways to optimize your files:

1.  **Graphical User Interface (GUI):**
    *   Double-click `Minifier4.exe` to launch the native WinAPI application.
    *   Use the file selector or **drag-and-drop** an HTML file onto the window.
    *   Configure options and click **Go !**.
    *   *(Note: Running the program without any command-line arguments will automatically launch the GUI.)*

2.  **Command Line Interface (CLI):**
    *   Ideal for automated build scripts or batch processing.
    *   Run `Minifier4.exe --help` or `-h` in your terminal to view the current list of available arguments and automation options.

---

### 💡 Why Minifier4?

Traditional web performance optimization often requires chaining together 5-10 separate tools (a minifier, a critical CSS generator, an inliner, a JS compressor, and a build system like Webpack/Gulp).

Minifier4's **monolithic** approach:

1.  **Eliminates overhead** from inter-process communication between tools.
2.  **Reduces build complexity** to a single, fast command.
3.  **Delivers unmatched speed** because it is purpose-built in C.

The **Dual Interface** approach means you can easily test and configure settings using the native GUI, and then deploy the exact same logic with the same speed in your automated build system using the CLI.

This translates directly into a faster, simpler optimization step in your build process, and **superior Core Web Vitals** for your final users.

---

### 🤝 Contributing

**I am not currently accepting code contributions (Pull Requests) to the Minifier4 codebase.**

Minifier4 is currently a solo project, and maintaining a high standard of code quality and architectural integrity is a top priority.

However, your feedback is invaluable!

*   **Bug Reports:** If you find an issue, please [open a bug report on GitHub](https://github.com/juanmalopman/Minifier4/issues).
*   **Feature Suggestions:** If you have an idea for an improvement, please [open a feature request on GitHub](https://github.com/juanmalopman/Minifier4/issues).

---

### 📄 License

Distributed under the **MIT License**. See `LICENSE` for more information.

---

### ⚒️ Building

Minifier4 is **IDE-agnostic** and **compiler-agnostic** (though it includes instructions for CMake). The repository includes a zero-dependency `build.bat` system. Simply ensure your preferred compiler is in your system PATH and run:

*   `build.bat /gcc /debug`
*   `build.bat /clang /release`
*   `build.bat /msvc /release`

---

### 📞 Contact

Juan Manuel López Manzano

Project Link: [https://github.com/juanmalopman/Minifier4](https://github.com/juanmalopman/Minifier4)