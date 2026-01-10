#!/bin/bash
# Create a new release

if [ -z "$1" ]; then
    echo "Usage: ./release.sh <version>"
    echo "Example: ./release.sh 1.0.1"
    exit 1
fi

VERSION=$1

echo "Creating release v$VERSION"

# Update VERSION file
echo "$VERSION" > VERSION

# Run tests
echo "Running tests..."
make test || exit 1
./tests/integration_tests.sh || exit 1

# Commit version bump
git add VERSION
git commit -m "Bump version to $VERSION"

# Create tag
git tag -a "v$VERSION" -m "Release v$VERSION"

echo "Release v$VERSION created"
echo "Push with: git push && git push --tags"
echo "GitHub Actions will build binaries for linux, macos, windows"
