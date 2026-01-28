; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @fetch_data() {
entry:
  ret i32 42
}

define i32 @process_data(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %value2 = load i32, ptr %value1, align 4
  %mul = mul i32 %value2, i32 2
  ret i32 %mul
}

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %future2 = alloca i32, align 4
  %data = alloca i32, align 4
  %future1 = alloca i32, align 4
  %fetch_data = call i32 @fetch_data()
  store i32 %fetch_data, ptr %future1, align 4
  %data1 = load i32, ptr %data, align 4
  %process_data = call i32 @process_data(i32 %data1)
  store i32 %process_data, ptr %future2, align 4
  ret i32 0
}
