/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

/* Windows clipboard image (PNG wire) — GDI+ encode/decode; linked into nv-platform-win. */

#ifdef _WIN32

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>

#include <cstring>
#include <vector>

#include "nv_base64.h"
#include "nv_util.h"

extern "C" {

#if defined(__GNUC__) || defined(__clang__)
#define NV_WIN_CLIPIMG_ATTR __attribute__((weak))
#else
#define NV_WIN_CLIPIMG_ATTR
#endif

static INIT_ONCE g_gdip_once = INIT_ONCE_STATIC_INIT;
static ULONG_PTR g_gdip_token;

static BOOL CALLBACK nv_win_gdip_init(PINIT_ONCE, PVOID, PVOID *) {
  Gdiplus::GdiplusStartupInput si;
  return Gdiplus::GdiplusStartup(&g_gdip_token, &si, nullptr) == Gdiplus::Ok;
}

static void nv_win_gdip_ensure(void) {
  (void)InitOnceExecuteOnce(&g_gdip_once, nv_win_gdip_init, nullptr, nullptr);
}

static int nv_win_get_png_encoder_clsid(CLSID *cls) {
  UINT num = 0, sz = 0;
  if (Gdiplus::GetImageEncodersSize(&num, &sz) != Gdiplus::Ok || sz == 0) return -1;
  std::vector<BYTE> buf(sz);
  auto *info = reinterpret_cast<Gdiplus::ImageCodecInfo *>(buf.data());
  if (Gdiplus::GetImageEncoders(num, sz, info) != Gdiplus::Ok) return -1;
  for (UINT j = 0; j < num; j++) {
    if (wcscmp(info[j].MimeType, L"image/png") == 0) {
      *cls = info[j].Clsid;
      return 0;
    }
  }
  return -1;
}

static int nv_win_png_ihdr_dims(const uint8_t *p, size_t n, int *w, int *h) {
  static const uint8_t sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  if (!w || !h || !p) return -1;
  *w = *h = 0;
  if (n < 24u) return -1;
  if (memcmp(p, sig, 8u) != 0) return -1;
  if (memcmp(p + 12u, "IHDR", 4u) != 0) return -1;
  *w = (int)(((unsigned)p[16] << 24) | ((unsigned)p[17] << 16) | ((unsigned)p[18] << 8) |
             (unsigned)p[19]);
  *h = (int)(((unsigned)p[20] << 24) | ((unsigned)p[21] << 16) | ((unsigned)p[22] << 8) |
             (unsigned)p[23]);
  if (*w <= 0 || *h <= 0) return -1;
  return 0;
}

static int nv_win_clip_read_png_handle(HANDLE hg, std::vector<uint8_t> *png, int *w, int *h) {
  SIZE_T sz = GlobalSize(hg);
  if (sz == 0 || sz > 256u * 1024u * 1024u) return -1;
  const uint8_t *p = (const uint8_t *)GlobalLock(hg);
  if (!p) return -1;
  png->assign(p, p + sz);
  GlobalUnlock(hg);
  return nv_win_png_ihdr_dims(png->data(), png->size(), w, h);
}

static int nv_win_dib_to_png(HANDLE hg, std::vector<uint8_t> *pngOut, int *w, int *h) {
  const BYTE *dib = (const BYTE *)GlobalLock(hg);
  if (!dib) return -1;
  SIZE_T total = GlobalSize(hg);
  if (total < sizeof(BITMAPINFOHEADER)) {
    GlobalUnlock(hg);
    return -1;
  }
  const BITMAPINFOHEADER *hdr = (const BITMAPINFOHEADER *)dib;
  DWORD hs = hdr->biSize;
  if (hs < sizeof(BITMAPINFOHEADER) || (SIZE_T)hs > total) {
    GlobalUnlock(hg);
    return -1;
  }
  DWORD extra = 0;
  if (hdr->biCompression == BI_BITFIELDS && (hdr->biBitCount == 16 || hdr->biBitCount == 32))
    extra = 12;
  else if (hdr->biCompression == BI_RGB && hdr->biBitCount <= 8) {
    DWORD nc = hdr->biClrUsed ? hdr->biClrUsed : (1u << hdr->biBitCount);
    extra = nc * (DWORD)sizeof(RGBQUAD);
  }
  DWORD offPixels = hs + extra;
  if (offPixels > (DWORD)total) {
    GlobalUnlock(hg);
    return -1;
  }

  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + offPixels;
  bfh.bfSize = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)total;

  IStream *pIn = nullptr;
  if (CreateStreamOnHGlobal(nullptr, TRUE, &pIn) != S_OK) {
    GlobalUnlock(hg);
    return -1;
  }
  ULONG wr = 0;
  if (pIn->Write(&bfh, sizeof(bfh), &wr) != S_OK || wr != sizeof(bfh)) {
    pIn->Release();
    GlobalUnlock(hg);
    return -1;
  }
  if (pIn->Write(dib, (ULONG)total, &wr) != S_OK || wr != (ULONG)total) {
    pIn->Release();
    GlobalUnlock(hg);
    return -1;
  }
  LARGE_INTEGER z{};
  if (pIn->Seek(z, STREAM_SEEK_SET, nullptr) != S_OK) {
    pIn->Release();
    GlobalUnlock(hg);
    return -1;
  }
  GlobalUnlock(hg);

  nv_win_gdip_ensure();
  Gdiplus::Bitmap bmp(pIn);
  pIn->Release();
  if (bmp.GetLastStatus() != Gdiplus::Ok) return -1;
  *w = (int)bmp.GetWidth();
  *h = (int)bmp.GetHeight();

  IStream *pOut = nullptr;
  if (CreateStreamOnHGlobal(nullptr, TRUE, &pOut) != S_OK) return -1;
  CLSID enc{};
  if (nv_win_get_png_encoder_clsid(&enc) != 0) {
    pOut->Release();
    return -1;
  }
  if (bmp.Save(pOut, &enc, nullptr) != Gdiplus::Ok) {
    pOut->Release();
    return -1;
  }
  HGLOBAL hglob = nullptr;
  if (GetHGlobalFromStream(pOut, &hglob) != S_OK) {
    pOut->Release();
    return -1;
  }
  void *pv = GlobalLock(hglob);
  SIZE_T psz = GlobalSize(hglob);
  if (!pv || psz == 0) {
    GlobalUnlock(hglob);
    pOut->Release();
    return -1;
  }
  pngOut->assign((const uint8_t *)pv, (const uint8_t *)pv + psz);
  GlobalUnlock(hglob);
  pOut->Release();
  return nv_win_png_ihdr_dims(pngOut->data(), pngOut->size(), w, h);
}

