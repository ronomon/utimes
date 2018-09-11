#include <nan.h>
#include <stdint.h>
#if defined(__APPLE__)
#include <sys/attr.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <io.h>
#include <windows.h>

void set_utimes_filetime(const uint64_t time, FILETIME* filetime) {
  int64_t temp = (int64_t) ((time * 10000ULL) + 116444736000000000ULL);
  (filetime)->dwLowDateTime = (DWORD) (temp & 0xFFFFFFFF);
  (filetime)->dwHighDateTime = (DWORD) (temp >> 32);
}
#endif

int set_utimes(
  const char* path,
  const uint8_t flags,
  const uint64_t btime,
  const uint64_t mtime,
  const uint64_t atime
) {
  if (flags == 0) return 0;
#if defined(__APPLE__)
  struct attrlist attrs;
  struct timespec times[3];
  unsigned int index = 0;
  memset(&attrs, 0, sizeof(struct attrlist));
  attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
  if (flags & 1) {
    attrs.commonattr |= ATTR_CMN_CRTIME;
    times[index].tv_sec = (time_t) (btime / 1000);
    times[index].tv_nsec = (long) ((btime % 1000) * 1000000);
    index++;
  }
  if (flags & 2) {
    attrs.commonattr |= ATTR_CMN_MODTIME;
    times[index].tv_sec = (time_t) (mtime / 1000);
    times[index].tv_nsec = (long) ((mtime % 1000) * 1000000);
    index++;
  }
  if (flags & 4) {
    attrs.commonattr |= ATTR_CMN_ACCTIME;
    times[index].tv_sec = (time_t) (atime / 1000);
    times[index].tv_nsec = (long) ((atime % 1000) * 1000000);
    index++;
  }
  return setattrlist(path, &attrs, times, index * sizeof(struct timespec), 0);
#elif defined(_WIN32)
  int chars = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
  if (chars == 0) return GetLastError();
  WCHAR* pathw = (WCHAR*) malloc(chars * sizeof(WCHAR));
  if (pathw == NULL) return ERROR_OUTOFMEMORY;
  MultiByteToWideChar(CP_UTF8, 0, path, -1, pathw, chars);
  HANDLE handle = CreateFileW(
    pathw,
    FILE_WRITE_ATTRIBUTES,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    NULL
  );
  free(pathw);
  if (handle == INVALID_HANDLE_VALUE) return GetLastError();
  FILETIME btime_filetime;
  FILETIME mtime_filetime;
  FILETIME atime_filetime;
  if (flags & 1) set_utimes_filetime(btime, &btime_filetime);
  if (flags & 2) set_utimes_filetime(mtime, &mtime_filetime);
  if (flags & 4) set_utimes_filetime(atime, &atime_filetime);
  if (!SetFileTime(
    handle,
    (flags & 1) ? &btime_filetime : NULL,
    (flags & 4) ? &atime_filetime : NULL,
    (flags & 2) ? &mtime_filetime : NULL
  )) {
    CloseHandle(handle);
    return GetLastError();
  } else {
    CloseHandle(handle);
    return 0;
  }
#else
  return -1;
#endif
}

class UtimesWorker : public Nan::AsyncWorker {
 public:
  UtimesWorker(
    v8::Local<v8::Object> &pathHandle,
    const uint8_t flags,
    const uint64_t btime,
    const uint64_t mtime,
    const uint64_t atime,
    Nan::Callback *callback
  ) : Nan::AsyncWorker(callback),
      flags(flags),
      btime(btime),
      mtime(mtime),
      atime(atime) {
        SaveToPersistent("pathHandle", pathHandle);
        path = node::Buffer::Data(pathHandle);
  }

  ~UtimesWorker() {}

  void Execute() {
    result = set_utimes(path, flags, btime, mtime, atime);
    if (result > 0) result = -result;
  }

  void HandleOKCallback () {
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = {
      Nan::New<v8::Number>(result)
    };
    callback->Call(1, argv, async_resource);
  }

 private:
  const char* path;
  const uint8_t flags;
  const uint64_t btime;
  const uint64_t mtime;
  const uint64_t atime;
  int result;
};

NAN_METHOD(utimes) {
  if (
    info.Length() != 6 ||
    !node::Buffer::HasInstance(info[0]) ||
    !info[1]->IsUint32() ||
    !info[2]->IsNumber() ||
    !info[3]->IsNumber() ||
    !info[4]->IsNumber() ||
    !info[5]->IsFunction()
  ) {
    return Nan::ThrowError(
      "bad arguments, expected: ("
      "buffer path, int flags, "
      "seconds btime, seconds mtime, seconds atime, "
      "function callback"
      ")"
    );
  }
  v8::Local<v8::Object> pathHandle = info[0].As<v8::Object>();
  const uint8_t flags = info[1]->Uint32Value();
  const uint64_t btime = info[2]->IntegerValue();
  const uint64_t mtime = info[3]->IntegerValue();
  const uint64_t atime = info[4]->IntegerValue();
  Nan::Callback *callback = new Nan::Callback(info[5].As<v8::Function>());
  Nan::AsyncQueueWorker(new UtimesWorker(
    pathHandle,
    flags,
    btime,
    mtime,
    atime,
    callback
  ));
}

NAN_MODULE_INIT(Init) {
  NAN_EXPORT(target, utimes);
}

NODE_MODULE(binding, Init)

// S.D.G.
