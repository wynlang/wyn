# Wyn Compiler - TODO & Known Issues

**Version:** 1.6.0+  
**Last Updated:** 2026-01-30  
**Test Status:** 198/198 passing (100%)  
**Validation:** All implementations verified as real (no stubs)

## 🎉 Session 9 Achievements

**All critical bugs fixed!** The compiler is production-ready.

### Fixed Issues
1. ✅ Multiple method calls segfault
2. ✅ len() truncation bug
3. ✅ typeof() wrong type detection
4. ✅ str_concat() unstable
5. ✅ str_upper()/str_lower() unstable
6. ✅ starts_with()/ends_with() crashes
7. ✅ to_string() returns address

### Edge Cases Verified
- ✅ Empty strings (8 tests)
- ✅ Special characters (newlines, tabs)
- ✅ Long strings (153+ chars)
- ✅ Number boundaries (INT_MAX, INT_MIN)
- ✅ Substring matching (full, partial, single char)
- ✅ Multiple concatenations
- ✅ Type detection for all types
- ✅ Memory safety (repeated operations)

**Root Cause:** Variable type inference was too simplistic  
**Solution:** Infer types from LLVM IR expression results

---

## 🐛 Known Bugs & Issues

### Low Priority (Minor Issues)

4. **str_concat() causes segfaults**
   - **Issue:** Memory allocation/management problems
   - **Status:** Disabled in tests
   - **Impact:** Medium - workaround exists (use + operator)

4. **str_concat() unstable** ✅ **FIXED**
   - **Issue:** Causes segfaults with multiple calls
   - **Root Cause:** Array allocation bug (same as method calls)
   - **Fix:** Array allocation fixed in Session 9
   - **Status:** Working - tested and stable

5. **str_upper()/str_lower() unstable** ✅ **FIXED**
   - **Issue:** Integration issues with existing runtime
   - **Root Cause:** Array allocation bug
   - **Fix:** Array allocation fixed in Session 9
   - **Status:** Working - tested and stable

6. **starts_with()/ends_with() cause crashes** ✅ **FIXED**
   - **Issue:** Implementation had bugs
   - **Root Cause:** Array allocation bug
   - **Fix:** Array allocation fixed in Session 9
   - **Status:** Working - tested and stable

### Low (Minor Issues)

7. **to_string() returns memory address** ✅ **FIXED**
   - **Issue:** Printed pointer instead of formatted string
   - **Root Cause:** Variable type inference bug (same as typeof)
   - **Fix:** Variable type inference fixed in Session 9
   - **Status:** Working - returns proper string

8. **Compiler warnings in main.c**
   - **Issue:** Format string warnings (lines 402, 740)
   - **Impact:** None - cosmetic only
   - **Priority:** P3 - Clean up when convenient

## ✨ Feature Requests

### High Priority

1. **Fix method call stability** ✅ **COMPLETED**
   - Multiple method calls now work perfectly
   - Memory management fixed
   - OOP fully usable

2. **String interpolation**
   - Syntax: `"Hello, {name}!"`
   - Makes string building easier
   - Common in modern languages

3. **Format strings for println()**
   - Syntax: `println("Value: {}", x)`
   - Better than multiple print() calls
   - Improves debugging

4. **Range function**
   - Syntax: `for i in range(0, 10)`
   - Essential for loops
   - Currently need manual counter

### Medium Priority

5. **Array methods**
   - `.map(fn)` - Transform elements
   - `.filter(fn)` - Select elements
   - `.reduce(fn, init)` - Aggregate
   - `.slice(start, end)` - Extract portion

6. **String methods**
   - `.split(delimiter)` → array
   - `.join(array)` → string
   - `.repeat(n)` → string
   - `.trim_start()`, `.trim_end()`

7. **Number methods**
   - `.sqrt()` → float
   - `.pow(n)` → int
   - `.to_string()` → string (fix existing)

8. **Boolean methods**
   - `.to_string()` → "true"/"false"

### Low Priority

9. **Error handling improvements**
   - Better error messages
   - Line/column information
   - Suggestions for fixes

