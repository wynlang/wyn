#!/bin/bash
# Cross-platform build script for Wyn

set -e

# Detect current platform
detect_platform() {
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

CURRENT_PLATFORM=$(detect_platform)

show_help() {
    echo "Usage: $0 [OPTIONS] [TARGET]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help     Show this help"
    echo "  -c, --clean    Clean build artifacts"
    echo "  -t, --test     Run tests after build"
    echo "  -a, --all      Build all platforms"
    echo "  -i, --info     Show platform information"
    echo ""
    echo "TARGETS:"
    echo "  current        Build for current platform ($CURRENT_PLATFORM)"
    echo "  windows        Build for Windows"
    echo "  linux          Build for Linux"
    echo "  macos          Build for macOS"
    echo ""
    echo "Examples:"
    echo "  $0 current     # Build for current platform"
    echo "  $0 --all       # Build for all platforms"
    echo "  $0 -t linux    # Build for Linux and run tests"
}

build_target() {
    local target=$1
    local run_tests=$2
    
    echo "Building Wyn compiler for $target..."
    
    case $target in
        "current")
            make wyn
            ;;
        "windows")
            make wyn-windows
            ;;
        "linux")
            make wyn-linux
            ;;
        "macos")
            make wyn-macos
            ;;
        *)
            echo "Unknown target: $target"
            exit 1
            ;;
    esac
    
    if [[ $run_tests == "true" && $target == "current" ]]; then
        echo "Running tests..."
        make test
    fi
    
    echo "✅ Build complete for $target"
}

# Parse arguments
CLEAN=false
TEST=false
ALL=false
INFO=false
TARGET=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -t|--test)
            TEST=true
            shift
            ;;
        -a|--all)
            ALL=true
            shift
            ;;
        -i|--info)
            INFO=true
            shift
            ;;
        current|windows|linux|macos)
            TARGET=$1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Clean if requested
if [[ $CLEAN == "true" ]]; then
    echo "Cleaning build artifacts..."
    make clean
    echo "✅ Clean complete"
fi

# Show platform info if requested
if [[ $INFO == "true" ]]; then
    make platform-info
    exit 0
fi

# Build all platforms if requested
if [[ $ALL == "true" ]]; then
    echo "Building for all platforms..."
    build_target "current" $TEST
    
    # Only attempt cross-compilation if tools are available
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        build_target "windows" false
    else
        echo "⚠️  Windows cross-compiler not available, skipping"
    fi
    
    # For Linux and macOS, only build if not on current platform
    if [[ $CURRENT_PLATFORM != "linux" ]]; then
        echo "⚠️  Linux cross-compilation not configured, skipping"
    fi
    
    if [[ $CURRENT_PLATFORM != "macos" ]]; then
        echo "⚠️  macOS cross-compilation not configured, skipping"
    fi
    
    exit 0
fi

# Build specific target or current platform
if [[ -z $TARGET ]]; then
    TARGET="current"
fi

build_target $TARGET $TEST