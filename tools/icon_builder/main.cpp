// Offline asset generator; Windows GDI renders a glyph, no font file is redistributed.
#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool value) { if (!value) throw std::runtime_error("Icon generation failed"); }
template<class T> void write(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
std::vector<std::uint32_t> render(int size) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size; info.bmiHeader.biHeight = size;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels{};
    HDC dc = CreateCompatibleDC(nullptr); require(dc != nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    require(bitmap != nullptr);
    const auto oldBitmap = SelectObject(dc, bitmap);
    RECT bounds{0,0,size,size};
    const auto brush = CreateSolidBrush(RGB(40,40,40));
    FillRect(dc, &bounds, brush); DeleteObject(brush);
    const int padding = size >= 32 ? size / 16 : 1;
    HFONT font = CreateFontW(-(size - 2 * padding), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        CHINESEBIG5_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH, L"Microsoft JhengHei UI");
    require(font != nullptr);
    const auto oldFont = SelectObject(dc, font);
    // Fail rather than silently publishing an unrelated fallback font or missing glyph.
    wchar_t face[80]{}; GetTextFaceW(dc, 80, face);
    require(std::wstring(face) == L"Microsoft JhengHei UI");
    WORD glyph{};
    require(GetGlyphIndicesW(dc, L"劃", 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && glyph != 0xffff);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
    require(DrawTextW(dc, L"劃", 1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) != 0);
    GdiFlush();
    auto* source = static_cast<std::uint32_t*>(pixels);
    std::vector<std::uint32_t> result(source, source + size * size);
    for (auto& pixel : result) pixel |= 0xff000000U;
    SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
    DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
    return result;
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 3) { std::cerr << "stroke_icon_builder OUTPUT.ico PREVIEW.png\n"; return 2; }
        constexpr std::array<int,8> sizes{16,20,24,32,40,48,64,256};
        std::array<std::vector<std::uint32_t>, sizes.size()> images;
        for (std::size_t i = 0; i < sizes.size(); ++i) images[i] = render(sizes[i]);
        std::ofstream out(std::filesystem::path(argv[1]), std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(out));
        write(out, std::uint16_t{0}); write(out, std::uint16_t{1}); write(out, std::uint16_t{8});
        std::uint32_t offset = 6 + 16 * static_cast<std::uint32_t>(sizes.size());
        for (const auto size : sizes) {
            const auto dimension = static_cast<std::uint8_t>(size == 256 ? 0 : size);
            const auto bytes = static_cast<std::uint32_t>(40 + size * size * 4 + ((size + 31) / 32) * 4 * size);
            write(out, dimension); write(out, dimension); write(out, std::uint16_t{0});
            write(out, std::uint16_t{1}); write(out, std::uint16_t{32}); write(out, bytes); write(out, offset);
            offset += bytes;
        }
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            const int size = sizes[i];
            BITMAPINFOHEADER header{}; header.biSize = sizeof(header); header.biWidth = size;
            header.biHeight = size * 2; header.biPlanes = 1; header.biBitCount = 32;
            write(out, header);
            out.write(reinterpret_cast<const char*>(images[i].data()), static_cast<std::streamsize>(images[i].size() * 4));
            std::vector<char> mask(static_cast<std::size_t>(((size + 31) / 32) * 4 * size), 0);
            out.write(mask.data(), static_cast<std::streamsize>(mask.size()));
        }
        out.close(); require(static_cast<bool>(out));
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token{}; require(Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok);
        {
            // Show small sizes at actual pixel dimensions, with the large glyph alongside.
            Gdiplus::Bitmap preview(440, 360, PixelFormat32bppARGB);
            Gdiplus::Graphics graphics(&preview); graphics.Clear(Gdiplus::Color(255,220,220,220));
            int nextX = 12;
            for (std::size_t i = 0; i < sizes.size(); ++i) {
                const auto size = sizes[i];
                Gdiplus::Bitmap tile(size, size, PixelFormat32bppARGB);
                for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
                    tile.SetPixel(x, y, Gdiplus::Color(images[i][static_cast<std::size_t>((size-1-y)*size+x)]));
                const int x = size == 256 ? 92 : nextX;
                const int y = size == 256 ? 90 : 8;
                graphics.DrawImage(&tile, x, y, size, size);
                nextX += size + 12;
            }
            const CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
            require(preview.Save(argv[2], &png, nullptr) == Gdiplus::Ok);
        }
        Gdiplus::GdiplusShutdown(token);
        std::cout << "Generated 8 icon sizes\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
