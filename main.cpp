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
#include <atlbase.h> // ����ָ�����
// #include <iostream>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")

// ����ȫ�ֱ��� extern "C" __declspec(dllexport)
HRESULT hr;
IMFMediaSource* pSource = nullptr;
IMFSourceReader* pReader = nullptr;
std::vector<IMFActivate*> devices; // ����ͷ����

// ����Ƿ���ڿ��õ�����ͷ ö��������Ƶ�����豸��USB ����ͷ��
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

// ���� SourceReader������ȡ�ֱ���
HRESULT ConfigureSourceReader(IMFSourceReader* pReader, int index_pix, uint32_t& width, uint32_t& height) {
    // ���� ö��ԭ������ ���� 
    printf("ö��ԭ��ý������:\n");
     for (DWORD i = 0; ; ++i) {
        IMFMediaType* pNative = nullptr;
        hr = pReader->GetNativeMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), i, &pNative
        );
        if (FAILED(hr)) {
            printf("  [%u] ���� �޸������� (hr=0x%08X)\n", i, hr);
            break;
        }
        UINT32 nativeWidth = 0, nativeHeight = 0;
        if (SUCCEEDED(MFGetAttributeSize(pNative, MF_MT_FRAME_SIZE, &nativeWidth, &nativeHeight))) {
            printf("  [%u] �ֱ���: %dx%d\n", i, nativeWidth, nativeHeight);
        }
        GUID subType = { 0 };
        pNative->GetGUID(MF_MT_SUBTYPE, &subType);
        WCHAR szGuid[64] = { 0 };
        StringFromGUID2(subType, szGuid, ARRAYSIZE(szGuid));
        wprintf(L"  [%u] subtype = %s\n", i, szGuid);
        pNative->Release();
    }
    // ��ȡ�豸ԭ��֧�ֵ�ý������
    IMFMediaType* pNativeType = nullptr;
    hr = pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), index_pix, &pNativeType);
    if (FAILED(hr)) {
        printf("��ȡ�豸ԭ��֧�ֵ�ý������ʧ��\n");
        return hr;
    }
    // ֱ��ʹ��ԭ����ʽ����
    hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pNativeType);
    // ��ȡ��Ӧ��Ƶ����ʽ�ĳߴ�width, height �޸�width, &height������ַ�ϵ�ֵ
    MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
    pNativeType->Release();
    return hr;
}


bool init_usb(int index_devices = 0, int index_pix = 0)
{
    uint32_t width = 0;
    uint32_t height = 0;
    // ��ʼ�� COM �� Media Foundation
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { MessageBox(NULL, L"CoInitializeEx ʧ��", L"����", MB_OK); return false; }
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) { MessageBox(NULL, L"MFStartup ʧ��", L"����", MB_OK); CoUninitialize(); return false; }
    // ö������ͷ
    if (!devices.empty()) { devices.clear(); }
    hr = EnumerateVideoDevices(devices);
    if (FAILED(hr) || devices.empty()) {
        MessageBox(NULL, L"δ�ҵ���Ƶ�����豸", L"����", MB_OK);
        MFShutdown();
        CoUninitialize();
        return false;
    }
    IMFActivate* pActivate = devices[index_devices]; // ѡ���һ������ͷ
    // �����豸����
    hr = pActivate->ActivateObject(IID_PPV_ARGS(&pSource));
    if (FAILED(hr)) {
        printf("�����豸����ʧ��:  0x%08X\n", hr);
        return false;
    }
    // ���� SourceReader 
    hr = MFCreateSourceReaderFromMediaSource(pSource, NULL, &pReader);
    if (FAILED(hr)) {
        pSource->Shutdown();
        pSource->Release();
        printf("���� SourceReaderʧ��:  0x%08X\n", hr);
        return false;
    }
    // ���������ʽ
    hr = ConfigureSourceReader(pReader, index_pix, width, height);
    if (FAILED(hr)) {
        pReader->Release();
        pSource->Shutdown();
        pSource->Release();
        printf("���������ʽʧ��:  0x%08X\n", hr);
        return false;
    }
    return true;
}



bool get_pix(char** nv12prt, int bufferSize)
{
    // ������Ч�Լ��
    if (!nv12prt || !*nv12prt) {
        printf("��������ָ����Ч\n");
        return false;
    }

    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;
    CComPtr<IMFSample> pSample = nullptr;
    hr = pReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &streamIndex, &flags, &timestamp, &pSample);
    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
        printf("USB����֡��ȡΪ��\n");
        return false;
    }
    if (!pSample) return false;
    CComPtr<IMFMediaBuffer> pBuffer = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr)) return false;
    BYTE* pData = nullptr; // ԭʼ����ָ��
    DWORD maxLen = 0, currLen = 0;
    // ��ӻ�������������
    if (FAILED(pBuffer->Lock(&pData, &maxLen, &currLen))) return false;
    // ��һ�������С��֤
    if (currLen > static_cast<DWORD>(bufferSize)) {
        printf("�������������Ҫ%u�ֽڣ����ṩ%u�ֽ�\n", currLen, bufferSize);
        pBuffer->Unlock();
        return false;
    }
    // ��ȫ�ڴ渴��
    if (memcpy_s(nv12prt, bufferSize, pData, currLen) != 0) {
        printf("�ڴ渴��ʧ��\n");
        pBuffer->Unlock();
        return false;
    }
    pBuffer->Unlock();
    return true;
}




bool release_usb() 
{
    // ����ǰ����ͷ
    pReader->Release();
    pSource->Shutdown();
    pSource->Release();
    // �ͷ������豸����
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
    int index_devices = 0; // ѡ��ڼ���USB���
    int index_pix = 0; // ѡ������ı��뷽ʽ
    bool is_init = false; // �Ƿ��ʼ��
    bool is_release = false; // �Ƿ��ͷ�
    bool is_pix = false; // �Ƿ��ȡͼ��
    const int bufferSize = 3110400; // 3110400 = 1920*1080*3/2
    
    char* nv12Buffer = new char[bufferSize]; // �������ļ�
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
