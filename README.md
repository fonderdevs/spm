<div align="center">

# 🚀 SPM 🚀
### A lightweight package manager for Linux

![Version](https://img.shields.io/badge/version-2.0.5-blue)
![Author](https://img.shields.io/badge/Author-parkourer10-purple)
![Tester](https://img.shields.io/badge/Author-eetuilia-green)

</div>

## 📦 About

spm is a simple and efficient package manager designed for FonderOS.

## 🛠️ Installation

#### Clone the repository:

```bash
git clone https://github.com/fonderdevs/spm.git
cd spm
```

#### Install the package manager:

```bash
make
make install
```

#### Uninstall the package manager:
```bash
make uninstall
make clean
```
## 📦 Usage

#### Basic Usage:

```bash
spm install <package>
```

#### Remove a package:

```bash
spm remove <package>
```

#### Update package repositories:

```bash
spm update
```

#### Upgrade installed packages:

```bash
spm upgrade
```

#### Search for packages:

```bash
spm search <term>
```

#### Switch between versions of a package:

```bash
spm switch-version <package> <version>
```

## 🔄 Managing Multiple Package Versions

spm supports managing multiple versions of the same package (like Java 8, Java 17, Java 21) and easily switching between them.

### Example: Managing Java Versions

If you have multiple Java versions installed:

```bash
# Install different Java versions
spm install openjdk-8
spm install openjdk-17
spm install openjdk-21

# Switch to Java 8 as the default
spm switch-version java 8

# Later, switch to Java 21
spm switch-version java 21
```

When you switch versions:
- spm creates symbolic links from the specific version to the generic name
- Applications that use the generic command (e.g., `java`) will use your selected version
- You can keep multiple versions installed simultaneously
- No need to manually modify PATH or environment variables


## 📦 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 📦 Contributing

Contributions are welcome! Please open an issue or submit a pull request.
