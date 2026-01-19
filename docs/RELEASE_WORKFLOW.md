# How to Release a New Version

This document outlines the complete process for releasing a new version of the Wyn programming language.

## Prerequisites

- All tests must pass
- All new features must be documented
- CHANGELOG.md should be ready for the new version
- Website content should be updated if user-facing changes exist

## Release Process

### 1. Pre-Release Validation

```bash
# Ensure clean working directory
git status

# Run full test suite
make clean
make wyn
make test
./tests/regression.sh

# Test all examples
for f in examples/*.wyn; do
    echo "Testing $f..."
    ./wyn "$f" || exit 1
done
```

### 2. Version Update

```bash
# Update all version references automatically
./update-version.sh 1.3.0

# This script updates:
# - VERSION file
# - README.md version references  
# - REFERENCE.md version references
# - CHANGELOG.md (adds new entry template)
# - Documentation files in docs/
# - Website version (if site/ directory exists)
```

### 3. Update CHANGELOG

Edit the newly created CHANGELOG.md entry:

```bash
vim CHANGELOG.md
```

Add specific details about:
- **Added**: New features and capabilities
- **Fixed**: Bug fixes and corrections  
- **Changed**: Breaking changes or modifications
- **Deprecated**: Features marked for removal
- **Removed**: Features removed in this version
- **Security**: Security-related fixes

### 4. Final Testing

```bash
# Test with updated version
make clean
make wyn

# Verify version is correct
./wyn --version

# Run comprehensive tests
make test
./tests/regression.sh
```

### 5. Create Release

```bash
# Use the automated release script
./release.sh 1.3.0

# This will:
# - Verify VERSION file is updated
# - Build and test the compiler
# - Run module tests
# - Test example compilation
# - Commit version changes
# - Create git tag
```

### 6. Update Website (if needed)

If there are user-facing changes:

```bash
cd ../site/

# Update website content
vim content/learn.md        # Update tutorial if syntax changed
vim content/examples.md     # Add new examples
vim content/stdlib.md       # Update API documentation

# Update version and rebuild
./update-version.sh 1.3.0
hugo --minify

# Deploy to hosting platform
# (Upload public/ directory to Cloudflare Pages, GitHub Pages, etc.)
```

### 7. Push and Deploy

```bash
# Push changes and tags
git push origin main
git push --tags

# GitHub Actions will automatically:
# - Build cross-platform binaries
# - Create GitHub release
# - Attach binaries to release
```

### 8. Create GitHub Release

1. Go to GitHub repository releases page
2. Find the new tag (created by release.sh)
3. Click "Create release from tag"
4. Use CHANGELOG.md content for release notes
5. Attach any additional files if needed
6. Publish release

## Version Numbering

Wyn follows semantic versioning (SemVer):

- **Major (X.0.0)**: Breaking changes, major new features
- **Minor (1.X.0)**: New features, backward compatible
- **Patch (1.2.X)**: Bug fixes, backward compatible

### Examples:
- `1.3.0` - New language features, new stdlib methods
- `1.2.3` - Bug fixes, performance improvements
- `2.0.0` - Breaking syntax changes, major redesign

## Release Checklist

### Pre-Release
- [ ] All tests pass (`make test`)
- [ ] All examples compile and run
- [ ] Regression tests pass (`./tests/regression.sh`)
- [ ] Documentation is up to date
- [ ] CHANGELOG.md is ready

### Version Update
- [ ] Run `./update-version.sh X.Y.Z`
- [ ] Edit CHANGELOG.md with specific changes
- [ ] Verify version in VERSION file
- [ ] Check README.md version references

### Testing
- [ ] Clean build works (`make clean && make wyn`)
- [ ] Version command works (`./wyn --version`)
- [ ] All tests still pass
- [ ] Examples still work

### Release
- [ ] Run `./release.sh X.Y.Z`
- [ ] Verify git tag created
- [ ] Push changes (`git push && git push --tags`)
- [ ] GitHub Actions builds successfully

### Post-Release
- [ ] Update website if needed
- [ ] Create GitHub release with notes
- [ ] Announce on community channels
- [ ] Update any external documentation

## Troubleshooting

### Build Failures
If the build fails during release:
1. Check compiler errors in build output
2. Ensure all source files are committed
3. Verify no syntax errors in new code
4. Run `make clean` and try again

### Test Failures
If tests fail during release:
1. Run individual test files to isolate issues
2. Check for breaking changes in new code
3. Update tests if behavior intentionally changed
4. Fix bugs before proceeding with release

### Version Conflicts
If version updates cause conflicts:
1. Manually verify VERSION file content
2. Check for merge conflicts in documentation
3. Ensure CHANGELOG.md format is correct
4. Re-run `./update-version.sh` if needed

## Automation

The release process is largely automated:

- **update-version.sh**: Updates all version references
- **release.sh**: Builds, tests, and creates release
- **GitHub Actions**: Builds cross-platform binaries
- **Website script**: Updates website version

This ensures consistency and reduces manual errors in the release process.

## Emergency Releases

For critical bug fixes:

1. Create hotfix branch from release tag
2. Apply minimal fix
3. Follow normal release process with patch version
4. Merge back to main branch

Example:
```bash
git checkout v1.2.1
git checkout -b hotfix-1.2.2
# Apply fix
./update-version.sh 1.2.2
# Edit CHANGELOG.md
./release.sh 1.2.2
git checkout main
git merge hotfix-1.2.2
```

This ensures stable releases while maintaining development momentum.