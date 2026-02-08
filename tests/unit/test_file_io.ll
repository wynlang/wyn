; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [18 x i8] c"/tmp/wyn_test.txt\00", align 1
@str.1 = private unnamed_addr constant [12 x i8] c"Hello, Wyn!\00", align 1
@str.2 = private unnamed_addr constant [18 x i8] c"/tmp/wyn_test.txt\00", align 1
@str.3 = private unnamed_addr constant [18 x i8] c"/tmp/wyn_test.txt\00", align 1
@str.4 = private unnamed_addr constant [12 x i8] c" More text.\00", align 1
@str.5 = private unnamed_addr constant [18 x i8] c"/tmp/wyn_test.txt\00", align 1
@str.6 = private unnamed_addr constant [21 x i8] c"/tmp/nonexistent.txt\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %not_exists = alloca i32, align 4
  %exists = alloca i32, align 4
  %result2 = alloca i32, align 4
  %content = alloca i32, align 4
  %result = alloca i32, align 4
  %wyn_file_write_simple = call i32 @wyn_file_write_simple(ptr @str, ptr @str.1)
  store i32 %wyn_file_write_simple, ptr %result, align 4
  %result1 = load i32, ptr %result, align 4
  %icmp = icmp ne i32 %result1, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %wyn_file_read_simple = call i32 @wyn_file_read_simple(ptr @str.2)
  store i32 %wyn_file_read_simple, ptr %content, align 4
  %content2 = load i32, ptr %content, align 4
  %icmp3 = icmp eq i32 %content2, 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %wyn_file_append_simple = call i32 @wyn_file_append_simple(ptr @str.3, ptr @str.4)
  store i32 %wyn_file_append_simple, ptr %result2, align 4
  %result26 = load i32, ptr %result2, align 4
  %icmp7 = icmp ne i32 %result26, 1
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end5
  ret i32 3

if.end9:                                          ; preds = %if.end5
  %wyn_file_exists_simple = call i32 @wyn_file_exists_simple(ptr @str.5)
  store i32 %wyn_file_exists_simple, ptr %exists, align 4
  %exists10 = load i32, ptr %exists, align 4
  %icmp11 = icmp ne i32 %exists10, 1
  br i1 %icmp11, label %if.then12, label %if.end13

if.then12:                                        ; preds = %if.end9
  ret i32 4

if.end13:                                         ; preds = %if.end9
  %wyn_file_exists_simple14 = call i32 @wyn_file_exists_simple(ptr @str.6)
  store i32 %wyn_file_exists_simple14, ptr %not_exists, align 4
  %not_exists15 = load i32, ptr %not_exists, align 4
  %icmp16 = icmp ne i32 %not_exists15, 0
  br i1 %icmp16, label %if.then17, label %if.end18

if.then17:                                        ; preds = %if.end13
  ret i32 5

if.end18:                                         ; preds = %if.end13
  ret i32 0
}

declare i32 @wyn_file_write_simple(ptr, ptr)

declare i32 @wyn_file_read_simple(ptr)

declare i32 @wyn_file_append_simple(ptr, ptr)

declare i32 @wyn_file_exists_simple(ptr)
