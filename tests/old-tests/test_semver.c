#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/semver.h"

void test_semver_parse() {
    SemVer ver;
    
    // Test basic version
    assert(semver_parse("1.2.3", &ver) == 0);
    assert(ver.major == 1 && ver.minor == 2 && ver.patch == 3);
    
    // Test with 'v' prefix
    assert(semver_parse("v2.0.1", &ver) == 0);
    assert(ver.major == 2 && ver.minor == 0 && ver.patch == 1);
    
    printf("✓ semver_parse tests passed\n");
}

void test_constraint_parse() {
    VersionConstraint constraint;
    
    // Test exact version
    assert(constraint_parse("1.0.0", &constraint) == 0);
    assert(constraint.type == CONSTRAINT_EXACT);
    assert(constraint.version.major == 1);
    
    // Test caret (^)
    assert(constraint_parse("^1.2.3", &constraint) == 0);
    assert(constraint.type == CONSTRAINT_CARET);
    assert(constraint.version.major == 1);
    
    // Test tilde (~)
    assert(constraint_parse("~1.2.3", &constraint) == 0);
    assert(constraint.type == CONSTRAINT_TILDE);
    
    // Test >=
    assert(constraint_parse(">=2.0.0", &constraint) == 0);
    assert(constraint.type == CONSTRAINT_GTE);
    
    // Test wildcard
    assert(constraint_parse("*", &constraint) == 0);
    assert(constraint.type == CONSTRAINT_ANY);
    
    printf("✓ constraint_parse tests passed\n");
}

void test_semver_compare() {
    SemVer v1 = {1, 0, 0};
    SemVer v2 = {2, 0, 0};
    SemVer v3 = {1, 1, 0};
    SemVer v4 = {1, 0, 1};
    
    assert(semver_compare(&v1, &v2) < 0);  // 1.0.0 < 2.0.0
    assert(semver_compare(&v2, &v1) > 0);  // 2.0.0 > 1.0.0
    assert(semver_compare(&v1, &v1) == 0); // 1.0.0 == 1.0.0
    assert(semver_compare(&v1, &v3) < 0);  // 1.0.0 < 1.1.0
    assert(semver_compare(&v1, &v4) < 0);  // 1.0.0 < 1.0.1
    
    printf("✓ semver_compare tests passed\n");
}

void test_semver_satisfies() {
    SemVer v120 = {1, 2, 0};
    SemVer v123 = {1, 2, 3};
    SemVer v125 = {1, 2, 5};
    SemVer v130 = {1, 3, 0};
    SemVer v200 = {2, 0, 0};
    
    // Test exact match
    VersionConstraint exact;
    constraint_parse("1.2.3", &exact);
    assert(semver_satisfies(&v123, &exact) == 1);
    assert(semver_satisfies(&v125, &exact) == 0);
    
    // Test caret (^1.2.3 = >=1.2.3, <2.0.0)
    VersionConstraint caret;
    constraint_parse("^1.2.3", &caret);
    assert(semver_satisfies(&v120, &caret) == 0);  // 1.2.0 < 1.2.3
    assert(semver_satisfies(&v123, &caret) == 1);  // 1.2.3 matches
    assert(semver_satisfies(&v125, &caret) == 1);  // 1.2.5 matches
    assert(semver_satisfies(&v130, &caret) == 1);  // 1.3.0 matches
    assert(semver_satisfies(&v200, &caret) == 0);  // 2.0.0 too high
    
    // Test tilde (~1.2.3 = >=1.2.3, <1.3.0)
    VersionConstraint tilde;
    constraint_parse("~1.2.3", &tilde);
    assert(semver_satisfies(&v120, &tilde) == 0);  // 1.2.0 < 1.2.3
    assert(semver_satisfies(&v123, &tilde) == 1);  // 1.2.3 matches
    assert(semver_satisfies(&v125, &tilde) == 1);  // 1.2.5 matches
    assert(semver_satisfies(&v130, &tilde) == 0);  // 1.3.0 too high
    
    // Test >=
    VersionConstraint gte;
    constraint_parse(">=1.2.3", &gte);
    assert(semver_satisfies(&v120, &gte) == 0);
    assert(semver_satisfies(&v123, &gte) == 1);
    assert(semver_satisfies(&v200, &gte) == 1);
    
    // Test wildcard
    VersionConstraint any;
    constraint_parse("*", &any);
    assert(semver_satisfies(&v120, &any) == 1);
    assert(semver_satisfies(&v200, &any) == 1);
    
    printf("✓ semver_satisfies tests passed\n");
}

int main() {
    printf("Running semver tests...\n\n");
    
    test_semver_parse();
    test_constraint_parse();
    test_semver_compare();
    test_semver_satisfies();
    
    printf("\n✅ All semver tests passed!\n");
    return 0;
}
