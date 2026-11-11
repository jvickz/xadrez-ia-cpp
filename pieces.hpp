#pragma once
#include <gdiplus.h>
#include <cctype>

// Original vector silhouettes, drawn at any board size with antialiasing.
inline void drawChessPiece(HDC dc, char piece, RECT rect, bool lifted = false) {
    using namespace Gdiplus;
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(static_cast<REAL>(rect.left), static_cast<REAL>(rect.top));
    g.ScaleTransform((rect.right - rect.left) / 100.f, (rect.bottom - rect.top) / 100.f);
    SolidBrush shadow(Color(lifted ? 85 : 45, 0, 0, 0));
    g.FillEllipse(&shadow, 22.f, lifted ? 86.f : 83.f, 60.f, 10.f);
    const bool white = piece >= 'A' && piece <= 'Z';
    const Color edge = white ? Color(255, 70, 61, 49) : Color(255, 15, 19, 24);
    const Color bright = white ? Color(255, 255, 251, 232) : Color(255, 93, 105, 117);
    const Color dark = white ? Color(255, 207, 196, 173) : Color(255, 31, 38, 47);
    LinearGradientBrush body(Point(25, 20), Point(74, 80), bright, dark);
    Pen outline(edge, 2.2f);
    outline.SetLineJoin(LineJoinRound);
    Pen detail(white ? Color(255, 122, 107, 81) : Color(255, 155, 164, 173), 2.f);
    auto shape = [&](GraphicsPath& path) { g.FillPath(&body, &path); g.DrawPath(&outline, &path); };
    GraphicsPath stem;
    stem.AddBezier(39, 45, 41, 58, 39, 67, 29, 75);
    stem.AddLine(29, 75, 71, 75);
    stem.AddBezier(71, 75, 61, 67, 59, 58, 61, 45);
    stem.CloseFigure();
    shape(stem);
    const char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
    GraphicsPath head;
    switch (kind) {
        case 'p':
            head.AddEllipse(36, 20, 28, 28);
            shape(head);
            g.FillEllipse(&body, 33, 45, 34, 9);
            g.DrawEllipse(&outline, 33, 45, 34, 9);
            break;
        case 'r': {
            Point points[] = {{28,18},{38,18},{38,28},{45,28},{45,18},{55,18},{55,28},
                              {62,28},{62,18},{72,18},{70,43},{61,50},{39,50},{30,43}};
            head.AddPolygon(points, 13); shape(head);
            g.DrawLine(&detail, 33, 39, 67, 39);
            break;
        }
        case 'n':
            head.AddLine(30, 72, 35, 55);
            head.AddLine(35, 55, 24, 56);
            head.AddBezier(24, 56, 17, 53, 22, 46, 26, 41);
            head.AddLine(26, 41, 41, 25);
            head.AddLine(41, 25, 41, 13);
            head.AddLine(41, 13, 54, 20);
            head.AddBezier(54, 20, 75, 26, 77, 53, 67, 73);
            head.CloseFigure(); shape(head);
            g.DrawBezier(&detail, 56, 28, 68, 39, 64, 53, 55, 62);
            { SolidBrush eye(edge); g.FillEllipse(&eye, 40, 34, 4, 4); }
            break;
        case 'b':
            head.AddBezier(50, 14, 22, 36, 30, 48, 50, 51);
            head.AddBezier(50, 51, 70, 48, 78, 36, 50, 14);
            head.CloseFigure(); shape(head);
            g.DrawLine(&detail, 52, 25, 42, 39);
            g.FillEllipse(&body, 46, 10, 8, 8);
            g.DrawEllipse(&outline, 46, 10, 8, 8);
            break;
        case 'q': {
            Point points[] = {{26,25},{38,39},{39,20},{50,34},{61,20},{62,39},{74,25},{65,53},{35,53}};
            head.AddPolygon(points, 9); shape(head);
            for (Point point : {Point(26,23), Point(39,18), Point(61,18), Point(74,23)}) {
                g.FillEllipse(&body, point.X - 4, point.Y - 4, 8, 8);
                g.DrawEllipse(&outline, point.X - 4, point.Y - 4, 8, 8);
            }
            g.DrawLine(&detail, 37, 46, 63, 46);
            break;
        }
        case 'k':
            head.AddBezier(32, 34, 22, 47, 34, 56, 50, 54);
            head.AddBezier(50, 54, 66, 56, 78, 47, 68, 34);
            head.CloseFigure(); shape(head);
            head.Reset();
            { Point cross[] = {{46,12},{54,12},{54,20},{63,20},{63,28},{54,28},
                                {54,37},{46,37},{46,28},{37,28},{37,20},{46,20}};
              head.AddPolygon(cross, 12); shape(head); }
            break;
        default: break;
    }
    GraphicsPath base;
    base.AddLine(29, 73, 71, 73);
    base.AddBezier(71, 73, 75, 77, 78, 81, 77, 86);
    base.AddLine(77, 86, 23, 86);
    base.AddBezier(23, 86, 22, 81, 25, 77, 29, 73);
    base.CloseFigure(); shape(base);
    g.DrawLine(&detail, 29, 80, 71, 80);
}
