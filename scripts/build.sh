#!/bin/bash
# build.sh - Build script for GCC mem_shared extension
# Copyright (C) 2024 Free Software Foundation, Inc.

set -e

# Configuration
GCC_VERSION=${GCC_VERSION:-"13.2.0"}
BUILD_DIR=${BUILD_DIR:-"build"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"/usr/local"}
NUM_CORES=${NUM_CORES:-$(nproc)}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check dependencies
check_dependencies() {
    log_info "Checking build dependencies..."
    
    local missing_deps=()
    
    # Essential build tools
    for cmd in gcc g++ make flex bison texinfo; do
        if ! command -v $cmd >/dev/null 2>&1; then
            missing_deps+=($cmd)
        fi
    done
    
    # Development libraries
    if ! pkg-config --exists gmp; then
        missing_deps+=("libgmp-dev")
    fi
    
    if ! pkg-config --exists mpfr; then
        missing_deps+=("libmpfr-dev")
    fi
    
    if ! pkg-config --exists libmpc; then
        missing_deps+=("libmpc-dev")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Please install them using your package manager:"
        log_info "  Ubuntu/Debian: sudo apt-get install ${missing_deps[*]}"
        log_info "  CentOS/RHEL: sudo yum install ${missing_deps[*]}"
        exit 1
    fi
    
    log_success "All dependencies satisfied"
}

# Download GCC source if needed
download_gcc_source() {
    log_info "Checking GCC source..."
    
    if [ ! -d "gcc-${GCC_VERSION}" ]; then
        log_info "Downloading GCC ${GCC_VERSION} source..."
        
        if [ ! -f "gcc-${GCC_VERSION}.tar.gz" ]; then
            wget "https://gcc.gnu.org/releases/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz"
        fi
        
        log_info "Extracting GCC source..."
        tar -xzf "gcc-${GCC_VERSION}.tar.gz"
    fi
    
    log_success "GCC source ready"
}

# Apply patches
apply_patches() {
    log_info "Applying mem_shared patches..."
    
    local gcc_src_dir="gcc-${GCC_VERSION}"
    
    # Check if patches are already applied
    if [ -f "${gcc_src_dir}/.mem_shared_patches_applied" ]; then
        log_warning "Patches already applied, skipping..."
        return
    fi
    
    # Apply each patch
    local patches=(
        "patches/c-common.h.patch"
        "patches/c-common.c.patch" 
        "patches/c-parser.c.patch"
        "patches/c-tree.h.patch"
        "patches/tree.h.patch"
        "patches/common.opt.patch"
        "patches/c-decl-patch.diff"
        "patches/expr-patch.diff"
        "patches/target.md.patch"
    )
    
    for patch in "${patches[@]}"; do
        if [ -f "$patch" ]; then
            log_info "Applying patch: $patch"
            patch -p1 -d "$gcc_src_dir" < "$patch"
        else
            log_warning "Patch not found: $patch"
        fi
    done
    
    # Copy our source files
    log_info "Copying mem_shared source files..."
    cp src/mem-shared.h "${gcc_src_dir}/gcc/"
    cp src/mem-shared.c "${gcc_src_dir}/gcc/"
    
    # Mark patches as applied
    touch "${gcc_src_dir}/.mem_shared_patches_applied"
    
    log_success "Patches applied successfully"
}

# Configure GCC build
configure_gcc() {
    log_info "Configuring GCC build..."
    
    local gcc_src_dir="gcc-${GCC_VERSION}"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    if [ ! -f "Makefile" ]; then
        log_info "Running configure..."
        "../${gcc_src_dir}/configure" \
            --prefix="$INSTALL_PREFIX" \
            --enable-languages=c,c++ \
            --enable-multicore-support \
            --enable-mem-shared \
            --disable-multilib \
            --enable-threads=posix \
            --with-system-zlib \
            --enable-__cxa_atexit \
            --disable-libunwind-exceptions \
            --enable-clocale=gnu \
            --disable-libstdcxx-pch \
            --with-tune=generic \
            --enable-checking=release \
            --build=x86_64-linux-gnu \
            --host=x86_64-linux-gnu \
            --target=x86_64-linux-gnu
    fi
    
    cd ..
    log_success "GCC configured"
}

# Build GCC
build_gcc() {
    log_info "Building GCC (this may take a while)..."
    
    cd "$BUILD_DIR"
    
    # Use parallel build
    log_info "Starting parallel build with $NUM_CORES cores..."
    make -j"$NUM_CORES"
    
    cd ..
    log_success "GCC build completed"
}

# Run tests
run_tests() {
    log_info "Running mem_shared specific tests..."
    
    # Build our test examples first
    cd "$BUILD_DIR"
    
    # Test basic functionality
    log_info "Testing basic mem_shared compilation..."
    
    # Create a simple test file
    cat > test_mem_shared.c << 'EOF'
mem_shared int test_var = 42;
mem_shared int test_array[100];

int main() {
    test_var = 100;
    test_array[0] = test_var;
    return 0;
}
EOF
    
    # Try to compile with our modified GCC
    if ./gcc/xgcc -B./gcc/ -fmem-shared -fmem-shared-cores=4 -S test_mem_shared.c; then
        log_success "Basic mem_shared compilation test passed"
    else
        log_error "Basic mem_shared compilation test failed"
        exit 1
    fi
    
    cd ..
}

# Main build process
main() {
    log_info "Starting GCC mem_shared build process..."
    log_info "GCC Version: $GCC_VERSION"
    log_info "Build Directory: $BUILD_DIR"
    log_info "Install Prefix: $INSTALL_PREFIX"
    log_info "CPU Cores: $NUM_CORES"
    
    check_dependencies
    download_gcc_source
    apply_patches
    configure_gcc
    build_gcc
    run_tests
    
    log_success "Build completed successfully!"
    log_info "To install, run: sudo ./scripts/install.sh"
    log_info "To run tests, run: ./scripts/test.sh"
}

# Handle command line arguments
case "${1:-}" in
    "clean")
        log_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
        log_success "Build directory cleaned"
        ;;
    "distclean")
        log_info "Performing distclean..."
        rm -rf "$BUILD_DIR" "gcc-${GCC_VERSION}" "gcc-${GCC_VERSION}.tar.gz"
        log_success "Distclean completed"
        ;;
    "help"|"-h"|"--help")
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  (no args)  - Build GCC with mem_shared support"
        echo "  clean      - Clean build directory"
        echo "  distclean  - Clean everything including downloaded source"
        echo "  help       - Show this help message"
        echo ""
        echo "Environment variables:"
        echo "  GCC_VERSION     - GCC version to build (default: 13.2.0)"
        echo "  BUILD_DIR       - Build directory (default: build)" 
        echo "  INSTALL_PREFIX  - Installation prefix (default: /usr/local)"
        echo "  NUM_CORES       - Number of CPU cores for parallel build (default: auto-detect)"
        ;;
    "")
        main
        ;;
    *)
        log_error "Unknown command: $1"
        log_info "Run '$0 help' for usage information"
        exit 1
        ;;
esac