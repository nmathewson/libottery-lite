
static HMODULE
load_windows_library(const TCHAR *library_name)
{
  TCHAR path[MAX_PATH];
  unsigned n;
  n = GetSystemDirectory(path, MAX_PATH);
  if (n == 0 || n + _tcslen(library_name) + 2 >= MAX_PATH)
    return 0;
  _tcscat(path, TEXT("\\"));
  _tcscat(path, library_name);
  return LoadLibrary(path);
}

static int
ottery_getentropy_fallback_kludge(u8 *out)
{
  struct fallback_entropy_accumulator fbe, *accumulator = &fbe;
  int iter;
  int iter;
  HMODULE netapi32 = NULL;
  NET_API_STATUS (*netfree_fn)(LPVOID) = NULL;

  fallback_entropy_accumulator_init(accumulator);

  {
    FBENT_ADD_FN_ADDR(ottery_getentropy_fallback_kludge);
    FBENT_ADD_FN_ADDR(printf);
    FBENT_ADD_ADDR(&iter);
  }
  {
    MEMORYSTATUSEX m;
    DWORD dw = GetCurrentProcessId();
    FBENT_ADD(dw);
    dw = GetCurrentThreadId();
    FBENT_ADD(dw);
    GlobalMemoryStatusEx(&m);
    FBENT_ADD(m);
  }
  {
    LPTCH env = GetEnvironmentStrings();
    TCHAR *end_of_env = env;
    /* This is double-nul-terminated. Ick. */
    while (strlen(end_of_env))
      end_of_env += strlen(end_of_env)+1;
    FBENT_ADD_CHUNK(env, end_of_env - env);
  }
  {
    HW_PROFILE_INFO hwp;
    GetCurrentHwProfile(&hwp);
    FBENT_ADD(hwp);
  }
  {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    FBENT_ADD(si);
  }
  /* Also see: VirtualQuery, VirtualQueryEx, Raw memory.

     ToollhelpCreateSnapshot32
   */
  {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (snap != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 pwe;
      THREADENTRY32 te;
      MODULEENTRY32 me;
      HEAPLIST32 hl;
      HEAPENTRY32 he;
      BOOL ok, ok2;
      pwe.dwSize = sizeof(pwe);
      te.dwSize = sizeof(te);
      me.dwSize = sizeof(me);
      hl.dwSize = sizeof(hl);
      he.dwSize = sizeof(he);

      for (ok = Heap32ListFirst(snap, &hl); ok; ok = Heap32ListNext(snap, &hl)) {
        FBENT_ADD(hl);
        for (ok2 = Heap32First(&he, hl.th32ProcessID, hl.th32HeapID); ok2;
             ok2 = Heap32Next(&he)) {
          FBENT_ADD(he);
        }
      }
      for (ok = Process32First(snap, &pwe); ok; ok = Process32Next(snap, &pwe)) {
        FBENT_ADD(pwe);
      }
      for (ok = Module32First(snap, &me); ok; ok = Module32Next(snap, &me)) {
        FBENT_ADD(me);
      }
      for (ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        FBENT_ADD(te);
      }
      CloseHandle(snap);
    }
  }

  {
    ULONG (WINAPI *getadapters_fn)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
    HMODULE lib = load_windows_library(TEXT("ihplapi.dll"));
    IP_ADAPTER_ADDRESSES *addrs = NULL;
    if (lib) {
      if ((getadapters_fn = (void*)GetProcAddress(lib, "GetAdaptersAddresses"))) {
        ULONG size, res;
        addrs = malloc(16384);
        if (addrs)
          goto done_getadapters;

        size = 16384;
        res = getadapters_fn(AF_UNSPEC, 0, NULL, addrs, &size);
        if (res == ERROR_BUFFER_OVERFLOW) {
          free(addrs);
          addrs = malloc(size);
          if (!addrs)
            goto done_getadapters;
          res = getadapters_fn(AF_UNSPEC, 0, NULL, addrs, &size);
        }
        if (res != NO_ERROR)
          goto done_getadapters;

        FBENT_ADD_CHUNK(addrs, size);
      }
    done_getadapters:
      if (addrs)
        free(addrs);
      CloseHandle(lib);
    }
  }

  netapi32 = load_windows_library("netapi32.dll");
  if (netapi32 != NULL) {
    NET_API_STATUS (*servergetinfo_fn)(LPWSTR, DWORD, LPBYTE *)
      = (void*)GetProcAddress(netapi32, "NetServerGetInfo");
    netfree_fn = (void*)GetProcAddress(netapi32, "NetApiBufferFree");

    if (servergetinfo_fn && netfree_fn) {
      LPBYTE b=NULL;
      if (servergetinfo_fn(NULL, 100, &b) == NERR_Success) {
        FBENT_ADD_CHUNK(b, sizeof(SERVER_INFO_100));
        netfree_fn(b);
      }
      if (servergetinfo_fn(NULL, 101, &b) == NERR_Success) {
        FBENT_ADD_CHUNK(b, sizeof(SERVER_INFO_101));
        netfree_fn(b);
      }
    }
  }

  for (iter = 0; iter < 128; ++iter) {
    {
      LARGE_INTEGER pc;
      FILETIME ft;
      DWORD ms;
      QueryPerformanceCounter(&pc);
      GetSystemTimePreciseAsFileTime(&ft);
      ms = GetTickCount();
      FBENT_ADD(pc);
      FBENT_ADD(ft);
      FBENT_ADD(ms);
    }
    if (netapi32 != NULL) {
      NET_API_STATUS (*statsget_fn)(LPWSTR, LPWSTR, DWORD, DWORD, LPBYTE *)
        = (void*)GetProcAddress(netapi32, "NetStatisticsGet");
      if (statsget_fn && netfree_fn) {
        LPBYTE b = NULL;
        if (statsget_fn(NULL, L"LanmanWorkstation", 0, 0, &b) == NERR_Success) {
          FBENT_ADD_CHUNK(b, sizeof(STAT_WORKSTATION_0));
          netfree_fn(b);
        }
        if (statsget_fn(NULL, L"LanmanServer", 0, 0, &b) == NERR_Success) {
          FBENT_ADD_CHUNK(b, sizeof(STAT_SERVER_0));
          netfree_fn(b);
        }
      }
    }

    if (iter % 16)
      Sleep(0);
    else
      Sleep(1);
  }

  if (netapi32)
    CloseHandle(netapi32);

  return fallback_entropy_accumulator_get_output(accumulator, out);
}
