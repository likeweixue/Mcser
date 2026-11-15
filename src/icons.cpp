// Mcser - 图标绘制 (纯 GDI 运行时绘制, 无外部资源文件)
#include "mcser.h"

// 在 24x24 区域绘制工具栏图标
void DrawToolIcon(HDC hdc, int x, int y, int iconId)
{
    HPEN penDark = CreatePen(PS_SOLID, 1, RGB(0x55, 0x55, 0x55));
    HPEN penGray = CreatePen(PS_SOLID, 1, RGB(0xAA, 0xAA, 0xAA));
    HPEN penPurple = CreatePen(PS_SOLID, 1, RGB(0x7D, 0x5B, 0xD6));
    HPEN penRed  = CreatePen(PS_SOLID, 2, RGB(0xC6, 0x28, 0x28));
    HPEN penGreen= CreatePen(PS_SOLID, 2, RGB(0x1A, 0x7F, 0x37));
    HPEN penBlue = CreatePen(PS_SOLID, 2, RGB(0x1A, 0x5F, 0xB4));
    HPEN penYellowDark = CreatePen(PS_SOLID, 1, RGB(0x8A, 0x6D, 0x1A));
    HBRUSH brYellow = CreateSolidBrush(RGB(0xF7, 0xD7, 0x74));
    HBRUSH brWhite  = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    HBRUSH brBlueGray = CreateSolidBrush(RGB(0xD8, 0xE4, 0xF0));
    HBRUSH brGray   = CreateSolidBrush(RGB(0xE8, 0xE8, 0xE8));
    HBRUSH brPurple = CreateSolidBrush(RGB(0x7D, 0x5B, 0xD6));

    HGDIOBJ oldPen = SelectObject(hdc, penDark);
    HGDIOBJ oldBrush = SelectObject(hdc, brWhite);

    switch (iconId) {
    case ICO_ADD: { // 文件夹 + 加号
        SelectObject(hdc, brYellow); SelectObject(hdc, penYellowDark);
        Rectangle(hdc, x+2, y+6, x+23, y+20);
        Rectangle(hdc, x+2, y+6, x+10, y+9);
        SelectObject(hdc, penGreen);
        MoveToEx(hdc, x+12, y+11, NULL); LineTo(hdc, x+12, y+16);
        MoveToEx(hdc, x+9, y+14, NULL);  LineTo(hdc, x+15, y+14);
        break; }
    case ICO_EXTRACT: { // 文件夹 + 向上箭头
        SelectObject(hdc, brYellow); SelectObject(hdc, penYellowDark);
        Rectangle(hdc, x+2, y+8, x+23, y+20);
        Rectangle(hdc, x+2, y+8, x+10, y+11);
        SelectObject(hdc, penGreen);
        MoveToEx(hdc, x+12, y+17, NULL); LineTo(hdc, x+12, y+10);
        MoveToEx(hdc, x+9, y+13, NULL);  LineTo(hdc, x+12, y+10);
        MoveToEx(hdc, x+15, y+13, NULL); LineTo(hdc, x+12, y+10);
        break; }
    case ICO_TEST: { // 剪贴板 + 对勾
        SelectObject(hdc, brWhite); SelectObject(hdc, penDark);
        Rectangle(hdc, x+6, y+4, x+18, y+22);
        SelectObject(hdc, brBlueGray);
        Rectangle(hdc, x+9, y+2, x+15, y+6);
        SelectObject(hdc, penGreen);
        MoveToEx(hdc, x+9, y+13, NULL); LineTo(hdc, x+11, y+15); LineTo(hdc, x+15, y+10);
        break; }
    case ICO_VIEW: { // 页面 + 放大镜
        SelectObject(hdc, brWhite); SelectObject(hdc, penDark);
        Rectangle(hdc, x+3, y+3, x+16, y+21);
        SelectObject(hdc, penGray);
        MoveToEx(hdc, x+6, y+7, NULL); LineTo(hdc, x+14, y+7);
        MoveToEx(hdc, x+6, y+11, NULL); LineTo(hdc, x+14, y+11);
        MoveToEx(hdc, x+6, y+15, NULL); LineTo(hdc, x+12, y+15);
        SelectObject(hdc, brWhite); SelectObject(hdc, penDark);
        Ellipse(hdc, x+15, y+13, x+23, y+21);
        MoveToEx(hdc, x+21, y+19, NULL); LineTo(hdc, x+24, y+22);
        break; }
    case ICO_DELETE: { // 页面 + 红叉
        SelectObject(hdc, brWhite); SelectObject(hdc, penDark);
        Rectangle(hdc, x+3, y+3, x+16, y+21);
        SelectObject(hdc, penGray);
        MoveToEx(hdc, x+6, y+7, NULL); LineTo(hdc, x+14, y+7);
        MoveToEx(hdc, x+6, y+11, NULL); LineTo(hdc, x+14, y+11);
        SelectObject(hdc, penRed);
        MoveToEx(hdc, x+19, y+7, NULL); LineTo(hdc, x+23, y+11);
        MoveToEx(hdc, x+23, y+7, NULL); LineTo(hdc, x+19, y+11);
        break; }
    case ICO_FIND: { // 放大镜
        SelectObject(hdc, brWhite); SelectObject(hdc, penDark);
        Ellipse(hdc, x+3, y+3, x+16, y+16);
        MoveToEx(hdc, x+15, y+15, NULL); LineTo(hdc, x+22, y+22);
        break; }
    case ICO_WIZARD: { // 魔法棒
        SelectObject(hdc, brPurple); SelectObject(hdc, penPurple);
        POINT star[4] = { {x+18,y+2},{x+20,y+6},{x+23,y+7},{x+20,y+9} };
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Polygon(hdc, star, 4);
        SelectObject(hdc, penPurple);
        POINT star2[4] = { {x+18,y+2},{x+16,y+6},{x+13,y+7},{x+16,y+9} };
        Polygon(hdc, star2, 4);
        SelectObject(hdc, penDark);
        MoveToEx(hdc, x+12, y+13, NULL); LineTo(hdc, x+4, y+21);
        break; }
    case ICO_INFO: { // 圆圈 i
        SelectObject(hdc, brBlueGray); SelectObject(hdc, penDark);
        Ellipse(hdc, x+3, y+3, x+22, y+22);
        SelectObject(hdc, penBlue);
        MoveToEx(hdc, x+12, y+10, NULL); LineTo(hdc, x+12, y+17);
        SelectObject(hdc, brWhite);
        Rectangle(hdc, x+11, y+6, x+14, y+9);
        break; }
    case ICO_REPAIR: { // 扳手
        SelectObject(hdc, brGray); SelectObject(hdc, penDark);
        Rectangle(hdc, x+12, y+14, x+21, y+21);
        Ellipse(hdc, x+3, y+3, x+19, y+19);
        SelectObject(hdc, brGray);
        Ellipse(hdc, x+8, y+8, x+14, y+14);
        break; }
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(penDark); DeleteObject(penGray); DeleteObject(penRed);
    DeleteObject(penGreen); DeleteObject(penBlue); DeleteObject(penPurple);
    DeleteObject(penYellowDark);
    DeleteObject(brYellow); DeleteObject(brWhite); DeleteObject(brBlueGray);
    DeleteObject(brGray); DeleteObject(brPurple);
}

// 创建应用图标 (堆叠的书, 致敬经典压缩软件图标)
HICON CreateAppIcon(int size)
{
    // 32bpp DIB
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // 自上而下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbmp) { DeleteDC(hdc); return NULL; }
    HGDIOBJ old = SelectObject(hdc, hbmp);

    // 背景透明
    HBRUSH brBg = CreateSolidBrush(RGB(0, 0, 0));
    RECT rc = { 0, 0, size, size };
    FillRect(hdc, &rc, brBg);
    DeleteObject(brBg);

    // 三本堆叠的书
    float s = size / 32.0f;
    HBRUSH brP = CreateSolidBrush(RGB(0x7D, 0x3C, 0x98));
    HBRUSH brB = CreateSolidBrush(RGB(0x1A, 0x5F, 0xB4));
    HBRUSH brG = CreateSolidBrush(RGB(0x26, 0xA2, 0x69));
    HBRUSH brW = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
    HPEN penDark = CreatePen(PS_SOLID, 1, RGB(0x33, 0x33, 0x33));

    // 紫色书(最下) / 蓝色书(中) / 绿色书(上)
    int bw = (int)(20 * s), bh = (int)(8 * s);
    RECT r1 = { (int)(2 * s), (int)(20 * s), (int)(22 * s), (int)(28 * s) };
    RECT r2 = { (int)(6 * s), (int)(13 * s), (int)(26 * s), (int)(21 * s) };
    RECT r3 = { (int)(10 * s), (int)(6 * s), (int)(30 * s), (int)(14 * s) };
    FillRect(hdc, &r1, brP);
    FillRect(hdc, &r2, brB);
    FillRect(hdc, &r3, brG);
    // 白色书页线
    for (int i = 1; i <= 2; i++) {
        int yy = r1.top + i * (r1.bottom - r1.top) / 3;
        RECT line = { r1.left + 3, yy, r1.right - 3, yy + 1 };
        FillRect(hdc, &line, brW);
        yy = r2.top + i * (r2.bottom - r2.top) / 3;
        RECT line2 = { r2.left + 3, yy, r2.right - 3, yy + 1 };
        FillRect(hdc, &line2, brW);
        yy = r3.top + i * (r3.bottom - r3.top) / 3;
        RECT line3 = { r3.left + 3, yy, r3.right - 3, yy + 1 };
        FillRect(hdc, &line3, brW);
    }

    DeleteObject(brP); DeleteObject(brB); DeleteObject(brG); DeleteObject(brW);
    DeleteObject(penDark);

    // 提取像素
    int stride = size * 4;
    BYTE* xorBits = (BYTE*)malloc(stride * size);
    GetDIBits(hdc, hbmp, 0, size, xorBits, &bmi, DIB_RGB_COLORS);

    // AND 掩码: 全 0 = 不透明 (注意: 掩码中 0 表示显示 XOR 像素)
    int andStride = ((size + 31) / 32) * 4;
    BYTE* andBits = (BYTE*)calloc(andStride * size, 1);

    SelectObject(hdc, old);
    DeleteObject(hbmp);
    DeleteDC(hdc);

    HICON hIcon = CreateIcon(g_hInst, size, size, 1, 32, andBits, xorBits);
    free(xorBits);
    free(andBits);
    return hIcon;
}