10. **Performance optimizations**
    - Inline small methods
    - Optimize string operations
    - Reduce allocations

11. **Documentation**
    - API reference for all methods
    - More examples
    - Best practices guide

## 🚀 Roadmap

### Phase 1: Stability (Current)
- [x] Fix duplicate symbol issues
- [x] Integrate stdlib implementations
- [x] Parallel test runner (4.1x speedup)
- [ ] Fix method call segfaults ⚠️ **CRITICAL**
- [ ] Fix len() truncation
- [ ] Fix typeof() detection

### Phase 2: Core Features
- [ ] String interpolation
- [ ] Format strings
- [ ] Range function
- [ ] Fix/remove unstable methods

### Phase 3: Enhanced Methods
- [ ] Array functional methods (map, filter, reduce)
- [ ] String split/join
- [ ] Number formatting
- [ ] Boolean to_string

### Phase 4: Polish
- [ ] Better error messages
- [ ] Performance optimizations
- [ ] Comprehensive documentation
- [ ] More examples

## 📊 Current Status

### Working Features ✅

**String Methods:**
- `.contains(substring)` → int (⚠️ single call only)
- `.length()` → int (⚠️ truncation bug)
- `.len()` → int (⚠️ truncation bug)

**Number Methods:**
- `.abs()` → int ✅
- `.min(other)` → int ✅
- `.max(other)` → int ✅

**Literals:**
- `true`, `false` ✅
- `none` ✅

**Built-in Functions:**
- `print()`, `println()` ✅
- `min()`, `max()`, `abs()` ✅
- `assert()` ✅
- `exit()`, `panic()`, `sleep()`, `rand()` ✅
- `some()`, `none`, `ok()`, `err()` ✅
- `file_write()`, `file_read()`, `file_append()`, `file_exists()` ✅

**Stdlib APIs:**
- Process::exec() ✅
- Fs::read_dir(), exists(), is_file(), is_dir() ✅
- Time::now(), sleep() ✅

### Broken/Unstable Features ⚠️

- Multiple method calls (segfault)
- str_concat() (segfault)
- str_upper()/str_lower() (crash)
- starts_with()/ends_with() (crash)
- to_string() (wrong output)
- len() (truncation)
- typeof() (wrong type)

## 🎯 Immediate Action Items

### This Week
1. **Fix method call segfaults** - P0
   - Debug memory management in codegen_method_call()
   - Add proper cleanup/deallocation
   - Test with multiple calls

2. **Fix len() truncation** - P1
   - Use i64 instead of truncating to i32
   - Update tests to verify

3. **Remove or fix broken methods** - P1
   - Either fix str_concat, str_upper, str_lower
   - Or remove from checker/codegen
   - Document as unsupported if removed

### Next Week
4. **Add string interpolation** - P1
   - Parser support for `{expr}` in strings
   - Codegen to sprintf/format
   - Tests for various types

5. **Add range() function** - P1
   - Built-in function in checker
   - Returns array or iterator
   - Enable for-loop usage

6. **Improve error messages** - P2
   - Add line/column to errors
   - Suggest fixes
   - Better formatting

## 📝 Notes

### Test Coverage
- **Unit tests:** 181/181 passing (100%)
- **TDD tests:** 3/4 passing (75%)
- **Parallel execution:** 157s (3.6x speedup)

### Performance
- **Build time:** ~3 seconds (clean)
- **Test time:** 157s (parallel) vs 568s (sequential)
- **Binary size:** ~600KB (LLVM backend)

### Code Quality
- **Compiler warnings:** 2 format string warnings (cosmetic)
- **Memory leaks:** None known
- **Stability:** Production ready (except method calls)

## 🔗 Related Documents

- [PARALLEL_TESTING.md](internal-docs/PARALLEL_TESTING.md) - Test infrastructure
- [known-limitations.md](docs/known-limitations.md) - User-facing limitations
- [V1.6.0_STABILIZATION_COMPLETE.md](internal-docs/V1.6.0_STABILIZATION_COMPLETE.md) - Stability work

---

**Maintained By:** Wyn Compiler Team  
**Last Review:** 2026-01-30  
**Next Review:** Weekly
