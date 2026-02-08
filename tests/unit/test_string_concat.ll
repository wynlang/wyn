; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [6 x i8] c"hello\00", align 1
@str.1 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@str.2 = private unnamed_addr constant [2 x i8] c"a\00", align 1
@str.3 = private unnamed_addr constant [2 x i8] c"b\00", align 1
@str.4 = private unnamed_addr constant [2 x i8] c"c\00", align 1
@str.5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.6 = private unnamed_addr constant [5 x i8] c"test\00", align 1
@str.7 = private unnamed_addr constant [5 x i8] c"test\00", align 1
@str.8 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %s6 = alloca ptr, align 8
  %s5 = alloca ptr, align 8
  %s4 = alloca ptr, align 8
  %s3 = alloca ptr, align 8
  %s2 = alloca ptr, align 8
  %s1 = alloca ptr, align 8
  store ptr @str, ptr %s1, align 8
  store ptr @str.1, ptr %s2, align 8
  %s11 = load ptr, ptr %s1, align 8
  %s22 = load ptr, ptr %s2, align 8
  %add = add ptr %s11, %s22
  store ptr %add, ptr %s3, align 8
  store ptr add (ptr add (ptr @str.2, ptr @str.3), ptr @str.4), ptr %s4, align 8
  store ptr add (ptr @str.5, ptr @str.6), ptr %s5, align 8
  store ptr add (ptr @str.7, ptr @str.8), ptr %s6, align 8
  ret i32 0
}
