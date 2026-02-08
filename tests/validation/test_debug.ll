; ModuleID = 'wyn_program'
source_filename = "wyn_program"

%Point = type { i32, i32 }

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @wyn_main() {
entry:
  %result = alloca i32, align 4
  %div_result = alloca i32, align 4
  %p = alloca %Point, align 8
  store %Point { i32 10, i32 20 }, ptr %p, align 4
  %p1 = load %Point, ptr %p, align 4
  %field_val = extractvalue %Point %p1, 1
  %div = sdiv i32 %field_val, 2
  store i32 %div, ptr %div_result, align 4
  %div_result2 = load i32, ptr %div_result, align 4
  %icmp = icmp ne i32 %div_result2, 10
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  %p3 = load %Point, ptr %p, align 4
  %field_val4 = extractvalue %Point %p3, 0
  %mul = mul i32 %field_val4, 2
  %p5 = load %Point, ptr %p, align 4
  %field_val6 = extractvalue %Point %p5, 1
  %div7 = sdiv i32 %field_val6, 2
  %add = add i32 %mul, %div7
  store i32 %add, ptr %result, align 4
  %result8 = load i32, ptr %result, align 4
  %icmp9 = icmp ne i32 %result8, 30
  br i1 %icmp9, label %if.then10, label %if.end11

if.then10:                                        ; preds = %if.end
  ret i32 2

if.end11:                                         ; preds = %if.end
  ret i32 0
}
