
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Size-aware realpath for both platforms. On Windows, _fullpath is purely
// lexical and does not resolve NTFS junctions/reparse points, so a junction
// planted inside the served root could bypass the realpath root-containment
// check. GetFinalPathNameByHandle follows reparse points to the real target.
// Unlike the old 2-argument shim, every intermediate buffer is bounded by the
// caller's real [resolved_size], so a small caller buffer (e.g. the 100-byte
// root buffer in main.c) can no longer be overrun by a 260-byte _MAX_PATH
// write.

char* realpath_n(const char* path, char* resolved, size_t resolved_size) {
#ifdef _WIN32
  if(resolved_size == 0)
    return NULL;
  if(!_fullpath(resolved, path, resolved_size))
    return NULL;

  // Room for the UTF-8 -> wide conversion of a path up to resolved_size plus
  // slack for the "\\?\" prefixes GetFinalPathNameByHandleW can add.
  size_t   wchars = resolved_size + 8;
  wchar_t* wide = (wchar_t*)malloc(wchars * sizeof(wchar_t));
  if(wide == NULL)
    return NULL;
  if(MultiByteToWideChar(CP_UTF8, 0, resolved, -1, wide, (int)wchars) == 0) {
    free(wide);
    return NULL;
  }

  HANDLE h =
      CreateFileW(wide, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if(h == INVALID_HANDLE_VALUE) {
    free(wide);
    return NULL;
  }

  DWORD len =
      GetFinalPathNameByHandleW(h, wide, (DWORD)wchars, FILE_NAME_NORMALIZED);
  CloseHandle(h);
  if(len == 0 || len >= wchars) {
    free(wide);
    return NULL;
  }

  wchar_t* p = wide;
  if(wcsncmp(p, L"\\\\?\\UNC\\", 8) == 0)
    p += 8;
  else if(wcsncmp(p, L"\\\\?\\", 4) == 0)
    p += 4;

  if(WideCharToMultiByte(CP_UTF8, 0, p, -1, resolved, (int)resolved_size, NULL,
                         NULL) == 0) {
    free(wide);
    return NULL;
  }
  free(wide);
  return resolved;
#else
  // realpath()'s 2-argument form requires resolved to be PATH_MAX bytes;
  // every caller in this codebase passes a smaller buffer, so resolve into
  // a PATH_MAX-sized local buffer first and only copy into the caller's
  // [resolved, resolved_size] if it actually fits.
  char tmp[PATH_MAX];
  if(realpath(path, tmp) == NULL)
    return NULL;
  size_t len = strlen(tmp);
  if(len >= resolved_size)
    return NULL;
  memcpy(resolved, tmp, len + 1);
  return resolved;
#endif
}
