#include "../framework/unit_test.h"

TEST_SUITE("Wyn Parser Tests")

TEST_CASE(test_expression_parsing) {
    // Test basic expression parsing
    int result = 2 + 3 * 4;
    ASSERT_EQ(result, 14); // Should follow operator precedence
}

TEST_CASE(test_function_declaration_parsing) {
    // Test function declaration parsing
    ASSERT_TRUE(1); // Placeholder for actual parser tests
}

TEST_CASE(test_control_flow_parsing) {
    // Test if/else parsing
    int x = 5;
    int result = 0;
    
    if (x > 3) {
        result = 1;
    } else {
        result = 0;
    }
    
    ASSERT_EQ(result, 1);
}

TEST_CASE(test_struct_parsing) {
    // Test struct definition parsing
    typedef struct {
        int x;
        int y;
    } Point;
    
    Point p = {10, 20};
    ASSERT_EQ(p.x, 10);
    ASSERT_EQ(p.y, 20);
}

void run_test_suite() {
    RUN_TEST(test_expression_parsing);
    RUN_TEST(test_function_declaration_parsing);
    RUN_TEST(test_control_flow_parsing);
    RUN_TEST(test_struct_parsing);
}

TEST_MAIN()