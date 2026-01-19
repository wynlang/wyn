#!/bin/bash
# Update Wyn version across all necessary files
# Usage: ./update-version.sh 1.3.0

set -e

if [ $# -eq 0 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.3.0"
    exit 1
fi

NEW_VERSION="$1"
SCRIPT_DIR="$(dirname "$0")"

# Validate version format
if ! echo "$NEW_VERSION" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Error: Version must be in format X.Y.Z (e.g., 1.3.0)"
    exit 1
fi

echo "Updating Wyn version to $NEW_VERSION..."

# Update VERSION file
echo "$NEW_VERSION" > "$SCRIPT_DIR/VERSION"
echo "✓ Updated VERSION file"

# Update README.md
sed -i.bak "s/\*\*Version:\*\* [0-9]\+\.[0-9]\+\.[0-9]\+/**Version:** $NEW_VERSION/" "$SCRIPT_DIR/README.md"
sed -i.bak "s/Wyn v[0-9]\+\.[0-9]\+\.[0-9]\+/Wyn v$NEW_VERSION/g" "$SCRIPT_DIR/README.md"
echo "✓ Updated README.md"

# Update REFERENCE.md
sed -i.bak "s/Wyn v[0-9]\+\.[0-9]\+\.[0-9]\+/Wyn v$NEW_VERSION/g" "$SCRIPT_DIR/REFERENCE.md"
echo "✓ Updated REFERENCE.md"

# Add new entry to CHANGELOG.md (at the top, after the header)
CHANGELOG_ENTRY="## [$NEW_VERSION] - $(date +%Y-%m-%d)

### Added
- [Add new features here]

### Fixed
- [Add bug fixes here]

### Changed
- [Add changes here]

"

# Create temporary file with new changelog entry
{
    head -n 2 "$SCRIPT_DIR/CHANGELOG.md"
    echo "$CHANGELOG_ENTRY"
    tail -n +3 "$SCRIPT_DIR/CHANGELOG.md"
} > "$SCRIPT_DIR/CHANGELOG.md.tmp"

mv "$SCRIPT_DIR/CHANGELOG.md.tmp" "$SCRIPT_DIR/CHANGELOG.md"
echo "✓ Added new entry to CHANGELOG.md"

# Update any hardcoded versions in documentation
find "$SCRIPT_DIR/docs" -name "*.md" -exec sed -i.bak "s/v[0-9]\+\.[0-9]\+\.[0-9]\+/v$NEW_VERSION/g" {} \; 2>/dev/null || true
echo "✓ Updated documentation files"

# Update site version if site directory exists
if [ -d "$SCRIPT_DIR/../site" ]; then
    cd "$SCRIPT_DIR/../site"
    if [ -f "./update-version.sh" ]; then
        ./update-version.sh "$NEW_VERSION"
        echo "✓ Updated website version"
    fi
    cd "$SCRIPT_DIR"
fi

# Clean up backup files
find "$SCRIPT_DIR" -name "*.bak" -delete 2>/dev/null || true

echo ""
echo "🎉 Version updated to $NEW_VERSION"
echo ""
echo "Next steps:"
echo "1. Edit CHANGELOG.md to add specific changes for this version"
echo "2. Test the build: make clean && make wyn"
echo "3. Run tests: make test"
echo "4. Create release: ./release.sh $NEW_VERSION"
echo ""