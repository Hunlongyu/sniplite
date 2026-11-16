/* Getting the text recognition module: three files into the "ocr" folder next to the
 * program, each from where its makers publish it.
 *
 *   PP-OCRv6_det_tiny.onnx, PP-OCRv6_rec_tiny.onnx   the models, as RapidOCR converted
 *                                                    them, from ModelScope
 *   onnxruntime.dll                                  ONNX Runtime 1.26.0, out of its
 *                                                    Python package on PyPI
 *
 * The package is a zip of 13 MB of which the library is 5.9 MB, so only those bytes
 * are asked for (a range request) and inflated here. Every file is checked against
 * the SHA-256 written below before it gets its name: what is installed is exactly
 * what this version of the program was tested with, whatever the servers send.
 *
 * Runs in the preferences window's process, on a worker thread. */
#include "ocr.h"
#include "update.h"

typedef struct {
    const WCHAR *host, *path, *name;
    uint32_t at, bytes; /* the bytes of the download that are wanted */
    uint32_t size;      /* of the file, installed */
    bool deflated;
    uint8_t sha[32];    /* of the file, installed */
} Part;

#define MODELS L"/models/RapidAI/RapidOCR/resolve/v3.10.0/onnx/PP-OCRv6/"
static const Part kParts[] = {
    {L"www.modelscope.cn", MODELS L"det/PP-OCRv6_det_tiny.onnx", L"PP-OCRv6_det_tiny.onnx", 0, 1829618, 1829618, false,
     {0xf4, 0x2c, 0x0f, 0xbd, 0x29, 0x4d, 0x95, 0xea, 0xc1, 0xa5, 0x50, 0xe1, 0x31, 0xb2, 0x77, 0xda,
      0xc9, 0x74, 0x62, 0xc8, 0x02, 0x5f, 0xa4, 0xb6, 0xc3, 0xce, 0xc1, 0xb7, 0x89, 0x4b, 0xd3, 0xd5}},
    {L"www.modelscope.cn", MODELS L"rec/PP-OCRv6_rec_tiny.onnx", L"PP-OCRv6_rec_tiny.onnx", 0, 4489813, 4489813, false,
     {0xe1, 0x6e, 0x24, 0x2d, 0xe5, 0x93, 0x7a, 0xd9, 0x26, 0x09, 0x22, 0x3f, 0x19, 0xbc, 0x2a, 0xff,
      0x37, 0x27, 0xee, 0x40, 0xb0, 0x95, 0xf9, 0x96, 0x90, 0x7c, 0x24, 0x74, 0x9b, 0xad, 0x25, 0x1b}},
    {L"files.pythonhosted.org",
     L"/packages/44/fc/026d0a7162b9c2153dac292baea9e027c42304dc1d9dc6f8ff5b4cfbaedd/onnxruntime-1.26.0-cp312-cp312-win_amd64.whl",
     L"onnxruntime.dll", 45852, 5896380, 16783672, true,
     {0xef, 0xa1, 0x4d, 0xc0, 0xf3, 0xe8, 0xd6, 0xe4, 0xfc, 0x7c, 0xe0, 0xb9, 0xb5, 0xaa, 0x70, 0xd6,
      0xe2, 0x7d, 0xee, 0xc9, 0x08, 0xaf, 0x05, 0xc1, 0xfa, 0x9a, 0x37, 0x27, 0x0f, 0x57, 0x46, 0x0b}},
};

/* ---- SHA-256 ---- */

static const uint32_t kSha[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static inline uint32_t ror(uint32_t v, int n) { return v >> n | v << (32 - n); }

static void sha_block(uint32_t h[8], const uint8_t *p)
{
    uint32_t w[64], s[8];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; i++)
        w[i] = w[i - 16] + (ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ w[i - 15] >> 3) + w[i - 7] +
               (ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ w[i - 2] >> 10);
    memcpy(s, h, sizeof s);
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = s[7] + (ror(s[4], 6) ^ ror(s[4], 11) ^ ror(s[4], 25)) + ((s[4] & s[5]) ^ (~s[4] & s[6])) + kSha[i] + w[i];
        uint32_t t2 = (ror(s[0], 2) ^ ror(s[0], 13) ^ ror(s[0], 22)) + ((s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]));
        memmove(s + 1, s, 7 * sizeof s[0]);
        s[4] += t1;
        s[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++)
        h[i] += s[i];
}

