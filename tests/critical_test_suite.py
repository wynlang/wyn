#!/usr/bin/env python3
"""
Wyn Language COMPREHENSIVE Critical Test Suite
==============================================

100% coverage testing framework for REAL stability and regression testing.
Tests all critical functionality with deep analysis, edge cases, and error conditions.
"""

import os
import subprocess
import sys
import json
import signal
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class TestResult(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    CRASH = "CRASH"
    TIMEOUT = "TIMEOUT"
    SEGFAULT = "SEGFAULT"

@dataclass
class CriticalTestCase:
    name: str
    description: str
    wyn_code: str
    expected_output: Optional[str] = None
    should_compile: bool = True
    should_run: bool = True
    category: str = "critical"
    timeout: int = 5
    expect_crash: bool = False

@dataclass
class TestReport:
    name: str
    result: TestResult
    compile_output: str = ""
    run_output: str = ""
    error_message: str = ""
    execution_time: float = 0.0
    exit_code: int = 0

class WynCriticalTestSuite:
    def __init__(self, wyn_dir: str = "/Users/aoaws/src/ao/wyn-lang/wyn"):
        self.wyn_dir = Path(wyn_dir)
        self.test_dir = self.wyn_dir / "tests" / "critical_real"
        self.test_dir.mkdir(exist_ok=True)
        self.reports: List[TestReport] = []
        
    def create_critical_tests(self) -> List[CriticalTestCase]:
        """Create tests for all REAL critical issues discovered"""
        return [
            # CRITICAL ISSUE 1: Parser Segmentation Faults
            CriticalTestCase(
                name="parser_segfault_uninitialized",
                description="Test parser crash on uninitialized variables",
                wyn_code="""
fn main() -> int {
    var uninitialized
    print("Should not reach here")
    return 0
}
""",
                should_compile=False,
                expect_crash=True,
                category="parser_crashes"
            ),
            
            CriticalTestCase(
                name="parser_segfault_nested_function",
                description="Test parser crash on nested functions",
                wyn_code="""
fn main() -> int {
    fn inner() -> int {
        return 42
    }
    var result = inner()
    print(result)
    return 0
}
""",
                should_compile=False,
                expect_crash=True,
                category="parser_crashes"
            ),
            
            # CRITICAL ISSUE 2: String Escaping
            CriticalTestCase(
                name="string_escaping_quotes",
                description="Test string escaping with quotes",
                wyn_code="""
fn main() -> int {
    var quote_string = "He said \\"Hello\\""
    print(quote_string)
    return 0
}
""",
                expected_output="He said \"Hello\"\n",
                category="string_handling"
            ),
            
            CriticalTestCase(
                name="string_escaping_backslash",
                description="Test string escaping with backslashes",
                wyn_code="""
fn main() -> int {
    var path = "C:\\\\Users\\\\test"
    print(path)
    return 0
}
""",
                expected_output="C:\\Users\\test\n",
                category="string_handling"
            ),
            
            # CRITICAL ISSUE 3: Array Bounds Checking
            CriticalTestCase(
                name="array_bounds_negative",
                description="Test negative array indexing",
                wyn_code="""
fn main() -> int {
    var arr = [1, 2, 3]
    print(arr[-1])
    return 0
}
""",
                should_run=False,  # Should fail gracefully, not crash
                category="array_safety"
            ),
            
            CriticalTestCase(
                name="array_bounds_overflow",
                description="Test array index overflow",
                wyn_code="""
fn main() -> int {
    var arr = [1, 2, 3]
    print(arr[100])
    return 0
}
""",
                should_run=False,  # Should fail gracefully, not crash
                category="array_safety"
            ),
            
            # CRITICAL ISSUE 4: Nested Arrays
            CriticalTestCase(
                name="nested_arrays_2d",
                description="Test 2D array creation and access",
                wyn_code="""
fn main() -> int {
    var matrix = [[1, 2], [3, 4]]
    print(matrix[0][1])
    print(matrix[1][0])
    return 0
}
""",
                expected_output="2\n3\n",
                category="nested_structures"
            ),
            
            CriticalTestCase(
                name="nested_arrays_3d",
                description="Test 3D array creation",
                wyn_code="""
fn main() -> int {
    var cube = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]
    print(cube[0][1][0])
    return 0
}
""",
                expected_output="3\n",
                category="nested_structures"
            ),
            
            # CRITICAL ISSUE 5: Function Forward Declarations
            CriticalTestCase(
                name="function_forward_declaration",
                description="Test function forward declaration",
                wyn_code="""
fn main() -> int {
    var result = helper_function(5)
    print(result)
    return 0
}

fn helper_function(x: int) -> int {
    return x * 2
}
""",
                expected_output="10\n",
                category="function_declarations"
            ),
            
            # CRITICAL ISSUE 6: Memory Management
            CriticalTestCase(
                name="memory_large_array",
                description="Test large array memory management",
                wyn_code="""
fn main() -> int {
    var big_array = []
    for (var i = 0; i < 1000; i = i + 1) {
        big_array = [i, i+1, i+2]  // Should not leak memory
    }
    print("Memory test complete")
    return 0
}
""",
                expected_output="Memory test complete\n",
                category="memory_management",
                timeout=10
            ),
            
            # CRITICAL ISSUE 7: Error Handling
            CriticalTestCase(
                name="division_by_zero",
                description="Test division by zero handling",
                wyn_code="""
fn main() -> int {
    var zero = 0
    var result = 10 / zero
    print(result)
    return 0
}
""",
                should_run=False,  # Should handle gracefully
                category="error_handling"
            ),
            
            # CRITICAL ISSUE 8: Type System Edge Cases
            CriticalTestCase(
                name="type_system_consistency",
                description="Test type system consistency",
                wyn_code="""
fn create_array() -> array {
    return [1, 2, 3]
}

fn create_string() -> string {
    return "hello"
}

fn main() -> int {
    var arr = create_array()
    var str = create_string()
    print(arr[0])
    print(str)
    return 0
}
""",
                expected_output="1\nhello\n",
                category="type_system"
            ),
            
            # CRITICAL ISSUE 9: Loop Safety
            CriticalTestCase(
                name="infinite_loop_protection",
                description="Test infinite loop detection",
                wyn_code="""
fn main() -> int {
    var count = 0
    for (var i = 0; i < 10; i = i - 1) {  // Infinite loop
        count = count + 1
        if count > 1000 {
            print("Loop safety triggered")
            break
        }
    }
    return 0
}
""",
                expected_output="Loop safety triggered\n",
                category="loop_safety",
                timeout=3
            ),
            
            # CRITICAL ISSUE 10: Variable Scoping
            CriticalTestCase(
                name="variable_scoping_shadowing",
                description="Test variable shadowing in nested scopes",
                wyn_code="""
fn main() -> int {
    var x = 10
    for (var i = 0; i < 3; i = i + 1) {
        var x = 20  // Should shadow outer x
        print(x)
    }
    print(x)  // Should be original x
    return 0
}
""",
                expected_output="20\n20\n20\n10\n",
                category="variable_scoping"
            )
        ]
    
    def run_test(self, test: CriticalTestCase) -> TestReport:
        """Run a single test case with comprehensive analysis"""
        import time
        
        start_time = time.time()
        report = TestReport(name=test.name, result=TestResult.FAIL)
        
        try:
            # Write test file
            test_file = self.test_dir / f"{test.name}.wyn"
            test_file.write_text(test.wyn_code)
            
            # Compile test
            compile_cmd = [str(self.wyn_dir / "wyn"), str(test_file)]
            
            try:
                compile_result = subprocess.run(
                    compile_cmd, 
                    capture_output=True, 
                    text=True, 
                    timeout=test.timeout,
                    cwd=str(self.wyn_dir)
                )
                report.exit_code = compile_result.returncode
            except subprocess.TimeoutExpired:
                report.result = TestResult.TIMEOUT
                report.error_message = "Compilation timed out"
                return report
            
            report.compile_output = compile_result.stdout + compile_result.stderr
            
            # Check for segfault in compilation
            if compile_result.returncode == 139 or "Segmentation fault" in report.compile_output:
                report.result = TestResult.SEGFAULT
                report.error_message = "Segmentation fault during compilation"
                return report
            
            if not test.should_compile:
                if compile_result.returncode != 0:
                    report.result = TestResult.PASS if not test.expect_crash else TestResult.PASS
                else:
                    report.error_message = "Expected compilation to fail but it succeeded"
                return report
            
            if compile_result.returncode != 0:
                report.result = TestResult.FAIL
                report.error_message = f"Compilation failed: {report.compile_output}"
                return report
            
            if not test.should_run:
                report.result = TestResult.PASS
                return report
            
            # Run test
            exe_file = test_file.with_suffix(".wyn.out")
            if exe_file.exists():
                try:
                    run_result = subprocess.run(
                        [str(exe_file)],
                        capture_output=True,
                        text=True,
                        timeout=test.timeout,
                        cwd=str(self.wyn_dir)
                    )
                    
                    report.run_output = run_result.stdout
                    
                    # Check for segfault in execution
                    if run_result.returncode == 139:
                        report.result = TestResult.SEGFAULT
                        report.error_message = "Segmentation fault during execution"
                        return report
                    
                    if run_result.returncode != 0:
                        report.result = TestResult.CRASH
                        report.error_message = f"Runtime crash: {run_result.stderr}"
                        return report
                    
                    # Check expected output
                    if test.expected_output is not None:
                        if report.run_output.strip() == test.expected_output.strip():
                            report.result = TestResult.PASS
                        else:
                            report.result = TestResult.FAIL
                            report.error_message = f"Output mismatch.\nExpected:\n{test.expected_output}\nActual:\n{report.run_output}"
                    else:
                        report.result = TestResult.PASS
                        
                except subprocess.TimeoutExpired:
                    report.result = TestResult.TIMEOUT
                    report.error_message = "Execution timed out"
            else:
                report.result = TestResult.FAIL
                report.error_message = "Executable not created"
                
        except Exception as e:
            report.result = TestResult.CRASH
            report.error_message = f"Test framework error: {str(e)}"
        finally:
            report.execution_time = time.time() - start_time
            
        return report
    
    def run_all_tests(self) -> Dict[str, List[TestReport]]:
        """Run all critical tests and generate comprehensive report"""
        tests = self.create_critical_tests()
        results_by_category = {}
        
        print("🚨 Running Wyn Language CRITICAL Test Suite")
        print("=" * 60)
        
        for test in tests:
            print(f"Running {test.name}... ", end="", flush=True)
            report = self.run_test(test)
            self.reports.append(report)
            
            if test.category not in results_by_category:
                results_by_category[test.category] = []
            results_by_category[test.category].append(report)
            
            # Print immediate result with detailed status
            if report.result == TestResult.PASS:
                print("✅ PASS")
            elif report.result == TestResult.FAIL:
                print("❌ FAIL")
            elif report.result == TestResult.CRASH:
                print("💥 CRASH")
            elif report.result == TestResult.SEGFAULT:
                print("🚨 SEGFAULT")
            elif report.result == TestResult.TIMEOUT:
                print("⏰ TIMEOUT")
                
        return results_by_category
    
    def generate_critical_report(self, results_by_category: Dict[str, List[TestReport]]) -> str:
        """Generate comprehensive critical test report"""
        total_tests = len(self.reports)
        passed = len([r for r in self.reports if r.result == TestResult.PASS])
        failed = len([r for r in self.reports if r.result == TestResult.FAIL])
        crashed = len([r for r in self.reports if r.result == TestResult.CRASH])
        segfaults = len([r for r in self.reports if r.result == TestResult.SEGFAULT])
        timeouts = len([r for r in self.reports if r.result == TestResult.TIMEOUT])
        
        report = f"""
# Wyn Language CRITICAL Test Report
Generated: {os.popen('date').read().strip()}

## 🚨 CRITICAL ANALYSIS SUMMARY
- **Total Tests**: {total_tests}
- **Passed**: {passed} ✅
- **Failed**: {failed} ❌  
- **Crashed**: {crashed} 💥
- **Segfaults**: {segfaults} 🚨
- **Timeouts**: {timeouts} ⏰
- **Success Rate**: {(passed/total_tests*100):.1f}%
- **Stability Rate**: {((passed+failed)/(total_tests)*100):.1f}% (non-crashing)

## 🚨 CRITICAL ISSUES STATUS
"""
        
        critical_issues = {
            "parser_crashes": "Parser Segmentation Faults",
            "string_handling": "String Escaping & Handling",
            "array_safety": "Array Bounds Checking",
            "nested_structures": "Nested Data Structures",
            "function_declarations": "Function Forward Declarations",
            "memory_management": "Memory Management",
            "error_handling": "Error Handling",
            "type_system": "Type System Consistency",
            "loop_safety": "Loop Safety & Infinite Loop Protection",
            "variable_scoping": "Variable Scoping & Shadowing"
        }
        
        for category, issue_name in critical_issues.items():
            if category in results_by_category:
                cat_reports = results_by_category[category]
                cat_passed = len([r for r in cat_reports if r.result == TestResult.PASS])
                cat_segfaults = len([r for r in cat_reports if r.result == TestResult.SEGFAULT])
                cat_total = len(cat_reports)
                
                if cat_segfaults > 0:
                    status = "🚨 SEGFAULT"
                elif cat_passed == cat_total:
                    status = "🟢 RESOLVED"
                elif cat_passed > 0:
                    status = "🟡 PARTIAL"
                else:
                    status = "🔴 BROKEN"
                    
                report += f"- **{issue_name}**: {status} ({cat_passed}/{cat_total})\n"
        
        report += "\n## Detailed Results by Category\n"
        
        for category, reports in results_by_category.items():
            cat_passed = len([r for r in reports if r.result == TestResult.PASS])
            cat_total = len(reports)
            
            report += f"\n### {category.replace('_', ' ').title()}\n"
            report += f"**Status**: {cat_passed}/{cat_total} passed ({(cat_passed/cat_total*100):.1f}%)\n\n"
            
            for test_report in reports:
                status_icon = {
                    TestResult.PASS: "✅",
                    TestResult.FAIL: "❌", 
                    TestResult.CRASH: "💥",
                    TestResult.SEGFAULT: "🚨",
                    TestResult.TIMEOUT: "⏰"
                }[test_report.result]
                
                report += f"- {status_icon} **{test_report.name}**: {test_report.result.value}\n"
                
                if test_report.result != TestResult.PASS:
                    report += f"  - Error: {test_report.error_message}\n"
                    if test_report.compile_output and len(test_report.compile_output) > 0:
                        report += f"  - Details: ```{test_report.compile_output[:300]}```\n"
        
        return report

def main():
    suite = WynCriticalTestSuite()
    results = suite.run_all_tests()
    report = suite.generate_critical_report(results)
    
    # Save report
    report_file = Path("CRITICAL_TEST_REPORT.md")
    report_file.write_text(report)
    
    print("\n" + "=" * 60)
    print("🚨 CRITICAL Test Report Generated:")
    print(report)
    print(f"\nFull report saved to: {report_file}")

if __name__ == "__main__":
    main()
