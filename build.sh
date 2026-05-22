#!/bin/bash
# Build script for duplicate file finder

echo "Building Duplicate File Finder..."

# Check if gcc is installed
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is not installed. Install with: sudo apt-get install build-essential"
    exit 1
fi

# Check if OpenSSL is available
if ! pkg-config --exists openssl; then
    echo "Error: OpenSSL development files not found. Install with:"
    echo "  Ubuntu/Debian: sudo apt-get install libssl-dev"
    echo "  Fedora: sudo dnf install openssl-devel"
    exit 1
fi

# Compile
gcc -o duplicate_finder duplicate_finder.c \
    -lpthread \
    $(pkg-config --cflags --libs openssl) \
    -Wall -Wextra -O2

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Usage:"
    echo "  ./duplicate_finder /path/to/scan"
    echo "  ./duplicate_finder /home /var /opt"
    echo ""
else
    echo "✗ Build failed!"
    exit 1
fi
