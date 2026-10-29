#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace editor {

inline constexpr COLORREF kPanel = RGB(29, 29, 30);
inline constexpr COLORREF kChrome = RGB(31, 31, 32);
inline constexpr COLORREF kSeparator = RGB(34, 34, 35);
inline constexpr COLORREF kGrip = RGB(104, 104, 106);
inline constexpr COLORREF kBorder = RGB(57, 57, 59);
inline constexpr COLORREF kButton = RGB(43, 43, 44);
inline constexpr COLORREF kButtonPrimary = RGB(51, 51, 53);
inline constexpr COLORREF kButtonTop = RGB(68, 68, 70);
inline constexpr COLORREF kButtonBottom = RGB(20, 20, 21);
inline constexpr COLORREF kInput = RGB(38, 38, 39);
inline constexpr COLORREF kInputActive = RGB(47, 47, 49);
inline constexpr COLORREF kInputBorder = RGB(74, 74, 76);
inline constexpr COLORREF kInputBorderActive = RGB(230, 112, 43);
inline constexpr COLORREF kRowSelected = RGB(48, 48, 49);
inline constexpr COLORREF kAccent = RGB(230, 112, 43);
inline constexpr COLORREF kIconOutline = RGB(20, 20, 21);
inline constexpr COLORREF kIconGrid = RGB(38, 38, 39);

} // namespace editor
