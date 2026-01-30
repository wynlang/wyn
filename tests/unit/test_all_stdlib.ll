; ModuleID = 'wyn_program'
source_filename = "wyn_program"

@__wyn_argc = global i32 0
@__wyn_argv = global ptr null

declare ptr @malloc(i64)

declare void @free(ptr)

declare i32 @printf(ptr, ...)

define i32 @abs_test(i32 %x) {
entry:
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %x2 = load i32, ptr %x1, align 4
  %icmp = icmp slt i32 %x2, 0
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %x3 = load i32, ptr %x1, align 4
  %sub = sub i32 0, %x3
  ret i32 %sub

if.end:                                           ; preds = %entry
  %x4 = load i32, ptr %x1, align 4
  ret i32 %x4
}

define i32 @max_test(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %icmp = icmp sgt i32 %a3, %b4
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %a5 = load i32, ptr %a1, align 4
  ret i32 %a5

if.end:                                           ; preds = %entry
  %b6 = load i32, ptr %b2, align 4
  ret i32 %b6
}

define i32 @wyn_year() {
entry:
  ret i32 2026
}

define i32 @wyn_cpu_count() {
entry:
  ret i32 8
}

define i32 @json_parse_int(i32 %s) {
entry:
  %s1 = alloca i32, align 4
  store i32 %s, ptr %s1, align 4
  ret i32 42
}

define i32 @crypto_checksum(i32 %data) {
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

define i32 @http_status_ok() {
entry:
  ret i32 200
}

define i32 @http_is_success(i32 %status) {
entry:
  %status1 = alloca i32, align 4
  store i32 %status, ptr %status1, align 4
  %status2 = load i32, ptr %status1, align 4
  %icmp = icmp sge i32 %status2, 200
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %status3 = load i32, ptr %status1, align 4
  %icmp4 = icmp slt i32 %status3, 300
  br i1 %icmp4, label %if.then5, label %if.end6

if.end:                                           ; preds = %if.end6, %entry
  ret i32 0

if.then5:                                         ; preds = %if.then
  ret i32 1

if.end6:                                          ; preds = %if.then
  br label %if.end
}

define i32 @db_is_connected(i32 %state) {
entry:
  %state1 = alloca i32, align 4
  store i32 %state, ptr %state1, align 4
  %state2 = load i32, ptr %state1, align 4
  %icmp = icmp eq i32 %state2, 1
  br i1 %icmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  ret i32 1

if.end:                                           ; preds = %entry
  ret i32 0
}

define i32 @db_count_results(i32 %rows) {
entry:
  %rows1 = alloca i32, align 4
  store i32 %rows, ptr %rows1, align 4
  %rows2 = load i32, ptr %rows1, align 4
  ret i32 %rows2
}

define i32 @wyn_main() {
entry:
  %db_test = alloca i32, align 4
  %http_test = alloca i32, align 4
  %crypto_test = alloca i32, align 4
  %json_test = alloca i32, align 4
  %os_test = alloca i32, align 4
  %time_test = alloca i32, align 4
  %math_test = alloca i32, align 4
  %abs_test = call i32 @abs_test(i32 -5)
  %max_test = call i32 @max_test(i32 10, i32 20)
  %add = add i32 %abs_test, %max_test
  store i32 %add, ptr %math_test, align 4
  %wyn_year = call i32 @wyn_year()
  %sub = sub i32 %wyn_year, 2000
  store i32 %sub, ptr %time_test, align 4
  %wyn_cpu_count = call i32 @wyn_cpu_count()
  store i32 %wyn_cpu_count, ptr %os_test, align 4
  %json_parse_int = call i32 @json_parse_int(i32 0)
  store i32 %json_parse_int, ptr %json_test, align 4
  %crypto_checksum = call i32 @crypto_checksum(i32 255)
  store i32 %crypto_checksum, ptr %crypto_test, align 4
  %http_status_ok = call i32 @http_status_ok()
  %div = sdiv i32 %http_status_ok, 100
  store i32 %div, ptr %http_test, align 4
  %db_count_results = call i32 @db_count_results(i32 10)
  store i32 %db_count_results, ptr %db_test, align 4
  ret i32 88
}
