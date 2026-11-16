#define MS_ABI __attribute__((ms_abi))
typedef unsigned long long size_t;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

MS_ABI void SetLastError(unsigned int err);
MS_ABI void* GetProcessHeap(void);
MS_ABI void* HeapAlloc(void* hHeap, unsigned int dwFlags, size_t dwBytes);
MS_ABI int HeapFree(void* hHeap, unsigned int dwFlags, void* lpMem);

#ifdef ENABLE_PROXY_LOG
MS_ABI void* CreateFileA(const char* lpFileName, unsigned int dwDesiredAccess, unsigned int dwShareMode, void* lpSecurityAttributes, unsigned int dwCreationDisposition, unsigned int dwFlagsAndAttributes, void* hTemplateFile);
MS_ABI int WriteFile(void* hFile, const void* lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int* lpNumberOfBytesWritten, void* lpOverlapped);

static void* g_log_file = (void*)(size_t)-1;

static void log_raw(const char* s, int len) {
    if (g_log_file == (void*)(size_t)-1) {
        g_log_file = CreateFileA("cabinet_proxy.log", 0x40000000, 1 | 2, 0, 4, 0x80000080, 0);
    }
    if (g_log_file && g_log_file != (void*)(size_t)-1) {
        unsigned int written = 0;
        WriteFile(g_log_file, s, len, &written, 0);
    }
}

static void log_str(const char* s) {
    int len = 0;
    while (s[len]) len++;
    log_raw(s, len);
}

static void log_hex(const char* prefix, unsigned long long val) {
    char buf[96];
    int p = 0;
    while (prefix[p] && p < 64) { buf[p] = prefix[p]; p++; }
    buf[p++] = '0'; buf[p++] = 'x';
    for (int i = 15; i >= 0; i--) {
        int nib = (val >> (i * 4)) & 0xf;
        buf[p++] = nib < 10 ? ('0' + nib) : ('a' + nib - 10);
    }
    buf[p++] = '\n';
    log_raw(buf, p);
}
#else
#define log_str(s) ((void)0)
#define log_hex(p, v) ((void)0)
#endif

void* memcpy(void* dst, const void* src, size_t n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* memset(void* dst, int val, size_t n) {
    u8* d = (u8*)dst;
    while (n--) *d++ = (u8)val;
    return dst;
}

/* Microsoft XPRESS Huffman declarations (from pyxca_xpress.c) */
u32 DecompressBufferProgress(
    u8 *UncompressedBuffer,
    u32 UncompressedBufferSize,
    u8 *CompressedBuffer,
    u32 CompressedBufferSize,
    u32 *FinalUncompressedSize,
    void *Workspace,
    void *Callback,
    void *CallbackContext,
    u32 ProgressBytes);

u32 CompressWorkSpaceSizeXpressHuff(
    u32 *CompressBufferWorkSpaceSize,
    u32 *DecompressBufferWorkSpaceSize);

struct MyDecompressor {
    void* ms_workspace;
    u32 ws_size;
    int algo;
};

static u32 g_ws_size = 0;

static void init_ws_size(void) {
    if (!g_ws_size) {
        u32 c_size = 0;
        CompressWorkSpaceSizeXpressHuff(&c_size, &g_ws_size);
        if (g_ws_size < 16384) g_ws_size = 16384;
    }
}

MS_ABI int CreateDecompressor(unsigned int Algorithm, void* AllocationRoutines, void** DecompressorHandle) {
    log_hex("[cab] CreateDecompressor algo=", Algorithm);
    if (!DecompressorHandle) {
        SetLastError(87); /* ERROR_INVALID_PARAMETER */
        return 0;
    }
    *DecompressorHandle = 0;
    init_ws_size();

    struct MyDecompressor* my = (struct MyDecompressor*)HeapAlloc(GetProcessHeap(), 8, sizeof(struct MyDecompressor));
    if (!my) {
        SetLastError(8); /* ERROR_NOT_ENOUGH_MEMORY */
        return 0;
    }
    my->algo = Algorithm & 0xff;
    my->ws_size = g_ws_size;
    my->ms_workspace = HeapAlloc(GetProcessHeap(), 8, g_ws_size + 64);
    if (!my->ms_workspace) {
        HeapFree(GetProcessHeap(), 0, my);
        SetLastError(8);
        return 0;
    }

    *DecompressorHandle = (void*)my;
    return 1;
}

MS_ABI int Decompress(void* DecompressorHandle, const void* CompressedData, size_t CompressedDataSize, void* UncompressedBuffer, size_t UncompressedBufferSize, size_t* UncompressedDataSize) {
    if (!DecompressorHandle || !CompressedData || !UncompressedBuffer) {
        log_str("[cab] Decompress invalid param\n");
        SetLastError(87);
        return 0;
    }
    struct MyDecompressor* my = (struct MyDecompressor*)DecompressorHandle;

    const u8* src = (const u8*)CompressedData;
    size_t src_len = CompressedDataSize;

    /* Skip Windows Compression API 28-byte buffer header (0a 51 e5 c0 ...) */
    if (src_len >= 28 && src[0] == 0x0a && src[1] == 0x51 && src[2] == 0xe5 && src[3] == 0xc0) {
        u32 hdr_len = 4 + (u32)src[4] + ((u32)src[5] << 8);
        if (hdr_len <= src_len) {
            src += hdr_len;
            src_len -= hdr_len;
        }
    }

    u32 final_size = 0;
    u32 status = DecompressBufferProgress(
        (u8*)UncompressedBuffer,
        (u32)UncompressedBufferSize,
        (u8*)src,
        (u32)src_len,
        &final_size,
        my->ms_workspace,
        0, 0, 0
    );

    if (status != 0 && src != (const u8*)CompressedData) {
        status = DecompressBufferProgress(
            (u8*)UncompressedBuffer,
            (u32)UncompressedBufferSize,
            (u8*)CompressedData,
            (u32)CompressedDataSize,
            &final_size,
            my->ms_workspace,
            0, 0, 0
        );
    }

    if (status != 0) {
        log_hex("[cab] Decompress failed status=", status);
        SetLastError(615); /* ERROR_BAD_COMPRESSION_BUFFER */
        return 0;
    }

    if (UncompressedDataSize) {
        *UncompressedDataSize = (size_t)final_size;
    }
    return 1;
}

MS_ABI int CloseDecompressor(void* DecompressorHandle) {
    if (!DecompressorHandle) {
        SetLastError(6); /* ERROR_INVALID_HANDLE */
        return 0;
    }
    struct MyDecompressor* my = (struct MyDecompressor*)DecompressorHandle;
    if (my->ms_workspace) {
        HeapFree(GetProcessHeap(), 0, my->ms_workspace);
        my->ms_workspace = 0;
    }
    HeapFree(GetProcessHeap(), 0, my);
    return 1;
}

MS_ABI int ResetDecompressor(void* DecompressorHandle) {
    return 1;
}

MS_ABI int DllMain(void* hinstDLL, unsigned int fdwReason, void* lpvReserved) {
    if (fdwReason == 1) {
        log_str("[cab] DllMain ATTACH\n");
    }
    return 1;
}
