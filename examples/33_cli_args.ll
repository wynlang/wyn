; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null
@str = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %pos_count = alloca i32, align 4
  %config_file = alloca i32, align 4
  %output_file = alloca i32, align 4
  %found_help = alloca i32, align 4
  %found_verbose = alloca i32, align 4
  %i = alloca i32, align 4
  %args = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %entry
  store i1 false, ptr %found_verbose, align 1
  store i1 false, ptr %found_help, align 1
  br label %while.header1

while.body:                                       ; No predecessors!

while.end:                                        ; No predecessors!

while.header1:                                    ; preds = %while.header
  store ptr @str, ptr %output_file, align 8
  store ptr @str.1, ptr %config_file, align 8
  br label %while.header4

while.body2:                                      ; No predecessors!

while.end3:                                       ; No predecessors!

while.header4:                                    ; preds = %while.header1
  store i32 0, ptr %pos_count, align 4
  br label %while.header7

while.body5:                                      ; No predecessors!

while.end6:                                       ; No predecessors!

while.header7:                                    ; preds = %while.header4
  ret i32 0

while.body8:                                      ; No predecessors!

while.end9:                                       ; No predecessors!
}
