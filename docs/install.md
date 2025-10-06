# 🐚 Nutshell Installation on Linux Distributions

**You can install Nutshell directly from the source code.**

---

### ⚙️ Prerequisites
Make sure you already have:
- **Git** — to clone the repository  
- **Make** and **C++ compiler** (such as `g++` or `clang`) — to build the project

**Install them (for Linux):**
```bash
sudo apt install git make g++ -y
```

**For Termux (Android):**
```bash
pkg install git make clang -y
```

---

### 🧩 Clone the Repository
> Note: you must install `git` first before cloning  
> On Termux, you can just use:
> ```bash
> pkg install git -y
> ```

```bash
# 1. Clone repository
git clone https://github.com/Turtlefes/Nutshell-nsh-.git

# 2. Navigate to project directory
cd Nutshell-nsh-
```

---

### 🏗️ Build & Install the Project
```bash
# Compile and install Nutshell
make install
```

If the Makefile doesn’t have a default install target, try:
```bash
make
sudo make install
```

---

### 🧠 Run Nutshell
After installation, you can run it directly:
```bash
nsh
```

If it doesn’t run, check if it’s inside the build directory:
```bash
./nsh
```

### 🧾 Notes
- Installation paths may differ depending on your system (`/usr/local/bin` or `/data/data/com.termux/files/usr/bin`)
- Use `make clean` to remove build files
- If compilation fails, ensure all required dependencies are installed