static void sha256(const uint8_t *p, size_t n, uint8_t out[32])
{
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t tail[128] = {0};
    size_t whole = n & ~(size_t)63, rest = n - whole, pad = rest < 56 ? 64 : 128;
    for (size_t i = 0; i < whole; i += 64)
        sha_block(h, p + i);
    memcpy(tail, p + whole, rest);
    tail[rest] = 0x80;
    for (int i = 0; i < 8; i++)
        tail[pad - 1 - i] = (uint8_t)((uint64_t)n * 8 >> (i * 8));
    for (size_t i = 0; i < pad; i += 64)
        sha_block(h, tail + i);
    for (int i = 0; i < 32; i++)
        out[i] = (uint8_t)(h[i / 4] >> (24 - i % 4 * 8));
}

/* ---- inflate (RFC 1951): the whole input to the whole output ---- */

typedef struct {
    const uint8_t *in;
    size_t inlen, inpos;
    uint64_t bits;
    int nbits;
    uint8_t *out;
    size_t outlen, outpos;
} Inf;

#define FAST 10 /* codes up to this long are read with one look into a table */

typedef struct {
    uint16_t count[16], symbol[288];
    uint16_t fast[1 << FAST]; /* by the next FAST bits: symbol << 4 | the code's length; 0 for a longer code */
} Huff;

static uint32_t take(Inf *s, int n)
{
    while (s->nbits < n && s->inpos < s->inlen) {
        s->bits |= (uint64_t)s->in[s->inpos++] << s->nbits;
        s->nbits += 8;
    }
    uint32_t v = (uint32_t)s->bits & ((1u << n) - 1);
    s->bits >>= n;
    s->nbits -= n; /* below zero past the end: caught by the callers' bounds */
    return v;
}

static void huff_build(Huff *h, const uint8_t *len, int n)
{
    uint16_t offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++)
        h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int l = 1; l < 15; l++)
        offs[l + 1] = (uint16_t)(offs[l] + h->count[l]);
    for (int i = 0; i < n; i++)
        if (len[i])
            h->symbol[offs[len[i]]++] = (uint16_t)i;
    /* the table: every short code, written as its bits arrive (last bit first), with all that can follow it */
    uint16_t next[16];
    unsigned code = 0;
    for (int l = 1; l <= 15; l++) {
        code = (code + h->count[l - 1]) << 1;
        next[l] = (uint16_t)code;
    }
    memset(h->fast, 0, sizeof h->fast);
    for (int i = 0; i < n; i++) {
        int l = len[i];
        if (!l)
            continue;
        unsigned c = next[l]++, r = 0;
        if (l > FAST)
            continue;
        for (int b = 0; b < l; b++)
            r |= (c >> b & 1) << (l - 1 - b);
        for (unsigned j = r; j < 1u << FAST; j += 1u << l)
            h->fast[j] = (uint16_t)(i << 4 | l);
    }
}

