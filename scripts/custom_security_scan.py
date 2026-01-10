#!/usr/bin/env python3
"""
Custom Security Scanner for Wyn Language
Part of Security Framework implementation
"""

import os
import re
import sys
from pathlib import Path

class SecurityScanner:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.issues = []
        
    def scan_file(self, file_path):
        """Scan a single file for security issues"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
            self.check_unsafe_functions(file_path, lines)
            self.check_buffer_overflows(file_path, lines)
            self.check_format_strings(file_path, lines)
            self.check_integer_overflows(file_path, lines)
            self.check_memory_leaks(file_path, lines)
            
        except Exception as e:
            print(f"Error scanning {file_path}: {e}")
    
    def check_unsafe_functions(self, file_path, lines):
        """Check for unsafe string functions"""
        unsafe_functions = [
            'strcpy', 'strcat', 'sprintf', 'gets', 'scanf'
        ]
        
        for i, line in enumerate(lines, 1):
            for func in unsafe_functions:
                if re.search(rf'\b{func}\s*\(', line):
                    self.issues.append({
                        'type': 'UNSAFE_FUNCTION',
                        'severity': 'HIGH',
                        'file': file_path,
                        'line': i,
                        'function': func,
                        'description': f'Use of unsafe function {func}() can lead to buffer overflows',
                        'recommendation': f'Replace {func}() with safer alternatives like strncpy(), strncat(), snprintf()'
                    })
    
    def check_buffer_overflows(self, file_path, lines):
        """Check for potential buffer overflow patterns"""
        patterns = [
            (r'char\s+\w+\[\d+\].*gets\s*\(', 'Fixed-size buffer with gets()'),
            (r'malloc\s*\(\s*strlen\s*\([^)]+\)\s*\)', 'malloc(strlen()) without +1 for null terminator'),
            (r'fread\s*\([^,]+,\s*1,\s*[^,]+,', 'fread() without size validation'),
        ]
        
        for i, line in enumerate(lines, 1):
            for pattern, desc in patterns:
                if re.search(pattern, line):
                    self.issues.append({
                        'type': 'BUFFER_OVERFLOW',
                        'severity': 'HIGH',
                        'file': file_path,
                        'line': i,
                        'description': desc,
                        'recommendation': 'Add bounds checking and use safe buffer operations'
                    })
    
    def check_format_strings(self, file_path, lines):
        """Check for format string vulnerabilities"""
        # Look for printf-family functions with user-controlled format strings
        patterns = [
            r'printf\s*\(\s*[^"]\w+',  # printf(variable)
            r'fprintf\s*\([^,]+,\s*[^"]\w+',  # fprintf(file, variable)
            r'sprintf\s*\([^,]+,\s*[^"]\w+',  # sprintf(buf, variable)
        ]
        
        for i, line in enumerate(lines, 1):
            for pattern in patterns:
                if re.search(pattern, line):
                    self.issues.append({
                        'type': 'FORMAT_STRING',
                        'severity': 'MEDIUM',
                        'file': file_path,
                        'line': i,
                        'description': 'Potential format string vulnerability',
                        'recommendation': 'Use format string literals: printf("%s", variable) instead of printf(variable)'
                    })
    
    def check_integer_overflows(self, file_path, lines):
        """Check for potential integer overflow in allocations"""
        patterns = [
            r'malloc\s*\([^)]*\*[^)]*\)',  # malloc(a * b)
            r'calloc\s*\([^,]*\*[^,]*,',   # calloc(a * b, size)
            r'realloc\s*\([^,]*,\s*[^)]*\*[^)]*\)',  # realloc(ptr, a * b)
        ]
        
        for i, line in enumerate(lines, 1):
            for pattern in patterns:
                if re.search(pattern, line):
                    self.issues.append({
                        'type': 'INTEGER_OVERFLOW',
                        'severity': 'MEDIUM',
                        'file': file_path,
                        'line': i,
                        'description': 'Potential integer overflow in memory allocation',
                        'recommendation': 'Check for overflow before multiplication: if (a > SIZE_MAX / b) return NULL;'
                    })
    
    def check_memory_leaks(self, file_path, lines):
        """Check for potential memory leaks"""
        malloc_pattern = r'\b(malloc|calloc|realloc)\s*\('
        free_pattern = r'\bfree\s*\('
        
        malloc_count = 0
        free_count = 0
        
        for line in lines:
            malloc_count += len(re.findall(malloc_pattern, line))
            free_count += len(re.findall(free_pattern, line))
        
        if malloc_count > free_count and malloc_count > 0:
            self.issues.append({
                'type': 'MEMORY_LEAK',
                'severity': 'HIGH',
                'file': file_path,
                'line': 0,
                'description': f'Potential memory leak: {malloc_count} allocations vs {free_count} frees',
                'recommendation': 'Ensure every malloc/calloc/realloc has a corresponding free()'
            })
    
    def scan_directory(self, directory):
        """Scan all C files in directory"""
        c_files = list(directory.glob('**/*.c')) + list(directory.glob('**/*.h'))
        
        for file_path in c_files:
            if '.git' not in str(file_path):
                self.scan_file(file_path)
    
    def generate_report(self):
        """Generate security report"""
        if not self.issues:
            return "✅ No security issues found!"
        
        report = f"# Wyn Language Security Scan Report\n\n"
        report += f"**Total Issues Found**: {len(self.issues)}\n\n"
        
        # Group by severity
        critical = [i for i in self.issues if i['severity'] == 'CRITICAL']
        high = [i for i in self.issues if i['severity'] == 'HIGH']
        medium = [i for i in self.issues if i['severity'] == 'MEDIUM']
        low = [i for i in self.issues if i['severity'] == 'LOW']
        
        if critical:
            report += f"## 🚨 Critical Issues ({len(critical)})\n\n"
            for issue in critical:
                report += self.format_issue(issue)
        
        if high:
            report += f"## ❌ High Severity Issues ({len(high)})\n\n"
            for issue in high:
                report += self.format_issue(issue)
        
        if medium:
            report += f"## ⚠️ Medium Severity Issues ({len(medium)})\n\n"
            for issue in medium:
                report += self.format_issue(issue)
        
        if low:
            report += f"## ℹ️ Low Severity Issues ({len(low)})\n\n"
            for issue in low:
                report += self.format_issue(issue)
        
        report += "\n## Recommendations\n\n"
        report += "1. **Immediate Action Required**: Fix all HIGH and CRITICAL severity issues\n"
        report += "2. **Memory Safety**: Implement systematic cleanup for all allocations\n"
        report += "3. **Input Validation**: Add bounds checking for all user inputs\n"
        report += "4. **Safe Functions**: Replace unsafe string functions with safe alternatives\n"
        report += "5. **Code Review**: Implement security code review process\n"
        
        return report
    
    def format_issue(self, issue):
        """Format a single issue for the report"""
        result = f"### {issue['type']}: {issue['description']}\n\n"
        result += f"- **File**: `{issue['file']}`\n"
        if issue['line'] > 0:
            result += f"- **Line**: {issue['line']}\n"
        result += f"- **Severity**: {issue['severity']}\n"
        if 'function' in issue:
            result += f"- **Function**: `{issue['function']}()`\n"
        result += f"- **Recommendation**: {issue['recommendation']}\n\n"
        return result

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 custom_security_scan.py <project_root>")
        sys.exit(1)
    
    project_root = sys.argv[1]
    scanner = SecurityScanner(project_root)
    
    # Scan src directory
    src_dir = Path(project_root) / 'src'
    if src_dir.exists():
        scanner.scan_directory(src_dir)
    
    # Generate and save report
    report = scanner.generate_report()
    
    report_file = Path(project_root) / 'security_detailed_report.md'
    with open(report_file, 'w') as f:
        f.write(report)
    
    print(f"Security scan complete. Report saved to: {report_file}")
    print(f"Total issues found: {len(scanner.issues)}")
    
    # Return exit code based on severity
    critical_high = [i for i in scanner.issues if i['severity'] in ['CRITICAL', 'HIGH']]
    return len(critical_high)

if __name__ == '__main__':
    sys.exit(main())
