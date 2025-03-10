<div align="center">

# 🚀 Steal Package Manager 🚀
### A lightweight package manager for FonderOS

![Version](https://img.shields.io/badge/version-2.0.3-blue)
![Author](https://img.shields.io/badge/Author-parkourer10-purple)

</div>

## 📦 About

Steal is a simple and efficient package manager designed for FonderOS.

## 🛠️ Installation

#### Clone the repository:

```bash
git clone https://github.com/fonderdevs/steal.git
cd steal-pkg-manager
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

#### Update a package:

```bash
steal update <package>
```

#### List installed packages:

```bash
steal list
```

etc.

## 📦 Package Format (for developers)

Steal packages are simple `.tar.xz` archives containing a `.install` file and a directory to having the package files.

#### .install

The `.install` file is a simple bash script that will be executed to install the package.

EXAMPLE:
```bash
#!/bin/bash

#!/bin/sh

# Navigate to source directory
cd xx

# Compile package
echo "Compiling package..."
make

# Install package
echo "Installing package..."
make install PREFIX=/usr

ln -sf /usr/local/bin/xx /usr/bin/xx

echo "Package installed successfully"
```

## 📦 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 📦 Contributing

Contributions are welcome! Please open an issue or submit a pull request.
