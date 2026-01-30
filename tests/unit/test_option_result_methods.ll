; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %err_val = alloca ptr, align 8
  %ok_val = alloca ptr, align 8
  %none_val = alloca ptr, align 8
  %some_val = alloca ptr, align 8
  %exit_code = alloca i32, align 4
  store i32 0, ptr %exit_code, align 4
  %tmp = alloca i32, align 4
  store i32 42, ptr %tmp, align 4
  %wyn_some = call ptr @wyn_some(ptr %tmp, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_some, ptr %some_val, align 8
  %some_val1 = load ptr, ptr %some_val, align 8
  %some_val2 = load ptr, ptr %some_val, align 8
  %wyn_none = call ptr @wyn_none()
  store ptr %wyn_none, ptr %none_val, align 8
  %none_val3 = load ptr, ptr %none_val, align 8
  %none_val4 = load ptr, ptr %none_val, align 8
  %tmp5 = alloca i32, align 4
  store i32 100, ptr %tmp5, align 4
  %wyn_ok = call ptr @wyn_ok(ptr %tmp5, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_ok, ptr %ok_val, align 8
  %ok_val6 = load ptr, ptr %ok_val, align 8
  %ok_val7 = load ptr, ptr %ok_val, align 8
  %tmp8 = alloca i32, align 4
  store i32 404, ptr %tmp8, align 4
  %wyn_err = call ptr @wyn_err(ptr %tmp8, i64 ptrtoint (ptr getelementptr (i32, ptr null, i32 1) to i64))
  store ptr %wyn_err, ptr %err_val, align 8
  %err_val9 = load ptr, ptr %err_val, align 8
  %err_val10 = load ptr, ptr %err_val, align 8
  %exit_code11 = load i32, ptr %exit_code, align 4
  ret i32 %exit_code11
}

declare ptr @wyn_some(ptr, i64)

declare ptr @wyn_none()

declare ptr @wyn_ok(ptr, i64)

declare ptr @wyn_err(ptr, i64)
