# Nutshell (nsh) - A Modern, Feature-Rich Shell for Developers

![GitHub License](https://img.shields.io/github/license/Turtlefes/Nutshell-nsh-) 
![GitHub Issues](https://img.shields.io/github/issues/Turtlefes/Nutshell-nsh-) 
![Open PRs](https://img.shields.io/github/issues-pr/Turtlefes/Nutshell-nsh-) 
![GitHub Stars](https://img.shields.io/github/stars/Turtlefes/Nutshell-nsh-?style=social) 
![GitHub Forks](https://img.shields.io/github/forks/Turtlefes/Nutshell-nsh-?style=social) 
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Release](https://img.shields.io/github/v/release/Turtlefes/Nutshell-nsh-)
![Build Status](https://img.shields.io/github/actions/workflow/status/Turtlefes/Nutshell-nsh-/build.yml?branch=main)
![Languages](https://img.shields.io/github/languages/top/Turtlefes/Nutshell-nsh-)


## 🚀 A Next-Generation Shell Experience

**Nutshell** (or **nsh**) is an innovative, modern shell environment meticulously crafted by a passionate 14-year-old developer. This isn't just another shell - it's a powerful, bash-compatible command interpreter that combines traditional UNIX philosophy with contemporary development features. Designed for both educational purposes and practical use, nsh offers an intuitive, feature-rich alternative to conventional shells.

## ✨ Standout Features

### 🧠 Advanced Bash-like Syntax Support

- **Comprehensive Command Parsing**: Experience near-seamless bash compatibility with sophisticated command interpretation
- **Beta Syntax Expansion**: Continuously evolving syntax support that grows with community feedback
- **Intelligent Tokenization**: Advanced parsing algorithms that handle complex command structures gracefully

### ⌨️ Intelligent Line Editing (Readline Integration)

- **Advanced Editing**: Full readline support with custom key bindings
- **History Management**: Persistent command history across sessions

### 🔧 Built-in Command Suite

- **Essential Utilities**: Comprehensive set of built-in commands for daily operations
- **Performance Optimized**: Lightning-fast execution for common tasks

### 🔄 Advanced Execution Features

- **Command Substitution**: Full support for nested command execution
- **Arithmetic Expansion**: Powerful mathematical evaluation with $((expression)) syntax
- **Complex Expressions**: Support for advanced mathematical operations including exponents, modulo, and bitwise operation
- **File execution**: Support for file execution like ```./your_script.sh``` and also you can use ```.nshrc``` and make it like a bash

### 🛡️ Robust Error Handling

- **Descriptive Error Messages**: Helpful feedback for debugging and learning
- **Graceful Failure Recovery**: Continuous operation after command errors
- **Syntax Validation**: Early detection of potential issues

### 🏗️ Technical Excellence

Built with Modern **C++17**: Leveraging the latest language features for:

- **Memory Safety**: Smart pointers and RAII patterns
- **Performance Optimization**: Efficient algorithms and data structures
- **Maintainable Code**: Clean, well-structured architecture

## 📦 Installation Made Simple

Get started with nsh in just a few steps:

```bash
# Clone the repository
git clone 'https://github.com/Turtlefes/Nutshell-nsh-'

# Navigate to project directory
cd 'Nutshell-nsh-'

# Build and install (requires make and C++17 compiler)
make install
```

### System Requirements

- **C++17** Compatible Compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- GNU Make or compatible build system
- Readline Development Libraries
- Standard UNIX Build Tools

## 🎮 Quick Start Guide

Once installed, launch nsh and experience the difference:

```bash
# Start the shell
nsh

# Try advanced features
echo "Math result: $((3 * (2**2 + 1)))"
# Output: Math result: 15

# Use command substitution
echo "Today is $(date +%A)"
# Output: Today is [current day]

# Experience more feature as far as you can!
```

## 🌟 Why Choose Nutshell?

### For Learners:

- Educational Design: Perfect for understanding shell internals
- Transparent Operation: Clear feedback on parsing and execution
- Safe Environment: Robust error handling prevents catastrophic mistakes

### For Developers:

- Bash Compatibility: Easy transition from existing workflows
- Modern Features: Contemporary improvements on traditional shell concepts
- Extensible Architecture: Build your own features and commands

### For System Administrators:

- Reliable Performance: Stable operation in production environments
- Familiar Syntax: Quick adoption for experienced shell users
- Advanced Scripting: Powerful capabilities for automation tasks

## 🤝 Community and Contribution

Join our growing community of shell enthusiasts! As a project created by a young developer, nsh thrives on:

- Community Feedback: Your ideas shape the future of nsh
- Educational Opportunities: Learn about shell development through our well-commented code
- Collaborative Development: Open to contributions of all experience levels

### How to Contribute:

1. Fork the repository
2. Create a feature branch
3. Submit a pull request with clear documentation
4. Join discussions in issues and feature requests

## 📊 Project Status

Current Version: *0.3.8.65* - Actively Developed

- **Core Features**: Stable and functional
- **Syntax Compatibility**: Expanding regularly
- **Performance**: Continuously optimized (Beta)
- **Documentation**: Growing with the project

## 🐛 Reporting Issues

Found a bug or have a feature request? We welcome:

- Detailed bug reports with reproduction steps
- Feature suggestions with use cases
- Documentation improvements
- Performance optimization ideas

## 📄 License

Nutshell is released under the GNU General Public License v3.0 (GPLv3)

- Ensures freedom to use, modify, and distribute
- Requires derivative works to remain open source
- Protects developer and user rights

## 🙌 Acknowledgments

Created with passion by a 14-year-old developer committed to:

- Open source education and accessibility
- Learning through practical project development
- Building tools that empower users
- Contributing to the open source ecosystem

## Final

**Experience the future of shell environments today with Nutshell - where traditional UNIX power meets modern development excellence!**

Join us in reshaping the command line experience for the next generation of developers and system administrators.