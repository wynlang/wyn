; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"HELLO\00", align 1
@str.2 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.3 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.4 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.5 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.6 = private unnamed_addr constant [12 x i8] c"hello world\00", align 1
@str.7 = private unnamed_addr constant [4 x i8] c"ell\00", align 1
@str.8 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.9 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@str.10 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.11 = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@str.12 = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.13 = private unnamed_addr constant [16 x i8] c"  hello world  \00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @test_case_conversion() {
entry:
  %cap = alloca i32, align 4
  %hello3 = alloca ptr, align 8
  %lower4 = alloca ptr, align 8
  %hello2 = alloca ptr, align 8
  %upper2 = alloca ptr, align 8
  %hello = alloca ptr, align 8
  store ptr @str, ptr %hello, align 8
  %hello1 = load ptr, ptr %hello, align 8
  %upper = call ptr @wyn_string_upper(ptr %hello1)
  store ptr %upper, ptr %upper2, align 8
  store ptr @str.1, ptr %hello2, align 8
  %hello23 = load ptr, ptr %hello2, align 8
  %lower = call ptr @wyn_string_lower(ptr %hello23)
  store ptr %lower, ptr %lower4, align 8
  store ptr @str.2, ptr %hello3, align 8
  %hello35 = load ptr, ptr %hello3, align 8
  ret i32 0
}

define i32 @test_string_info() {
entry:
  %not_empty = alloca i32, align 4
  %hello = alloca ptr, align 8
  %empty = alloca i32, align 4
  %empty_str = alloca ptr, align 8
  %len = alloca i64, align 8
  %text = alloca ptr, align 8
  store ptr @str.3, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strlen = call i64 @strlen(ptr %text1)
  store i64 %strlen, ptr %len, align 4
  store ptr @str.4, ptr %empty_str, align 8
  %empty_str2 = load ptr, ptr %empty_str, align 8
  store ptr @str.5, ptr %hello, align 8
  %hello3 = load ptr, ptr %hello, align 8
  %len4 = load i64, ptr %len, align 4
  ret i64 %len4
}

define i32 @test_search() {
entry:
  %idx = alloca i32, align 4
  %ends = alloca i32, align 4
  %starts = alloca i32, align 4
  %has_ell = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.6, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %strstr_result = call ptr @strstr(ptr %text1, ptr @str.7)
  %is_found = icmp ne ptr %strstr_result, null
  %contains = zext i1 %is_found to i32
  store i32 %contains, ptr %has_ell, align 4
  %text2 = load ptr, ptr %text, align 8
  %prefix_len = call i64 @strlen(ptr @str.8)
  %strncmp = call i32 @strncmp(ptr %text2, ptr @str.8, i64 %prefix_len)
  %is_equal = icmp eq i32 %strncmp, 0
  %starts_with = zext i1 %is_equal to i32
  store i32 %starts_with, ptr %starts, align 4
  %text3 = load ptr, ptr %text, align 8
  %str_len = call i64 @strlen(ptr %text3)
  %suffix_len = call i64 @strlen(ptr @str.9)
  %offset = sub i64 %str_len, %suffix_len
  %str_end = getelementptr i8, ptr %text3, i64 %offset
  %strcmp = call i32 @strcmp(ptr %str_end, ptr @str.9)
  %is_equal4 = icmp eq i32 %strcmp, 0
  %ends_with = zext i1 %is_equal4 to i32
  store i32 %ends_with, ptr %ends, align 4
  %text5 = load ptr, ptr %text, align 8
  %idx6 = load i32, ptr %idx, align 4
  ret i32 %idx6
}

define i32 @test_manipulation() {
entry:
  %reversed = alloca i32, align 4
  %repeated = alloca i32, align 4
  %hi = alloca ptr, align 8
  %sliced = alloca i32, align 4
  %replaced = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.10, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %text2 = load ptr, ptr %text, align 8
  store ptr @str.11, ptr %hi, align 8
  %hi3 = load ptr, ptr %hi, align 8
  %text4 = load ptr, ptr %text, align 8
  ret i32 0
}

define i32 @test_chaining() {
entry:
  %result = alloca i32, align 4
  %hello = alloca ptr, align 8
  store ptr @str.12, ptr %hello, align 8
  %hello1 = load ptr, ptr %hello, align 8
  %upper = call ptr @wyn_string_upper(ptr %hello1)
  %lower = call ptr @wyn_string_lower(ptr %upper)
  ret i32 0
}

define i32 @test_complex_chaining() {
entry:
  %result = alloca i32, align 4
  %text = alloca ptr, align 8
  store ptr @str.13, ptr %text, align 8
  %text1 = load ptr, ptr %text, align 8
  %upper = call ptr @wyn_string_upper(ptr %text1)
  ret i32 0
}

define i32 @wyn_main() {
entry:
  %idx = alloca i32, align 4
  %len = alloca i32, align 4
  %test_case_conversion = call i32 @test_case_conversion()
  %test_string_info = call i32 @test_string_info()
  store i32 %test_string_info, ptr %len, align 4
  %test_search = call i32 @test_search()
  store i32 %test_search, ptr %idx, align 4
  %test_manipulation = call i32 @test_manipulation()
  %test_chaining = call i32 @test_chaining()
  %test_complex_chaining = call i32 @test_complex_chaining()
  %len1 = load i32, ptr %len, align 4
  %idx2 = load i32, ptr %idx, align 4
  %add = add i32 %len1, %idx2
  ret i32 %add
}

declare ptr @wyn_string_upper(ptr)

declare ptr @wyn_string_lower(ptr)

declare i64 @strlen(ptr)

declare ptr @strstr(ptr, ptr)

declare i32 @strncmp(ptr, ptr, i64)

declare i32 @strcmp(ptr, ptr)
