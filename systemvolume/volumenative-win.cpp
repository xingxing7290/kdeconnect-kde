/*
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "../plugins/systemvolume/PolicyConfig.h"
#include "volumecontroller.h"
#include <Functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <qt_windows.h>
#include <wrl/client.h>

namespace SystemVolume
{
using Microsoft::WRL::ComPtr;
class WindowsAudioSystem : public AudioSystem
{
public:
    WindowsAudioSystem()
    {
        const auto result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_uninitialize = SUCCEEDED(result);
        if (SUCCEEDED(result) || result == RPC_E_CHANGED_MODE)
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_enumerator));
    }
    ~WindowsAudioSystem() override
    {
        m_enumerator.Reset();
        if (m_uninitialize)
            CoUninitialize();
    }
    QList<Sink> read(QString *error) override
    {
        error->clear();
        if (!m_enumerator)
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_enumerator));
        if (!m_enumerator) {
            *error = QStringLiteral("audio_unavailable");
            return {};
        }
        ComPtr<IMMDeviceCollection> devices;
        if (FAILED(m_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
            *error = QStringLiteral("enumeration_failed");
            return {};
        }
        ComPtr<IMMDevice> defaultDevice;
        QString defaultId;
        if (SUCCEEDED(m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice)))
            defaultId = identifier(defaultDevice.Get());
        UINT count = 0;
        if (FAILED(devices->GetCount(&count))) {
            *error = QStringLiteral("enumeration_failed");
            return {};
        }
        QList<Sink> result;
        for (UINT index = 0; index < count; ++index) {
            ComPtr<IMMDevice> device;
            if (FAILED(devices->Item(index, &device)))
                continue;
            const auto id = identifier(device.Get());
            ComPtr<IAudioEndpointVolume> volume;
            if (id.isEmpty() || FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &volume)))
                continue;
            float scalar = 0;
            BOOL muted = FALSE;
            if (FAILED(volume->GetMasterVolumeLevelScalar(&scalar)) || FAILED(volume->GetMute(&muted)))
                continue;
            QString description = id;
            ComPtr<IPropertyStore> properties;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
                PROPVARIANT property;
                PropVariantInit(&property);
                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &property)) && property.vt == VT_LPWSTR && property.pwszVal)
                    description = QString::fromWCharArray(property.pwszVal);
                PropVariantClear(&property);
            }
            result.append({id, description, qBound(0, qRound(scalar * 100), 100), bool(muted), id == defaultId});
        }
        return result;
    }
    QString setVolume(const QString &id, int value) override
    {
        const auto endpoint = volumeEndpoint(id);
        return endpoint && SUCCEEDED(endpoint->SetMasterVolumeLevelScalar(float(value) / 100, nullptr)) ? QString() : QStringLiteral("volume_failed");
    }
    QString setMuted(const QString &id, bool value) override
    {
        const auto endpoint = volumeEndpoint(id);
        return endpoint && SUCCEEDED(endpoint->SetMute(value, nullptr)) ? QString() : QStringLiteral("mute_failed");
    }
    QString setDefault(const QString &id) override
    {
        if (!volumeEndpoint(id))
            return QStringLiteral("output_unavailable");
        ComPtr<IPolicyConfigVista> policy;
        // Reuse the existing KDE Windows multimedia-role policy interface.
        if (FAILED(CoCreateInstance(__uuidof(CPolicyConfigVistaClient), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy))))
            return QStringLiteral("default_output_failed");
        return SUCCEEDED(policy->SetDefaultEndpoint(reinterpret_cast<LPCWSTR>(id.utf16()), eMultimedia)) ? QString() : QStringLiteral("default_output_failed");
    }

private:
    QString identifier(IMMDevice *device)
    {
        LPWSTR text = nullptr;
        const auto result = device->GetId(&text);
        const auto id = SUCCEEDED(result) && text ? QString::fromWCharArray(text) : QString();
        CoTaskMemFree(text);
        return id;
    }
    ComPtr<IAudioEndpointVolume> volumeEndpoint(const QString &id)
    {
        ComPtr<IAudioEndpointVolume> endpoint;
        if (!m_enumerator)
            return endpoint;
        ComPtr<IMMDevice> device;
        if (FAILED(m_enumerator->GetDevice(reinterpret_cast<LPCWSTR>(id.utf16()), &device)))
            return endpoint;
        DWORD state = 0;
        if (SUCCEEDED(device->GetState(&state)) && state == DEVICE_STATE_ACTIVE)
            device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &endpoint);
        return endpoint;
    }
    bool m_uninitialize = false;
    ComPtr<IMMDeviceEnumerator> m_enumerator;
};
std::unique_ptr<AudioSystem> createNativeAudioSystem()
{
    return std::make_unique<WindowsAudioSystem>();
}
}
