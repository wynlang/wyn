// Fixture header for the bindgen test.
#define SAMPLE_MAX 100
#define SAMPLE_NAME "sample"

int add_one(int x);
double half(double x);
const char* label(const char* prefix);
void reset(void);
void* obj_new(int seed);
int obj_value(void* o);
void obj_free(void* o);

// A function declared behind a GNU export-attribute, as real library headers do
// (LZ4LIB_API / ZSTDLIB_API expand to this after `cc -E`). Bindgen must strip the
// leading __attribute__((...)) and still bind the function.
__attribute__((visibility("default"))) int exported_fn(int n);
