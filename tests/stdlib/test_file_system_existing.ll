; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %args = alloca i32, align 4
  %is_d = alloca i32, align 4
  %is_f = alloca i32, align 4
  %files = alloca i32, align 4
  %files1 = load i32, ptr %files, align 4
  %i8_ptr = bitcast i32 %files1 to ptr
  %length_i8_ptr = getelementptr i8, ptr %i8_ptr, i64 -8
  %array_length = load i64, ptr %length_i8_ptr, align 4
  %length_int = trunc i64 %array_length to i32
  %icmp = icmp eq i32 %length_int, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %is_f2 = load i32, ptr %is_f, align 4
  %icmp3 = icmp eq i32 %is_f2, 0
  br i1 %icmp3, label %if.then4, label %if.end5

if.then4:                                         ; preds = %if.end
  ret i32 2

if.end5:                                          ; preds = %if.end
  %is_d6 = load i32, ptr %is_d, align 4
  %icmp7 = icmp eq i32 %is_d6, 0
  br i1 %icmp7, label %if.then8, label %if.end9

if.then8:                                         ; preds = %if.end5
  ret i32 3

if.end9:                                          ; preds = %if.end5
  %args10 = load i32, ptr %args, align 4
  %i8_ptr11 = bitcast i32 %args10 to ptr
  %length_i8_ptr12 = getelementptr i8, ptr %i8_ptr11, i64 -8
  %array_length13 = load i64, ptr %length_i8_ptr12, align 4
  %length_int14 = trunc i64 %array_length13 to i32
  %icmp15 = icmp eq i32 %length_int14, 0
  br i1 %icmp15, label %if.then16, label %if.end17

if.then16:                                        ; preds = %if.end9
  ret i32 4

if.end17:                                         ; preds = %if.end9
  ret i32 0
}