static int huff_read(Inf *s, const Huff *h)
{
    while (s->nbits < 15 && s->inpos < s->inlen) {
        s->bits |= (uint64_t)s->in[s->inpos++] << s->nbits;
        s->nbits += 8;
    }
    unsigned e = h->fast[s->bits & ((1u << FAST) - 1)];
    if (e) {
        s->bits >>= e & 15;
        s->nbits -= (int)(e & 15);
        return (int)(e >> 4);
    }
    int code = 0, first = 0, index = 0; /* a longer code: bit by bit */
    for (int len = 1; len <= 15; len++) {
        code |= (int)take(s, 1);
        int count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static bool inflate_codes(Inf *s, const Huff *lit, const Huff *dist)
{
    static const uint16_t lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = huff_read(s, lit);
        if (sym < 0 || s->nbits < 0)
            return false;
        if (sym < 256) {
            if (s->outpos >= s->outlen)
                return false;
            s->out[s->outpos++] = (uint8_t)sym;
        } else if (sym == 256) {
            return true;
        } else {
            if ((sym -= 257) >= 29)
                return false;
            size_t len = lbase[sym] + take(s, lext[sym]);
            int d = huff_read(s, dist);
            if (d < 0 || d >= 30)
                return false;
            size_t back = dbase[d] + take(s, dext[d]);
            if (back > s->outpos || len > s->outlen - s->outpos)
                return false;
            uint8_t *to = s->out + s->outpos;
            for (size_t i = 0; i < len; i++) /* may overlap itself: byte by byte */
                to[i] = to[i - back];
            s->outpos += len;
        }
    }
}

static bool inflate(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    Inf s = {in, inlen, 0, 0, 0, out, outlen, 0};
    Huff *lit = mem_alloc(2 * sizeof(Huff)), *dist = lit + 1;
    bool ok = lit != NULL;
    for (uint32_t last = 0; ok && !last;) {
        last = take(&s, 1);
        uint32_t type = take(&s, 2);
        uint8_t len[320];
        if (type == 0) { /* stored: from the next whole byte, giving back the bytes read ahead */
            if (s.nbits > 0)
                s.inpos -= (size_t)(s.nbits / 8);
            s.bits = 0, s.nbits = 0;
            if (s.inpos + 4 > s.inlen)
                ok = false;
            else {
                size_t n = s.in[s.inpos] | (size_t)s.in[s.inpos + 1] << 8;
                s.inpos += 4;
                ok = n <= s.inlen - s.inpos && n <= s.outlen - s.outpos;
                if (ok) {
                    memcpy(s.out + s.outpos, s.in + s.inpos, n);
                    s.inpos += n, s.outpos += n;
                }
            }
        } else if (type == 1) { /* the fixed codes */
            int i = 0;
            for (; i < 144; i++) len[i] = 8;
            for (; i < 256; i++) len[i] = 9;
            for (; i < 280; i++) len[i] = 7;
            for (; i < 288; i++) len[i] = 8;
            huff_build(lit, len, 288);
            memset(len, 5, 30);
            huff_build(dist, len, 30);
            ok = inflate_codes(&s, lit, dist);
        } else if (type == 2) { /* codes sent with the block */
            int nlen = (int)take(&s, 5) + 257, ndist = (int)take(&s, 5) + 1, ncode = (int)take(&s, 4) + 4, i = 0;
            memset(len, 0, sizeof len);
            for (; i < ncode; i++)
                len[order[i]] = (uint8_t)take(&s, 3);
            huff_build(lit, len, 19);
            for (i = 0; ok && i < nlen + ndist;) {
                int sym = huff_read(&s, lit), rep = 1, v = sym;
                if (sym < 0 || s.nbits < 0)
                    ok = false;
                else if (sym == 16) {
                    ok = i > 0;
                    v = ok ? len[i - 1] : 0;
                    rep = 3 + (int)take(&s, 2);
                } else if (sym == 17) {
                    v = 0, rep = 3 + (int)take(&s, 3);
                } else if (sym == 18) {
                    v = 0, rep = 11 + (int)take(&s, 7);
                }
                if (i + rep > nlen + ndist)
                    ok = false;
                for (; ok && rep > 0; rep--)
                    len[i++] = (uint8_t)v;
            }
            if (ok) {
                uint8_t dl[32];
                memcpy(dl, len + nlen, (size_t)ndist);
                huff_build(lit, len, nlen);
                huff_build(dist, dl, ndist);
                ok = inflate_codes(&s, lit, dist);
            }
        } else {
            ok = false;
        }
    }
    mem_free(lit);
    return ok && s.outpos == outlen;
}

/* ---- the files ---- */

static bool part_path(const WCHAR *name, const WCHAR *suffix, WCHAR *path, int cap)
{
    WCHAR dir[MAX_PATH * 2];
    ocr_dir(dir, COUNTOF(dir));
    if (lstrlenW(dir) + lstrlenW(name) + lstrlenW(suffix) + 2 > cap)
        return false;
    wsprintfW(path, L"%s\\%s%s", dir, name, suffix);
    return true;
}

typedef struct {
    uint8_t *buf;
    uint32_t have, want;
    uint32_t at;          /* where the wanted bytes begin in the whole file */
    DWORD status;         /* 206: the server sent just them; 200: the file from its start */
    bool begun;
    uint32_t done, total;      /* of all the parts, for the progress */
    volatile LONG *cancel;
    void (*progress)(int, void *);
    void *ctx;
    int shown;
} Fetch;

static bool fetch_sink(const void *data, DWORD n, void *ctx)
{
    Fetch *f = ctx;
    const uint8_t *p = data;
    if (*f->cancel)
        return false;
    if (!f->begun) {
        f->begun = true;
        if (f->status == 206)
            f->at = 0;
    }
    if (f->at) { /* the server ignored the range: pass over what comes before */
        DWORD skip = MINI(f->at, n);
        f->at -= skip, p += skip, n -= skip;
    }
    DWORD k = MINI(n, f->want - f->have);
    memcpy(f->buf + f->have, p, k);
    f->have += k;
    int pct = (int)((uint64_t)(f->done + f->have) * 100 / f->total);
    if (pct != f->shown && f->progress)
        f->progress(f->shown = pct, f->ctx);
    return f->have < f->want; /* enough: the rest of a whole file is not wanted */
}

static bool same_file(const Part *p)
{
    WCHAR path[MAX_PATH * 2];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    return part_path(p->name, L"", path, COUNTOF(path)) && GetFileAttributesExW(path, GetFileExInfoStandard, &fa) &&
           !fa.nFileSizeHigh && fa.nFileSizeLow == p->size;
}

int ocr_get(volatile LONG *cancel, void (*progress)(int percent, void *ctx), void *ctx)
{
    WCHAR dir[MAX_PATH * 2], path[MAX_PATH * 2], tmp[MAX_PATH * 2];
    ocr_dir(dir, COUNTOF(dir));
    if (!CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return OCR_GET_WRITE;
    Fetch f = {0};
    f.cancel = cancel, f.progress = progress, f.ctx = ctx, f.shown = -1;
    for (int i = 0; i < COUNTOF(kParts); i++)
        f.total += kParts[i].bytes;
    int result = OCR_GET_OK;
    for (int i = 0; i < COUNTOF(kParts) && result == OCR_GET_OK; i++) {
        const Part *p = &kParts[i];
        if (same_file(p)) { /* there from an earlier, interrupted time */
            f.done += p->bytes;
            continue;
        }
        if (!part_path(p->name, L"", path, COUNTOF(path)) || !part_path(p->name, L".part", tmp, COUNTOF(tmp)))
            return OCR_GET_WRITE;
        uint8_t *got = mem_alloc(p->bytes), *file = got, sha[32];
        WCHAR range[64] = L"";
        f.buf = got, f.have = 0, f.want = p->bytes, f.at = p->at, f.begun = false;
        if (!got)
            return OCR_GET_WRITE;
        if (p->at)
            wsprintfW(range, L"Range: bytes=%u-%u\r\n", p->at, p->at + p->bytes - 1);
        http_fetch(p->host, p->path, range[0] ? range : NULL, &f.status, fetch_sink, &f);
        if (*cancel)
            result = OCR_GET_CANCEL;
        else if (f.have != f.want)
            result = OCR_GET_NET;
        if (result == OCR_GET_OK && p->deflated) {
            file = mem_alloc(p->size);
            if (!file || !inflate(got, p->bytes, file, p->size))
                result = OCR_GET_CHECK;
        }
        if (result == OCR_GET_OK) {
            sha256(file, p->size, sha);
            if (memcmp(sha, p->sha, 32))
                result = OCR_GET_CHECK;
        }
        if (result == OCR_GET_OK) {
            HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            DWORD wrote = 0;
            bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, file, p->size, &wrote, NULL) && wrote == p->size;
            if (h != INVALID_HANDLE_VALUE)
                CloseHandle(h);
            if (!ok || !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
                DeleteFileW(tmp);
                result = OCR_GET_WRITE;
            }
        }
        if (file != got)
            mem_free(file);
        mem_free(got);
        f.done += p->bytes;
    }
    return result;
}

/* ---- the module carried by the program itself ----
 *
 * sniplite_<version>_portable_with_OCR_x64.exe is sniplite.exe with a zip of the ocr folder
 * put after it (pack.bat: copy /b), so the module travels in the one file. A zip is read from
 * its end, where its directory is, and the directory says where each file begins, counted
 * from the zip's own start: here that is the end of the program. The first time text is to be
 * read, the folder is written beside the program, every file of the module checked against
 * its SHA-256 as a downloaded one would be. A sniplite.exe without the zip is not slowed by
 * any of this: the last 22 bytes of the file are looked at once. */

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

typedef struct {
    HANDLE f;
    uint64_t dir, shift; /* where the zip's directory is in the file; what to add to the places it names */
    uint32_t dir_bytes, count;
} Pack;

static bool read_at(HANDLE f, uint64_t at, void *buf, uint32_t n)
{
    LARGE_INTEGER li;
    DWORD got;
    li.QuadPart = (LONGLONG)at;
    return SetFilePointerEx(f, li, NULL, FILE_BEGIN) && ReadFile(f, buf, n, &got, NULL) && got == n;
}

static bool pack_open(Pack *k)
{
    WCHAR exe[MAX_PATH * 2];
    LARGE_INTEGER size;
    uint8_t end[22]; /* a zip's last record, when it has no comment */
    k->f = GetModuleFileNameW(NULL, exe, COUNTOF(exe))
               ? CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL)
               : INVALID_HANDLE_VALUE;
    if (k->f == INVALID_HANDLE_VALUE)
        return false;
    if (GetFileSizeEx(k->f, &size) && size.QuadPart > 22 && read_at(k->f, (uint64_t)size.QuadPart - 22, end, 22) &&
        rd32(end) == 0x06054b50) {
        k->count = rd16(end + 10);
        k->dir_bytes = rd32(end + 12);
        k->dir = (uint64_t)size.QuadPart - 22 - k->dir_bytes;
        k->shift = k->dir - rd32(end + 16);
        if (k->dir_bytes < (uint64_t)size.QuadPart - 22 && rd32(end + 16) <= k->dir)
            return true;
    }
    CloseHandle(k->f);
    return false;
}