NV_WIN_CLIPIMG_ATTR char *nv_win_clipboard_read_image(int *out_w, int *out_h) {
  std::vector<uint8_t> png;
  int w = 0, hgt = 0;
  HANDLE clip = nullptr;
  UINT fmtPng;

  if (out_w) *out_w = 0;
  if (out_h) *out_h = 0;
  nv_win_gdip_ensure();

  if (!OpenClipboard(nullptr)) return nullptr;

  fmtPng = RegisterClipboardFormatA("PNG");
  if (fmtPng != 0 && (clip = GetClipboardData(fmtPng)) != nullptr) {
    if (nv_win_clip_read_png_handle(clip, &png, &w, &hgt) != 0) {
      CloseClipboard();
      return nullptr;
    }
  } else if ((clip = GetClipboardData(CF_DIBV5)) != nullptr || (clip = GetClipboardData(CF_DIB)) != nullptr) {
    if (nv_win_dib_to_png(clip, &png, &w, &hgt) != 0) {
      CloseClipboard();
      return nullptr;
    }
  } else {
    CloseClipboard();
    return nullptr;
  }
  CloseClipboard();

  if (png.empty() || w <= 0 || hgt <= 0) return nullptr;

  size_t encb = nv_base64_encode_bound(png.size());
  char *out = (char *)malloc(encb);
  if (!out || nv_base64_encode(png.data(), png.size(), out, encb) < 0) {
    free(out);
    return nullptr;
  }
  if (out_w) *out_w = w;
  if (out_h) *out_h = hgt;
  return out;
}

NV_WIN_CLIPIMG_ATTR int nv_win_clipboard_write_image(const char *base64_png) {
  size_t blen, maxd, dlen = 0;
  std::vector<uint8_t> raw;
  HGLOBAL hg;
  void *pv;
  UINT fmtPng;

  if (!base64_png) return -1;
  blen = strlen(base64_png);
  maxd = nv_base64_decode_max_out(blen);
  raw.resize(maxd ? maxd : 1);
  if (nv_base64_decode(base64_png, blen, raw.data(), raw.size(), &dlen) != 0) return -1;
  raw.resize(dlen);
  if (dlen < 24u || nv_win_png_ihdr_dims(raw.data(), raw.size(), nullptr, nullptr) != 0) return -1;

  hg = GlobalAlloc(GMEM_MOVEABLE, dlen);
  if (!hg) return -1;
  pv = GlobalLock(hg);
  if (!pv) {
    GlobalFree(hg);
    return -1;
  }
  memcpy(pv, raw.data(), dlen);
  GlobalUnlock(hg);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(hg);
    return -1;
  }
  if (!EmptyClipboard()) {
    GlobalFree(hg);
    CloseClipboard();
    return -1;
  }
  fmtPng = RegisterClipboardFormatA("PNG");
  if (fmtPng == 0 || SetClipboardData(fmtPng, hg) == nullptr) {
    GlobalFree(hg);
    CloseClipboard();
    return -1;
  }
  CloseClipboard();
  return 0;
}

NV_WIN_CLIPIMG_ATTR int nv_win_clipboard_has_image(void) {
  UINT fmtPng = RegisterClipboardFormatA("PNG");
  if (fmtPng != 0 && IsClipboardFormatAvailable(fmtPng)) return 1;
  if (IsClipboardFormatAvailable(CF_DIBV5)) return 1;
  if (IsClipboardFormatAvailable(CF_DIB)) return 1;
  return 0;
}

} /* extern "C" */

#endif /* _WIN32 */
