#!/bin/bash
# 測試編譯腳本 - 驗證所有文件可以正確編譯

echo "╔════════════════════════════════════════╗"
echo "║   SMMU Compilation Test                ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Get script directory
SCRIPT_DIR=$(dirname "$0")
ROOT_DIR="$SCRIPT_DIR/.."

# Move to root
cd "$ROOT_DIR" || exit 1

# 測試基本編譯（不需要 SystemC）
echo "Testing basic compilation (no SystemC)..."
make clean > /dev/null 2>&1
if make > /dev/null 2>&1; then
    echo "✅ Basic compilation successful"
else
    echo "❌ Basic compilation failed"
    exit 1
fi

# 運行基本測試
echo ""
echo "Running basic tests..."
if ./bin/test_smmu > /dev/null 2>&1; then
    echo "✅ Basic tests passed"
else
    echo "❌ Basic tests failed"
    exit 1
fi

# 檢查 SystemC
echo ""
echo "Checking SystemC availability..."
if [ -z "$SYSTEMC_HOME" ]; then
    echo "⚠️  SYSTEMC_HOME not set"
    echo "   Set SYSTEMC_HOME to enable SystemC TLM compilation"
    echo "   Example: export SYSTEMC_HOME=/usr/local/systemc-2.3.3"
else
    echo "   SYSTEMC_HOME=$SYSTEMC_HOME"
    
    if [ -f "$SYSTEMC_HOME/include/systemc.h" ]; then
        echo "✅ SystemC found"
        
        # 測試 SystemC 編譯
        echo ""
        echo "Testing SystemC TLM compilation..."
        # Navigate to systemc folder to run its makefile, assuming it relies on relative paths now
        cd systemc || exit 1
        if make -f Makefile.systemc systemc > /dev/null 2>&1; then
            echo "✅ SystemC TLM compilation successful"
        else
            echo "❌ SystemC TLM compilation failed"
            echo "   Check SystemC installation and SYSTEMC_HOME path"
        fi
        cd ..
    else
        echo "❌ SystemC not found at $SYSTEMC_HOME"
        echo "   Check SYSTEMC_HOME path"
    fi
fi

echo ""
echo "╔════════════════════════════════════════╗"
echo "║   Compilation Test Complete            ║"
echo "╚════════════════════════════════════════╝"