bool ocr_packed(void)
{
    static int known; /* 1 no, 2 yes: the program's file does not change while it runs */
    if (!known) {
        Pack k;
        known = pack_open(&k) ? 2 : 1;
        if (known == 2)
            CloseHandle(k.f);
    }
    return known == 2;
}

/* one file of the zip, beside the program: checked if it is one of the module's, written, given its name */
static bool pack_write(const WCHAR *name, const uint8_t *data, uint32_t n)
{
    WCHAR path[MAX_PATH * 2], tmp[MAX_PATH * 2];
    uint8_t sha[32];
    for (int i = 0; i < COUNTOF(kParts); i++)
        if (!lstrcmpiW(name, kParts[i].name)) {
            sha256(data, n, sha);
            if (n != kParts[i].size || memcmp(sha, kParts[i].sha, 32))
                return false;
        }
    if (!part_path(name, L"", path, COUNTOF(path)) || !part_path(name, L".part", tmp, COUNTOF(tmp)))
        return false;
    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wrote = 0;
    bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, data, n, &wrote, NULL) && wrote == n;
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    if (!ok || !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp);
        return false;
    }
    return true;
}

bool ocr_unpack(void)
{
    Pack k;
    WCHAR dir[MAX_PATH * 2];
    if (!pack_open(&k))
        return false;
    ocr_dir(dir, COUNTOF(dir));
    uint8_t *cd = mem_alloc(k.dir_bytes ? k.dir_bytes : 1);
    bool ok = cd && read_at(k.f, k.dir, cd, k.dir_bytes) && (CreateDirectoryW(dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    const uint8_t *e = cd, *end = cd + k.dir_bytes;
    for (uint32_t i = 0; ok && i < k.count; i++) {
        if (e + 46 > end || rd32(e) != 0x02014b50) {
            ok = false;
            break;
        }
        uint32_t method = rd16(e + 10), csize = rd32(e + 20), usize = rd32(e + 24), nlen = rd16(e + 28), at = rd32(e + 42);
        const char *name = (const char *)e + 46;
        e += 46 + nlen + rd16(e + 30) + rd16(e + 32);
        if (e > end) {
            ok = false;
            break;
        }
        /* only plain files right in "ocr/": nothing that could be written anywhere else */
        WCHAR file[64];
        bool plain = nlen > 4 && nlen < 4 + COUNTOF(file) && !memcmp(name, "ocr/", 4) && name[4] != '.';
        for (uint32_t c = 4; plain && c < nlen; c++) {
            char ch = name[c];
            plain = ch > 32 && ch < 127 && ch != '/' && ch != '\\' && ch != ':';
            file[c - 4] = (WCHAR)ch;
        }
        if (!plain)
            continue; /* the folder's own entry, or something not expected here */
        file[nlen - 4] = 0;
        uint8_t head[30], *in = mem_alloc(csize ? csize : 1), *out = method ? mem_alloc(usize ? usize : 1) : in;
        ok = in && out && read_at(k.f, k.shift + at, head, 30) && rd32(head) == 0x04034b50 &&
             read_at(k.f, k.shift + at + 30 + rd16(head + 26) + rd16(head + 28), in, csize) &&
             (method == 0 ? csize == usize : method == 8 && inflate(in, csize, out, usize)) && pack_write(file, out, usize);
        if (out != in)
            mem_free(out);
        mem_free(in);
    }
    mem_free(cd);
    CloseHandle(k.f);
    return ok && ocr_installed();
}

bool ocr_remove(void)
{
    WCHAR dir[MAX_PATH * 2], path[MAX_PATH * 2];
    ocr_dir(dir, COUNTOF(dir));
    bool ok = true;
    for (int i = 0; i < COUNTOF(kParts); i++) {
        if (part_path(kParts[i].name, L"", path, COUNTOF(path)) && !DeleteFileW(path) && GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND) /* neither the file nor the folder there: nothing to remove */
            ok = false;
        if (part_path(kParts[i].name, L".part", path, COUNTOF(path)))
            DeleteFileW(path);
    }
    RemoveDirectoryW(dir); /* stays when something else is in it */
    return ok;
}

int ocr_size_mb(void)
{
    uint64_t sum = 0;
    for (int i = 0; i < COUNTOF(kParts); i++)
        sum += kParts[i].size;
    return (int)((sum + 512 * 1024) >> 20);
}
