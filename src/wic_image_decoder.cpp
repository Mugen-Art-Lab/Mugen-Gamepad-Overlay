// SPDX-FileCopyrightText: 2026 Mugen Art Lab
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wic_image_decoder.hpp"

#include <filesystem>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace mugen {
namespace {

std::filesystem::path utf8_to_native_path(const std::string &value)
{
#if defined(__cpp_char8_t)
    const auto *first = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path(std::u8string(first, first + value.size()));
#else
    return std::filesystem::path(value);
#endif
}

#if defined(_WIN32)
struct ComApartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComApartment()
    {
        if (SUCCEEDED(result))
            CoUninitialize();
    }
    bool usable() const
    {
        // COM can already be initialized as STA on the OBS caller thread.
        return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
    }
};
#endif

} // namespace

bool decode_image_file_rgba_wic(const std::string &utf8_path, uint32_t max_dimension,
                                uint64_t max_pixel_bytes, uint32_t &width,
                                uint32_t &height, std::vector<uint8_t> &rgba,
                                std::string &error)
{
#if defined(_WIN32)
    ComApartment apartment;
    if (!apartment.usable()) {
        error = "Windows image decoder could not initialize COM";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        error = "Windows image decoder could not initialize";
        return false;
    }

    const std::wstring native_path = utf8_to_native_path(utf8_path).wstring();
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        native_path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(hr)) {
        error = "Image could not be decoded";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        error = "Image could not be decoded";
        return false;
    }

    UINT pixel_width = 0;
    UINT pixel_height = 0;
    hr = frame->GetSize(&pixel_width, &pixel_height);
    if (FAILED(hr) || pixel_width == 0 || pixel_height == 0) {
        error = "Image dimensions could not be read";
        return false;
    }

    const uint64_t byte_count = static_cast<uint64_t>(pixel_width) * pixel_height * 4ULL;
    if (pixel_width > max_dimension || pixel_height > max_dimension ||
        byte_count > max_pixel_bytes || byte_count > UINT32_MAX) {
        error = "Image is too large";
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
    }
    if (FAILED(hr)) {
        error = "Image pixel format is unsupported";
        return false;
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(byte_count));
    hr = converter->CopyPixels(nullptr, pixel_width * 4U,
                               static_cast<UINT>(byte_count), pixels.data());
    if (FAILED(hr)) {
        error = "Image pixels could not be decoded";
        return false;
    }

    width = pixel_width;
    height = pixel_height;
    rgba = std::move(pixels);
    return true;
#else
    (void)utf8_path;
    (void)max_dimension;
    (void)max_pixel_bytes;
    (void)width;
    (void)height;
    (void)rgba;
    error = "Windows image decoder is unavailable";
    return false;
#endif
}

} // namespace mugen
