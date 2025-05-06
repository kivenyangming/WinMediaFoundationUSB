#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <mferror.h>
#include <ks.h>
#include <ksmedia.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <vector>
#include <fstream>
#include <atlbase.h> // 智能指针管理
// #include <iostream>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")

// 设置全局变量 extern "C" __declspec(dllexport)
HRESULT hr;
IMFMediaSource* pSource = nullptr;
IMFSourceReader* pReader = nullptr;
std::vector<IMFActivate*> devices; // 摄像头容器

// 检测是否存在可用的摄像头 枚举所有视频捕获设备（USB 摄像头）
HRESULT EnumerateVideoDevices(std::vector<IMFActivate*>& devices) {
    IMFAttributes* pAttrs = nullptr;
    hr = MFCreateAttributes(&pAttrs, 1);
    if (FAILED(hr)) return hr;

    hr = pAttrs->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if (FAILED(hr)) { pAttrs->Release(); return hr; }

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttrs, &ppDevices, &count);
    if (SUCCEEDED(hr)) {
        for (UINT32 i = 0; i < count; i++) {
            devices.push_back(ppDevices[i]);
        }
        CoTaskMemFree(ppDevices);
    }
    pAttrs->Release();
    return hr;
}

// 配置 SourceReader，并获取分辨率
HRESULT ConfigureSourceReader(IMFSourceReader* pReader, int index_pix, uint32_t& width, uint32_t& height) {
    // —— 枚举原生类型 —— 
    printf("枚举原生媒体类型:\n");
     for (DWORD i = 0; ; ++i) {
        IMFMediaType* pNative = nullptr;
        hr = pReader->GetNativeMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), i, &pNative
        );
        if (FAILED(hr)) {
            printf("  [%u] —— 无更多类型 (hr=0x%08X)\n", i, hr);
            break;
        }
        UINT32 nativeWidth = 0, nativeHeight = 0;
        if (SUCCEEDED(MFGetAttributeSize(pNative, MF_MT_FRAME_SIZE, &nativeWidth, &nativeHeight))) {
            printf("  [%u] 分辨率: %dx%d\n", i, nativeWidth, nativeHeight);
        }
        GUID subType = { 0 };
        pNative->GetGUID(MF_MT_SUBTYPE, &subType);
        WCHAR szGuid[64] = { 0 };
        StringFromGUID2(subType, szGuid, ARRAYSIZE(szGuid));
        wprintf(L"  [%u] subtype = %s\n", i, szGuid);
        pNative->Release();
    }
    // 获取设备原生支持的媒体类型
    IMFMediaType* pNativeType = nullptr;
    hr = pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), index_pix, &pNativeType);
    if (FAILED(hr)) {
        printf("获取设备原生支持的媒体类型失败\n");
        return hr;
    }
    // 直接使用原生格式配置
    hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pNativeType);
    // 获取对应视频流格式的尺寸width, height 修改width, &height参数地址上的值
    MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
    pNativeType->Release();
    return hr;
}


bool init_usb(int index_devices = 0, int index_pix = 0)
{
    uint32_t width = 0;
    uint32_t height = 0;
    // 初始化 COM 与 Media Foundation
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { MessageBox(NULL, L"CoInitializeEx 失败", L"错误", MB_OK); return false; }
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { MessageBox(NULL, L"MFStartup 失败", L"错误", MB_OK); CoUninitialize(); return false; }
    // 枚举摄像头
    if (!devices.empty()) { devices.clear(); }
    hr = EnumerateVideoDevices(devices);
    if (FAILED(hr) || devices.empty()) {
        MessageBox(NULL, L"未找到视频捕获设备", L"错误", MB_OK);
        MFShutdown();
        CoUninitialize();
        return false;
    }
    IMFActivate* pActivate = devices[index_devices]; // 选择第一个摄像头
    // 激活设备对象
    hr = pActivate->ActivateObject(IID_PPV_ARGS(&pSource));
    if (FAILED(hr)) {
        printf("激活设备对象失败:  0x%08X\n", hr);
        return false;
    }
    // 创建 SourceReader 
    hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
    if (FAILED(hr)) {
        pSource->Shutdown();
        pSource->Release();
        printf("创建 SourceReader失败:  0x%08X\n", hr);
        return false;
    }
    // 配置输出格式
    hr = ConfigureSourceReader(pReader, index_pix, width, height);
    if (FAILED(hr)) {
        pReader->Release();
        pSource->Shutdown();
        pSource->Release();
        printf("配置输出格式失败:  0x%08X\n", hr);
        return false;
    }
    return true;
}



bool get_pix(char** nv12prt, int bufferSize)
{
    // 参数有效性检查
    if (!nv12prt || !*nv12prt) {
        printf("错误：输入指针无效\n");
        return false;
    }

    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;
    CComPtr<IMFSample> pSample = nullptr;
    hr = pReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &streamIndex, &flags, &timestamp, &pSample);
    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
        printf("USB数据帧获取为空\n");
        return false;
    }
    if (!pSample) return false;
    CComPtr<IMFMediaBuffer> pBuffer = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr)) return false;
    BYTE* pData = nullptr; // 原始数据指针
    DWORD maxLen = 0, currLen = 0;
    // 添加缓冲区锁定保护
    if (FAILED(pBuffer->Lock(&pData, &maxLen, &currLen))) return false;
    // 添家缓冲区大小验证
    if (currLen > static_cast<DWORD>(bufferSize)) {
        printf("缓冲区溢出！需要%u字节，仅提供%u字节\n", currLen, bufferSize);
        pBuffer->Unlock();
        return false;
    }
    // 安全内存复制
    if (memcpy_s(nv12prt, bufferSize, pData, currLen) != 0) {
        printf("内存复制失败\n");
        pBuffer->Unlock();
        return false;
    }
    pBuffer->Unlock();
    return true;
}




bool release_usb() 
{
    // 清理当前摄像头
    pReader->Release();
    pSource->Shutdown();
    pSource->Release();
    // 释放所有设备对象
    for (auto p : devices) 
    {
        p->Release();
    }
    devices.clear();
    MFShutdown();
    CoUninitialize();
    return true;
}

int main()
{
    int index_devices = 0; // 选择第几个USB相机
    int index_pix = 0; // 选择相机的编码方式
    bool is_init = false; // 是否初始化
    bool is_release = false; // 是否释放
    bool is_pix = false; // 是否获取图像
    const int bufferSize = 3110400; // 3110400 = 1920*1080*3/2
    
    char* nv12Buffer = new char[bufferSize]; // 二进制文件
    is_init = init_usb(index_devices, index_pix);
    printf("is_usb = %d\n", is_init);
  
    for (int a = 0; a < 60; a = a + 1) 
    {
        is_pix = get_pix(&nv12Buffer, bufferSize);
        printf("main: nv12Buffer = %p\n", nv12Buffer);
    }
 
    std::ofstream file("C:/Users/kiven/Desktop/SourceReader/ReadUSB/NV12BIN.bin", std::ios::binary);
    if (file.is_open()) {
        file.write(nv12Buffer, bufferSize);
        file.close();
    }
  
    
    is_release = release_usb();
    delete[] nv12Buffer;
    printf("is_usb = %d\n", is_release);
    return 0;
}
