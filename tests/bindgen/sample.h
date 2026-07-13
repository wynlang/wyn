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
