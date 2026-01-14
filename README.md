## Minifier4: The Monolithic Web Performance Optimizer

[![Windows Only](https://img.shields.io/badge/Platform-Windows%20Only-blue?style=flat-square)](https://github.com/juanmalopman/Minifier4)
[![Written in C](https://img.shields.io/badge/Language-C-333333?style=flat-square)](https://github.com/juanmalopman/Minifier4)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A **monolithic, all-in-one solution** built entirely in **C** to transform and optimize static HTML documents for maximum web performance, with a special focus on **First Contentful Paint (FCP)** and **Google's Core Web Vitals**. It performs complex, multi-layered optimizations in **milliseconds**.

---

### ✨ Features

Minifier4 combines numerous web performance best practices into a single, **lightweight** executable. It offers a powerful **native WinAPI GUI** for ease of use and a **robust command-line interface** for automation.

*   **Blazing Fast & Multi-Threaded:** Written in C, Minifier4 utilizes multi-threading to achieve transformation and compilation in **milliseconds**.
*   **Smart Single-Instance Architecture:** The CLI and GUI are integrated. If you run a command while the GUI is open, arguments are **forwarded to the existing instance** instantly, allowing for seamless control without spawning overhead.
*   **Monolithic All-in-One Solution:** A single executable with **zero external dependencies** or build tools (no Node.js, Webpack, etc.) required.
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

1.  Download the latest executable (`Minifier4.exe`) from the [**Releases page**](https://github.com/juanmalopman/Minifier4/releases). *(Coming soon)*
2.  *(Optional but Recommended)* Add the directory containing `Minifier4.exe` to your Windows System PATH environment variable for easy CLI access.

#### Usage Options

Minifier4 provides two ways to optimize your files:

1.  **Graphical User Interface (GUI):**
    *   Double-click `Minifier4.exe` to launch the native WinAPI application.
    *   Use the file selector to select an HTML, CSS or JS input file. If HTML, linked files are processed automatically. 
    *   Configure options and click **Go !**.
    *   *(Note: Running the program without any command-line arguments will automatically launch the GUI.)*

2.  **Command Line Interface (CLI):**
    *   Ideal for automated build scripts or batch processing.
    *   **Forwarding:** If Minifier4 is already running (e.g., the GUI is open), calling the CLI will forward commands to that running instance.

---

### 📖 Command Line Reference

Usage: `Minifier4.exe [options]`
Options are case-insensitive and order-independent.

```text
GENERAL:
  --help, -h                   Show this help information.
  --run, -r                    Execute minification immediately.
  --headless, -hd              Execute minification and exit (no GUI).
  --close, -c                  Exit.

CONFIGURATION:
  --load, -l                   Load last saved settings.
  --save, -s                   Save current settings.
  --default, -d                Reset settings to defaults.
                               (If input is set, default output becomes:
                               <filename>_min.<ext>)

INPUT / OUTPUT:
  --input, -i <PATH>           Specify input file path.
  --out-path, -op <PATH>       Specify output directory.
  --out-file, -of <NAME>       Specify custom output filename.
  --out-strip, -os <TEXT>      Text segment to remove from output path.
  --no-out-file, -nof          Dry run (do not write to disk).

MODE:
  --HTML                       Process only if HTML (and linked assets).
  --CSS                        Process only if CSS.
  --JS                         Process only if JavaScript.
  --auto-detect, -ad           Process HTML, CSS or JS inputs.

PROCESSING BEHAVIOR:
  --mangle, -m                 Enable name mangling (IDs, vars).
  --no-mangle, -nm             Disable name mangling.
  --random-mangle, -rm         Enable randomized mangling strings.
  --no-random-mangle, -nrm     Disable randomized mangling strings.
  --fallback, -f               Enable fallback to last success on error.
  --no-fallback, -nf           Disable fallback on error.