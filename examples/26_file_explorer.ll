; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"12345\00", align 1
@str.2 = private unnamed_addr constant [9 x i8] c"hello123\00", align 1
@str.3 = private unnamed_addr constant [11 x i8] c"/home/user\00", align 1
@str.4 = private unnamed_addr constant [13 x i8] c"document.txt\00", align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %extension = alloca i32, align 4
  %dirname = alloca i32, align 4
  %basename = alloca i32, align 4
  %full_path = alloca i32, align 4
  %file = alloca i32, align 4
  %dir = alloca i32, align 4
  %alnum_str = alloca i32, align 4
  %digit_str = alloca i32, align 4
  %alpha_str = alloca i32, align 4
  %i = alloca i32, align 4
  %entries = alloca i32, align 4
  %cwd = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %entry
  store ptr @str, ptr %alpha_str, align 8
  store ptr @str.1, ptr %digit_str, align 8
  store ptr @str.2, ptr %alnum_str, align 8
  store ptr @str.3, ptr %dir, align 8
  store ptr @str.4, ptr %file, align 8
  ret i32 0

while.body:                                       ; No predecessors!

while.end:                                        ; No predecessors!
}
