#!/bin/bash
# Test IDE Integration (Task 12)

set -e

echo "=== IDE Integration Tests ==="
echo

# Test 1: VS Code extension files exist
echo "Test 1: VS Code extension structure"
if [ -d "../vscode-extension" ] && \
   [ -f "../vscode-extension/package.json" ] && \
   [ -f "../vscode-extension/language-configuration.json" ] && \
   [ -f "../vscode-extension/syntaxes/wyn.tmLanguage.json" ] && \
   [ -f "../vscode-extension/src/extension.ts" ] && \
   [ -f "../vscode-extension/tsconfig.json" ] && \
   [ -f "../vscode-extension/README.md" ]; then
    echo "✅ VS Code extension complete (6 files)"
else
    echo "❌ VS Code extension incomplete"
    exit 1
fi

# Test 2: Neovim plugin files exist
echo "Test 2: Neovim plugin structure"
if [ -d "../nvim-plugin" ] && \
   [ -f "../nvim-plugin/lua/wyn/init.lua" ] && \
   [ -f "../nvim-plugin/README.md" ]; then
    echo "✅ Neovim plugin complete (2 files)"
else
    echo "❌ Neovim plugin incomplete"
    exit 1
fi

# Test 3: VS Code package.json structure
echo "Test 3: VS Code extension manifest"
if grep -q '"name": "wyn"' ../vscode-extension/package.json && \
   grep -q '"displayName": "Wyn"' ../vscode-extension/package.json && \
   grep -q '"version"' ../vscode-extension/package.json && \
   grep -q '"activationEvents"' ../vscode-extension/package.json; then
    echo "✅ Extension structure valid"
else
    echo "❌ Extension structure invalid"
    exit 1
fi

# Test 4: Syntax grammar exists
echo "Test 4: Syntax highlighting grammar"
if grep -q '"scopeName": "source.wyn"' ../vscode-extension/syntaxes/wyn.tmLanguage.json && \
   grep -q '"patterns"' ../vscode-extension/syntaxes/wyn.tmLanguage.json; then
    echo "✅ Syntax grammar complete"
else
    echo "❌ Syntax grammar incomplete"
    exit 1
fi

# Test 5: LSP client integration
echo "Test 5: LSP client integration"
if grep -q "vscode-languageclient" ../vscode-extension/src/extension.ts && \
   grep -q "LanguageClient" ../vscode-extension/src/extension.ts; then
    echo "✅ LSP integration complete"
else
    echo "❌ LSP integration incomplete"
    exit 1
fi

# Test 6: Neovim LSP configuration
echo "Test 6: Neovim LSP configuration"
if grep -q "lspconfig" ../nvim-plugin/lua/wyn/init.lua && \
   grep -q "wyn" ../nvim-plugin/lua/wyn/init.lua; then
    echo "✅ Neovim configuration complete"
else
    echo "❌ Neovim configuration incomplete"
    exit 1
fi

# Test 7: Documentation exists
echo "Test 7: Documentation"
if grep -q "Installation" ../vscode-extension/README.md && \
   grep -q "Installation" ../nvim-plugin/README.md; then
    echo "✅ Documentation complete"
else
    echo "❌ Documentation incomplete"
    exit 1
fi

echo
echo "=== All IDE Integration Tests Passed ==="
