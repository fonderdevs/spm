<div align="center">

# 🚀 Steal Package Manager 🚀
### A lightweight package manager for FonderOS

![Version](https://img.shields.io/badge/version-2.0.4-blue)
![Author](https://img.shields.io/badge/Author-parkourer10-purple)

</div>

## 📦 About

Steal is a simple and efficient package manager designed for FonderOS.

## 🛠️ Installation

#### Clone the repository:

```bash
git clone https://github.com/fonderdevs/steal.git
cd steal
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
steal install <package>
```

#### Remove a package:

```bash
steal remove <package>
```

#### Update package repositories:

```bash
steal update
```

#### Upgrade installed packages:

```bash
steal upgrade
```

#### Search for packages:

```bash
steal search <term>
```

#### Switch between versions of a package:

```bash
steal switch-version <package> <version>
```

## 🔄 Managing Multiple Package Versions

Steal supports managing multiple versions of the same package (like Java 8, Java 17, Java 21) and easily switching between them.

### Example: Managing Java Versions

If you have multiple Java versions installed:

```bash
# Install different Java versions
steal install openjdk8
steal install openjdk17
steal install openjdk21

# Switch to Java 8 as the default
steal switch-version java 8

# Later, switch to Java 21
steal switch-version java 21
```

When you switch versions:
- Steal creates symbolic links from the specific version to the generic name
- Applications that use the generic command (e.g., `java`) will use your selected version
- You can keep multiple versions installed simultaneously
- No need to manually modify PATH or environment variables


## 📦 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 📦 Contributing

Contributions are welcome! Please open an issue or submit a pull request.
