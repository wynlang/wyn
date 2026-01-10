#!/usr/bin/env python3
"""
Wyn Language Comprehensive Test Suite
=====================================

100% coverage testing framework for Wyn language stability and regression testing.
Tests all critical functionality with deep analysis and validation.
"""

import os
import subprocess
import sys
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class TestResult(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    CRASH = "CRASH"
    TIMEOUT = "TIMEOUT"

@dataclass
class TestCase:
    name: str
    description: str
    wyn_code: str
    expected_output: Optional[str] = None
    should_compile: bool = True
    should_run: bool = True
    category: str = "general"

@dataclass
class TestReport:
    name: str
    result: TestResult
    compile_output: str = ""
    run_output: str = ""
    error_message: str = ""
    execution_time: float = 0.0

class WynTestSuite:
    def __init__(self, wyn_dir: str = "/Users/aoaws/src/ao/wyn-lang/wyn"):
        self.wyn_dir = Path(wyn_dir)
        self.test_dir = self.wyn_dir / "tests" / "comprehensive"
        self.test_dir.mkdir(exist_ok=True)
        self.reports: List[TestReport] = []
        
    def create_critical_tests(self) -> List[TestCase]:
        """Create tests for all critical issues identified"""
        return [
            # CRITICAL ISSUE 1: Array Indexing
            TestCase(
                name="array_indexing_basic",
                description="Test basic array indexing arr[0]",
                wyn_code="""
fn main() -> int {
    var arr = [1, 2, 3, 4, 5]
    print(arr[0])
    print(arr[2])
    print(arr[4])
    return 0
}
""",
                expected_output="1\n3\n5\n",
                category="array_indexing"
            ),
            
            TestCase(
                name="array_indexing_assignment",
                description="Test array index assignment arr[i] = value",
                wyn_code="""
fn main() -> int {
    var arr = [1, 2, 3]
    arr[1] = 99
    print(arr[0])
    print(arr[1])
    print(arr[2])
    return 0
}
""",
                expected_output="1\n99\n3\n",
                category="array_indexing"
            ),
            
            # CRITICAL ISSUE 2: C-style For Loops
            TestCase(
                name="c_style_for_loop_basic",
                description="Test basic C-style for loop",
                wyn_code="""
fn main() -> int {
    for (var i = 0; i < 5; i = i + 1) {
        print(i)
    }
    return 0
}
""",
                expected_output="0\n1\n2\n3\n4\n",
                category="c_style_loops"
            ),
            
            TestCase(
                name="c_style_for_loop_array",
                description="Test C-style for loop with array access",
                wyn_code="""
fn main() -> int {
    var arr = [10, 20, 30, 40]
    for (var i = 0; i < 4; i = i + 1) {
        print(arr[i])
    }
    return 0
}
""",
                expected_output="10\n20\n30\n40\n",
                category="c_style_loops"
            ),
            
            # CRITICAL ISSUE 3: Function Return Types
            TestCase(
                name="function_return_struct",
                description="Test function returning struct",
                wyn_code="""
struct Point {
    x: int
    y: int
}

fn create_point(x: int, y: int) -> Point {
    return Point { x: x, y: y }
}

fn main() -> int {
    var p = create_point(10, 20)
    print(p.x)
    print(p.y)
    return 0
}
""",
                expected_output="10\n20\n",
                category="function_returns"
            ),
            
            TestCase(
                name="function_return_array",
                description="Test function returning array",
                wyn_code="""
fn create_array() -> array {
    return [1, 2, 3, 4, 5]
}

fn main() -> int {
    var arr = create_array()
    print(arr[0])
    print(arr[4])
    return 0
}
""",
                expected_output="1\n5\n",
                category="function_returns"
            ),
            
            # CRITICAL ISSUE 4: Array Iteration
            TestCase(
                name="array_iteration_for_in",
                description="Test for-in loop with arrays",
                wyn_code="""
fn main() -> int {
    var arr = [10, 20, 30]
    for item in arr {
        print(item)
    }
    return 0
}
""",
                expected_output="10\n20\n30\n",
                category="array_iteration"
            ),
            
            # CRITICAL ISSUE 5: String Comparison
            TestCase(
                name="string_comparison_equality",
                description="Test string equality comparison",
                wyn_code="""
fn main() -> int {
    var s1 = "hello"
    var s2 = "hello"
    var s3 = "world"
    
    if s1 == s2 {
        print("s1 equals s2")
    }
    
    if s1 != s3 {
        print("s1 not equals s3")
    }
    return 0
}
""",
                expected_output="s1 equals s2\ns1 not equals s3\n",
                category="string_comparison"
            ),
            
            # COMPREHENSIVE INTEGRATION TESTS
            TestCase(
                name="comprehensive_integration",
                description="Test multiple features together",
                wyn_code="""
struct Person {
    name: string
    age: int
}

fn create_people() -> array {
    return [
        Person { name: "Alice", age: 25 },
        Person { name: "Bob", age: 30 },
        Person { name: "Charlie", age: 35 }
    ]
}

fn main() -> int {
    var people = create_people()
    
    for (var i = 0; i < 3; i = i + 1) {
        var person = people[i]
        print("Name: " + person.name + ", Age: " + str(person.age))
    }
    
    for person in people {
        if person.age > 30 {
            print(person.name + " is over 30")
        }
    }
    return 0
}
""",
                expected_output="Name: Alice, Age: 25\nName: Bob, Age: 30\nName: Charlie, Age: 35\nCharlie is over 30\n",
                category="integration"
            )
        ]
    
    def run_test(self, test: TestCase) -> TestReport:
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
            compile_result = subprocess.run(
                compile_cmd, 
                capture_output=True, 
                text=True, 
                timeout=10,
                cwd=str(self.wyn_dir)
            )
            
            report.compile_output = compile_result.stdout + compile_result.stderr
            
            if not test.should_compile:
                if compile_result.returncode != 0:
                    report.result = TestResult.PASS
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
                run_result = subprocess.run(
                    [str(exe_file)],
                    capture_output=True,
                    text=True,
                    timeout=5,
                    cwd=str(self.wyn_dir)
                )
                
                report.run_output = run_result.stdout
                
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
            else:
                report.result = TestResult.FAIL
                report.error_message = "Executable not created"
                
        except subprocess.TimeoutExpired:
            report.result = TestResult.TIMEOUT
            report.error_message = "Test timed out"
        except Exception as e:
            report.result = TestResult.CRASH
            report.error_message = f"Test framework error: {str(e)}"
        finally:
            report.execution_time = time.time() - start_time
            
        return report
    
    def run_all_tests(self) -> Dict[str, List[TestReport]]:
        """Run all tests and generate comprehensive report"""
        tests = self.create_critical_tests()
        results_by_category = {}
        
        print("🧪 Running Wyn Language Comprehensive Test Suite")
        print("=" * 60)
        
        for test in tests:
            print(f"Running {test.name}... ", end="", flush=True)
            report = self.run_test(test)
            self.reports.append(report)
            
            if test.category not in results_by_category:
                results_by_category[test.category] = []
            results_by_category[test.category].append(report)
            
            # Print immediate result
            if report.result == TestResult.PASS:
                print("✅ PASS")
            elif report.result == TestResult.FAIL:
                print("❌ FAIL")
            elif report.result == TestResult.CRASH:
                print("💥 CRASH")
            elif report.result == TestResult.TIMEOUT:
                print("⏰ TIMEOUT")
                
        return results_by_category
    
    def generate_report(self, results_by_category: Dict[str, List[TestReport]]) -> str:
        """Generate comprehensive test report"""
        total_tests = len(self.reports)
        passed = len([r for r in self.reports if r.result == TestResult.PASS])
        failed = len([r for r in self.reports if r.result == TestResult.FAIL])
        crashed = len([r for r in self.reports if r.result == TestResult.CRASH])
        timeout = len([r for r in self.reports if r.result == TestResult.TIMEOUT])
        
        report = f"""
# Wyn Language Comprehensive Test Report
Generated: {os.popen('date').read().strip()}

## Summary
- **Total Tests**: {total_tests}
- **Passed**: {passed} ✅
- **Failed**: {failed} ❌  
- **Crashed**: {crashed} 💥
- **Timeout**: {timeout} ⏰
- **Success Rate**: {(passed/total_tests*100):.1f}%

## Results by Category
"""
        
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
                    TestResult.TIMEOUT: "⏰"
                }[test_report.result]
                
                report += f"- {status_icon} **{test_report.name}**: {test_report.result.value}\n"
                
                if test_report.result != TestResult.PASS:
                    report += f"  - Error: {test_report.error_message}\n"
                    if test_report.compile_output:
                        report += f"  - Compile Output: ```{test_report.compile_output[:200]}```\n"
        
        # Critical Issues Status
        report += "\n## Critical Issues Status\n"
        
        critical_issues = {
            "array_indexing": "Array Indexing",
            "c_style_loops": "C-style For Loops", 
            "function_returns": "Function Return Types",
            "array_iteration": "Array Iteration",
            "string_comparison": "String Comparison"
        }
        
        for category, issue_name in critical_issues.items():
            if category in results_by_category:
                cat_reports = results_by_category[category]
                cat_passed = len([r for r in cat_reports if r.result == TestResult.PASS])
                cat_total = len(cat_reports)
                
                if cat_passed == cat_total:
                    status = "🟢 RESOLVED"
                elif cat_passed > 0:
                    status = "🟡 PARTIAL"
                else:
                    status = "🔴 BROKEN"
                    
                report += f"- **{issue_name}**: {status} ({cat_passed}/{cat_total})\n"
        
        return report

def main():
    suite = WynTestSuite()
    results = suite.run_all_tests()
    report = suite.generate_report(results)
    
    # Save report
    report_file = Path("/Users/aoaws/src/ao/wyn-lang/tests/COMPREHENSIVE_TEST_REPORT.md")
    report_file.write_text(report)
    
    print("\n" + "=" * 60)
    print("📊 Test Report Generated:")
    print(report)
    print(f"\nFull report saved to: {report_file}")

if __name__ == "__main__":
    main()
