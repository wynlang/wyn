// Minimal, ABI-accurate OpenCL 1.2 header subset for Wyn's OpenCL backend.
//
// WHY THIS EXISTS: src/gpu_opencl.c loads libOpenCL at RUN time via dlopen and
// never links the loader, so at BUILD time it needs only the OpenCL types and
// enum constants - not the full Khronos SDK. Vendoring this minimal subset lets
// the OpenCL backend compile everywhere Wyn builds it, including:
//   - `wyn cross linux` / `wyn cross windows` FROM the macOS dev box (which has
//     no system OpenCL headers), and
//   - native linux builds without the opencl-headers package installed.
//
// ABI SAFETY: OpenCL's ABI is fixed by the Khronos spec (cl_int is 32-bit,
// cl_ulong 64-bit, handles are opaque pointers, the enum values below are the
// standard ones). Because we only dlsym the entry points, matching these
// spec-mandated sizes/values is sufficient for correct calls into the real
// libOpenCL at run time. If a full <CL/cl.h> is preferred on a given build
// host, drop -Ivendor/opencl and it will use the system header instead - the
// declarations here are a compatible subset.

#ifndef WYN_VENDOR_CL_CL_H
#define WYN_VENDOR_CL_CL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t   cl_int;
typedef uint32_t  cl_uint;
typedef int64_t   cl_long;
typedef uint64_t  cl_ulong;

typedef cl_uint   cl_bool;
typedef cl_ulong  cl_device_type;
typedef cl_ulong  cl_mem_flags;
typedef cl_ulong  cl_command_queue_properties;
typedef intptr_t  cl_context_properties;
typedef cl_uint   cl_program_build_info;

// Opaque handle types are pointers to incomplete structs (spec ABI).
typedef struct _cl_platform_id*   cl_platform_id;
typedef struct _cl_device_id*     cl_device_id;
typedef struct _cl_context*       cl_context;
typedef struct _cl_command_queue* cl_command_queue;
typedef struct _cl_program*       cl_program;
typedef struct _cl_kernel*        cl_kernel;
typedef struct _cl_mem*           cl_mem;
typedef struct _cl_event*         cl_event;

// Standard enum/flag values (Khronos OpenCL spec).
#define CL_SUCCESS                 0
#define CL_FALSE                   0
#define CL_TRUE                    1

#define CL_DEVICE_TYPE_DEFAULT     (1 << 0)
#define CL_DEVICE_TYPE_CPU         (1 << 1)
#define CL_DEVICE_TYPE_GPU         (1 << 2)
#define CL_DEVICE_TYPE_ALL         0xFFFFFFFF

#define CL_MEM_READ_WRITE          (1 << 0)
#define CL_MEM_WRITE_ONLY          (1 << 1)
#define CL_MEM_READ_ONLY           (1 << 2)

#define CL_PROGRAM_BUILD_LOG       0x1183

#ifdef __cplusplus
}
#endif

#endif // WYN_VENDOR_CL_CL_H
