#ifdef _WIN32
#define MRI_EXPORT extern "C" __declspec(dllexport)
#else
#define MRI_EXPORT extern "C"
#endif

MRI_EXPORT int Init(const char*)
{
    return 0;
}
