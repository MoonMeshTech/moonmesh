#!/bin/bash
# GMP Source Installation Script
# Version: 1.2
# Description: Automates GMP installation from source including download, compilation, installation and cleanup
# Supported systems: Linux (Debian/Ubuntu, RHEL/CentOS), macOS
# Usage: ./install_gmp.sh [version] [install-path]

if [[ -f /etc/redhat-release ]]; then
    CENTOS_VERSION=$(grep -oE '[0-9]+\.[0-9]+' /etc/redhat-release)
    if [[ "$CENTOS_VERSION" == "7."* ]]; then
        echo "Warning: CentOS 7 reached the end of its lifecycle on June 30, 2024 and is no longer officially supported."
        echo "This script was skipped due to compatibility issues."
        exit 0
    fi
fi

# ===== Configuration Section =====
DEFAULT_VERSION="6.3.0"                  # Default GMP version [1](@ref)
DEFAULT_PREFIX="/usr/local"              # Default installation prefix [2,4](@ref)
BUILD_OPTIONS="--enable-cxx"            # Enable C++ support [1,10](@ref)
PARALLEL_JOBS=$(nproc)                   # Parallel compilation jobs based on CPU cores

# ===== Runtime Parameters =====
VERSION="${1:-$DEFAULT_VERSION}"        # Use first argument or default version
PREFIX="${2:-$DEFAULT_PREFIX}"          # Use second argument or default prefix
SOURCE_DIR="/tmp/gmp-$VERSION-src"       # Temporary source directory
TAR_FILE="gmp-$VERSION.tar.xz"           # Source archive name
DOWNLOAD_URL="https://gmplib.org/download/gmp/$TAR_FILE" # Official download URL [1,4](@ref)

# ===== Function: Install Dependencies =====
install_dependencies() {
    echo "[1/7] Installing system dependencies..."
    if [[ -x "$(command -v apt-get)" ]]; then
        # Debian/Ubuntu systems
        sudo apt-get update
        sudo apt-get install -y build-essential m4 libtool wget
    elif [[ -x "$(command -v yum)" ]]; then
        # RHEL/CentOS systems
        sudo yum groupinstall -y "Development Tools"
        sudo yum install -y m4 libtool wget
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS systems
        if ! command -v brew &> /dev/null; then
            echo "Error: Homebrew required. Install from https://brew.sh"
            exit 1
        fi
        brew install m4 libtool wget
    else
        echo "Error: Unsupported package manager"
        exit 1
    fi
}

# ===== Function: Download and Extract Source =====
download_source() {
    echo "[2/7] Downloading GMP version $VERSION..."
    wget -q --show-progress "$DOWNLOAD_URL" -O "$TAR_FILE" || {
        echo "Error: Download failed. Verify version exists at $DOWNLOAD_URL"
        exit 1
    }
    
    echo "[3/7] Extracting source code..."
    mkdir -p "$SOURCE_DIR"
    tar -xf "$TAR_FILE" -C "$SOURCE_DIR" --strip-components=1
    rm -f "$TAR_FILE"  # Remove downloaded archive
    cd "$SOURCE_DIR" || exit 1
}

# ===== Function: Compile and Install =====
compile_and_install() {
    echo "[4/7] Configuring build..."
    CFLAGS="-fPIE" CXXFLAGS="-fPIE" LDFLAGS="-pie" ./configure  --enable-pic --prefix="$PREFIX" $BUILD_OPTIONS | tee configure.log || {
        echo "Error: Configuration failed. Check configure.log"
        exit 1
    }
    
    echo "[5/7] Compiling source (using $PARALLEL_JOBS jobs)..."
    make -j "$PARALLEL_JOBS" | tee make.log || {
        echo "Error: Compilation failed. Check make.log"
        exit 1
    }
    
    echo "[6/7] Running test suite..."
    make check | tee check.log || {
        echo "Warning: Tests failed. Check check.log before proceeding"
        read -r -p "Continue installation? (y/N) " response
        if [[ ! "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            exit 1
        fi
    }
    
    echo "[7/7] Installing to $PREFIX..."
    sudo make install | tee install.log || {
        echo "Error: Installation failed. Check install.log"
        exit 1
    }
}

# ===== Function: Post-Install Configuration =====
post_install() {
    echo "[Post-Install] Updating linker cache..."
    if [[ "$PREFIX" != "/usr" && "$PREFIX" != "/usr/local" ]]; then
        # Add custom library path to ldconfig [4](@ref)
        echo "$PREFIX/lib" | sudo tee /etc/ld.so.conf.d/gmp.conf >/dev/null
    fi
    sudo ldconfig
    
    # Create verification test
    echo "Verifying installation..."
    cat << EOF > verify_gmp.c
#include <gmp.h>
#include <stdio.h>
int main() {
    mpz_t a, b, c;
    mpz_init(a); mpz_init(b); mpz_init(c);
    mpz_set_str(a, "1234567890", 10);
    mpz_set_str(b, "9876543210", 10);
    mpz_add(c, a, b);
    gmp_printf("Result: %Zd\\n", c);
    mpz_clear(a); mpz_clear(b); mpz_clear(c);
    return 0;
}
EOF
    
    # Compile and run test
    gcc verify_gmp.c -lgmp -o verify_gmp
    ./verify_gmp && echo "Verification PASSED" || echo "Verification FAILED"
    rm verify_gmp verify_gmp.c
}

# ===== Function: Cleanup =====
cleanup() {
    echo "[Cleanup] Removing source files..."
    cd /tmp || exit 0
    sudo rm -rf "$SOURCE_DIR"
    echo "GMP $VERSION installation completed"
}

# ===== Main Execution =====
install_dependencies
download_source
compile_and_install
post_install
cleanup
