#!/bin/bash
# Security Review Script for Wyn Language
# Part of Security Framework implementation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔒 Wyn Language Security Review${NC}"
echo "========================================"

# Function to run static analysis
run_static_analysis() {
    echo -e "\n${BLUE}🔍 Running Static Analysis...${NC}"
    
    # Check if clang is available
    if command -v clang >/dev/null 2>&1; then
        echo "Running Clang Static Analyzer..."
        find "$PROJECT_ROOT/src" -name "*.c" -exec clang --analyze -Xanalyzer -analyzer-output=text {} \; 2>&1 | tee security_static_analysis.log
    else
        echo -e "${YELLOW}⚠️ Clang not available, skipping static analysis${NC}"
    fi
    
    # Check if cppcheck is available
    if command -v cppcheck >/dev/null 2>&1; then
        echo "Running Cppcheck..."
        cppcheck --enable=all --error-exitcode=0 "$PROJECT_ROOT/src" 2>&1 | tee security_cppcheck.log
    else
        echo -e "${YELLOW}⚠️ Cppcheck not available, installing would improve security scanning${NC}"
    fi
}

# Function to scan for vulnerability patterns
run_vulnerability_scan() {
    echo -e "\n${BLUE}🛡️ Running Vulnerability Pattern Scan...${NC}"
    
    local issues_found=0
    
    # Check for unsafe string functions
    echo "Checking for unsafe string functions..."
    if grep -rn "strcpy\|strcat\|sprintf\|gets" "$PROJECT_ROOT/src" --include="*.c" --include="*.h"; then
        echo -e "${RED}❌ Found potentially unsafe string functions${NC}"
        issues_found=$((issues_found + 1))
    else
        echo -e "${GREEN}✅ No unsafe string functions found${NC}"
    fi
    
    # Check for format string vulnerabilities
    echo "Checking for format string vulnerabilities..."
    if grep -rn "printf.*%.*)" "$PROJECT_ROOT/src" --include="*.c" | grep -v 'printf("'; then
        echo -e "${RED}❌ Found potential format string vulnerabilities${NC}"
        issues_found=$((issues_found + 1))
    else
        echo -e "${GREEN}✅ No format string vulnerabilities found${NC}"
    fi
    
    # Check for potential integer overflows in allocations
    echo "Checking for potential integer overflow in allocations..."
    if grep -rn "malloc.*\*\|calloc.*\*" "$PROJECT_ROOT/src" --include="*.c"; then
        echo -e "${YELLOW}⚠️ Found multiplications in allocations - review for integer overflow${NC}"
        issues_found=$((issues_found + 1))
    else
        echo -e "${GREEN}✅ No obvious integer overflow patterns in allocations${NC}"
    fi
    
    # Check for hardcoded credentials
    echo "Checking for hardcoded credentials..."
    if grep -rni "password\|secret\|key.*=" "$PROJECT_ROOT/src" --include="*.c" --include="*.h"; then
        echo -e "${RED}❌ Found potential hardcoded credentials${NC}"
        issues_found=$((issues_found + 1))
    else
        echo -e "${GREEN}✅ No hardcoded credentials found${NC}"
    fi
    
    # Check for memory allocation without bounds checking
    echo "Checking for memory allocations..."
    local malloc_count=$(grep -rc "malloc\|calloc\|realloc" "$PROJECT_ROOT/src" --include="*.c" | awk -F: '{sum += $2} END {print sum}')
    local free_count=$(grep -rc "free(" "$PROJECT_ROOT/src" --include="*.c" | awk -F: '{sum += $2} END {print sum}')
    
    echo "Found $malloc_count allocations and $free_count free calls"
    if [ "$malloc_count" -gt "$free_count" ]; then
        echo -e "${RED}❌ More allocations than frees - potential memory leaks${NC}"
        issues_found=$((issues_found + 1))
    else
        echo -e "${GREEN}✅ Allocation/free count looks balanced${NC}"
    fi
    
    return $issues_found
}

# Function to check file permissions
check_file_permissions() {
    echo -e "\n${BLUE}🔐 Checking File Permissions...${NC}"
    
    # Check for world-writable files
    if find "$PROJECT_ROOT" -type f -perm -002 2>/dev/null | grep -v ".git"; then
        echo -e "${RED}❌ Found world-writable files${NC}"
        return 1
    else
        echo -e "${GREEN}✅ No world-writable files found${NC}"
    fi
    
    # Check for executable source files
    if find "$PROJECT_ROOT/src" -name "*.c" -o -name "*.h" | xargs ls -l | grep "^-rwx"; then
        echo -e "${YELLOW}⚠️ Found executable source files${NC}"
        return 1
    else
        echo -e "${GREEN}✅ Source files have appropriate permissions${NC}"
    fi
    
    return 0
}

# Function to generate security report
generate_security_report() {
    local total_issues=$1
    
    echo -e "\n${BLUE}📊 Security Scan Summary${NC}"
    echo "========================================"
    
    if [ "$total_issues" -eq 0 ]; then
        echo -e "${GREEN}✅ No critical security issues found${NC}"
        echo -e "${GREEN}✅ Security scan PASSED${NC}"
    elif [ "$total_issues" -le 3 ]; then
        echo -e "${YELLOW}⚠️ $total_issues minor security issues found${NC}"
        echo -e "${YELLOW}⚠️ Security scan PASSED with warnings${NC}"
    else
        echo -e "${RED}❌ $total_issues security issues found${NC}"
        echo -e "${RED}❌ Security scan FAILED${NC}"
    fi
    
    # Generate detailed report
    cat > security_report.md << EOF
# Wyn Language Security Scan Report
**Date**: $(date)
**Scan Type**: Automated Security Review
**Total Issues Found**: $total_issues

## Scan Results

### Static Analysis
- Clang Static Analyzer: $([ -f security_static_analysis.log ] && echo "Completed" || echo "Skipped")
- Cppcheck: $([ -f security_cppcheck.log ] && echo "Completed" || echo "Skipped")

### Vulnerability Patterns
- Unsafe string functions: $(grep -c "strcpy\|strcat\|sprintf\|gets" "$PROJECT_ROOT/src"/*.c 2>/dev/null || echo "0") instances
- Format string issues: Checked
- Integer overflow patterns: Checked
- Hardcoded credentials: Checked
- Memory management: Reviewed

### File Permissions
- World-writable files: Checked
- Executable source files: Checked

## Recommendations

1. **Memory Management**: Implement systematic cleanup for all allocations
2. **Input Validation**: Add bounds checking for all user inputs
3. **Error Handling**: Ensure no sensitive information leaks in error messages
4. **Dependencies**: Regular security updates for build tools

## Next Steps

1. Address any critical issues found
2. Implement automated security testing in CI/CD
3. Regular security reviews for all code changes
4. Security training for development team

EOF

    echo -e "\n${BLUE}📄 Detailed report saved to: security_report.md${NC}"
}

# Main execution
main() {
    cd "$PROJECT_ROOT"
    
    local total_issues=0
    
    # Run all security checks
    run_static_analysis
    
    run_vulnerability_scan
    local vuln_issues=$?
    total_issues=$((total_issues + vuln_issues))
    
    check_file_permissions
    local perm_issues=$?
    total_issues=$((total_issues + perm_issues))
    
    # Generate final report
    generate_security_report $total_issues
    
    # Update security agent status
    echo "Security scan completed with $total_issues issues" > "$PROJECT_ROOT/.agents/security/status.txt"
    echo "25%" > "$PROJECT_ROOT/.agents/security/progress.txt"
    
    exit $total_issues
}

# Run main function
main "$@"
