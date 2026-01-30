; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

declare void @print(i32)

declare void @print_string(ptr)

define i32 @hash(i32 %key, i32 %size) {
entry:
  %size2 = alloca i32, align 4
  %key1 = alloca i32, align 4
  store i32 %key, ptr %key1, align 4
  store i32 %size, ptr %size2, align 4
  %key3 = load i32, ptr %key1, align 4
  %key4 = load i32, ptr %key1, align 4
  %size5 = load i32, ptr %size2, align 4
  %div = sdiv i32 %key4, %size5
  %size6 = load i32, ptr %size2, align 4
  %mul = mul i32 %div, %size6
  %sub = sub i32 %key3, %mul
  ret i32 %sub
}

define i32 @wyn_main() {
entry:
  %h3 = alloca i32, align 4
  %h2 = alloca i32, align 4
  %h1 = alloca i32, align 4
  %size = alloca i32, align 4
  store i32 10, ptr %size, align 4
  %size1 = load i32, ptr %size, align 4
  %hash = call i32 @hash(i32 15, i32 %size1)
  store i32 %hash, ptr %h1, align 4
  %size2 = load i32, ptr %size, align 4
  %hash3 = call i32 @hash(i32 25, i32 %size2)
  store i32 %hash3, ptr %h2, align 4
  %size4 = load i32, ptr %size, align 4
  %hash5 = call i32 @hash(i32 37, i32 %size4)
  store i32 %hash5, ptr %h3, align 4
  %h16 = load i32, ptr %h1, align 4
  %h27 = load i32, ptr %h2, align 4
  %add = add i32 %h16, %h27
  %h38 = load i32, ptr %h3, align 4
  %add9 = add i32 %add, %h38
  ret i32 %add9
}
