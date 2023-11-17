#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* PRIMITIVES */

typedef int32_t   b32;
typedef int8_t    byte;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef float     f32;
typedef double    f64;
typedef uintptr_t uptr;
typedef ptrdiff_t size;
typedef size_t    usize;

/* MACROS (from nullprogram.com) */

#define sizeof(x) (usize)sizeof(x)
#define alignof(x) (usize)_Alignof(x)
#define countof(a) (sizeof(a) / sizeof(*(a)))
#define lengthof(s) (countof(s) - 1)

// FIXME
#ifndef _WIN32
#define panic(message) \
    do { \
        fprintf(stderr, "Panic in %s (%s:%d): %s\n", __FUNCTION__, __FILE__, __LINE__, message); \
        exit(1); \
    } while (0)
#endif

/* WIN32 */

#ifdef _WIN32
#define W32(r) __declspec(dllimport) r __stdcall
W32(void)   ExitProcess(u32);
W32(i32)    GetStdHandle(u32);
W32(byte *) VirtualAlloc(byte *, usize, u32, u32);
W32(b32)    WriteConsoleA(uptr, u8 *, u32, u32 *, void *);

#define panic(message) \
    do { \
        fprintf(stderr, "Panic in %s (%s:%d): %s\n", __FUNCTION__, __FILE__, __LINE__, message); \
        ExitProcess(1); \
    } while (0)

#define STD_OUTPUT_HANDLE ((DWORN)-11)
typedef int BOOL;
typedef void *HANDLE;
typedef u32 DWORD;

typedef struct _SYSTEM_INFO {
  union {
    u32 dwOemId;
    struct {
      u16 wProcessorArchitecture;
      u16 wReserved;
    } DUMMYSTRUCTNAME;
  } DUMMYUNIONNAME;
  u32     dwPageSize;
  void    *lpMinimumApplicationAddress;
  void    *lpMaximumApplicationAddress;
  u32 *dwActiveProcessorMask;
  u32     dwNumberOfProcessors;
  u32     dwProcessorType;
  u32     dwAllocationGranularity;
  u16      wProcessorLevel;
  u16      wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef union _LARGE_INTEGER {
  struct {
    u32 LowPart;
    i32  HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    u32 LowPart;
    i32  HighPart;
  } u;
  i64 QuadPart;
} LARGE_INTEGER;

#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RESET 0x800000
#define MEM_RESET_UNDO 0x1000000

#define PAGE_READONLY 0x2
#define PAGE_READWRITE 0x4
#endif

/* USEFUL STUFF */

#define COLOR_WHITE (v4){1, 1, 1, 1}

/* LINEAR ALGEBRA & MATH */

typedef union v2 {
    struct {
        f32 x, y;
    };
    struct {
        f32 width, height;
    };
    f32 data[2];
} v2;

typedef union v3 {
    struct {
        f32 x, y, z;
    };
    struct {
        f32 width, height, depth;
    };
    f32 data[3];
} v3;

typedef union v4 {
    struct {
        f32 x, y, z, w;
    };
    struct {
        f32 r, g, b, a;
    };
    f32 data[4];
} v4;

typedef union m4 {
    f32 data[4][4];
} m4;

/* MEMORY */

typedef struct arena {
    byte *base;
    usize size;
    usize committed;
    usize offset;
} arena;

typedef struct bump {
    byte *buf;
    usize size;
    usize committed;
    usize offset;
} bump;

#define KB 1024
#define MB KB * 1024
#define GB MB * 1024

void mem_copy(void *dest, void *src, int size);
void mem_zero(void *mem, int size);

/* STRING */

#define s8(s) (s8){(u8 *)s, lengthof(s)}

typedef struct s8 {
    u8 *data;
    usize len;
} s8;

typedef struct rope_node rope_node;
struct rope_node {
    rope_node *left;
    rope_node *right;
    s8 str;
};

s8 string_case_insensitive_search(s8 s, s8 q);

/* OS */

typedef struct file_read_result {
    s8 str;
    b32 ok;
} file_read_result;

file_read_result os_file_read(const char *path, arena *a, arena *scratch);

/* MATH */

void m4_ortho(m4 *m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f);

/* GRAPHICS */

#define RENDER_WIDTH 1920
#define RENDER_HEIGHT 1080

typedef void * window_handle;

typedef struct batch_vertex {
    v3 pos;
    v4 color;
    v2 uvs;
    i32 slot;
} batch_vertex;

typedef struct texture {
    u32 id;
    f32 width;
    f32 height;
    f32 frame_width;
    f32 frame_height;
} texture;

#endif
