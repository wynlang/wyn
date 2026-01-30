; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @hash(i32 %val) {
entry:
  %i = alloca i32, align 4
  %v = alloca i32, align 4
  %h = alloca i32, align 4
  %val1 = alloca i32, align 4
  store i32 %val, ptr %val1, align 4
  store i32 5381, ptr %h, align 4
  %val2 = load i32, ptr %val1, align 4
  store i32 %val2, ptr %v, align 4
  store i32 0, ptr %i, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %i3 = load i32, ptr %i, align 4
  %icmp = icmp slt i32 %i3, 4
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %h4 = load i32, ptr %h, align 4
  %mul = mul i32 %h4, 33
  %v5 = load i32, ptr %v, align 4
  %rem = srem i32 %v5, 256
  %add = add i32 %mul, %rem
  store i32 %add, ptr %h, align 4
  %v6 = load i32, ptr %v, align 4
  %div = sdiv i32 %v6, 256
  store i32 %div, ptr %v, align 4
  %i7 = load i32, ptr %i, align 4
  %add8 = add i32 %i7, 1
  store i32 %add8, ptr %i, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %h9 = load i32, ptr %h, align 4
  ret i32 %h9
}

define i32 @checksum(i32 %data) {
entry:
  %d = alloca i32, align 4
  %cs = alloca i32, align 4
  %data1 = alloca i32, align 4
  store i32 %data, ptr %data1, align 4
  store i32 0, ptr %cs, align 4
  %data2 = load i32, ptr %data1, align 4
  store i32 %data2, ptr %d, align 4
  br label %while.header

while.header:                                     ; preds = %while.body, %entry
  %d3 = load i32, ptr %d, align 4
  %icmp = icmp sgt i32 %d3, 0
  br i1 %icmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.header
  %cs4 = load i32, ptr %cs, align 4
  %d5 = load i32, ptr %d, align 4
  %rem = srem i32 %d5, 256
  %add = add i32 %cs4, %rem
  store i32 %add, ptr %cs, align 4
  %d6 = load i32, ptr %d, align 4
  %div = sdiv i32 %d6, 256
  store i32 %div, ptr %d, align 4
  br label %while.header

while.end:                                        ; preds = %while.header
  %cs7 = load i32, ptr %cs, align 4
  %rem8 = srem i32 %cs7, 256
  ret i32 %rem8
}

define i32 @wyn_main() {
entry:
  %c = alloca i32, align 4
  %h = alloca i32, align 4
  %hash = call i32 @hash(i32 12345)
  store i32 %hash, ptr %h, align 4
  %checksum = call i32 @checksum(i32 1000)
  store i32 %checksum, ptr %c, align 4
  %c1 = load i32, ptr %c, align 4
  ret i32 %c1
}
