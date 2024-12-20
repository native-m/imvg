#ifndef __IMVG_H__
#define __IMVG_H__

#include "imvg_misc.h"
#include <cfloat>

#define IVG_MAKE_COLOR(r, g, b, a) (uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24))

using IvgCoord = float;
using IvgByte = char;

enum IvgStrokeCap
{
    IvgStrokeCap_Butt,
    IvgStrokeCap_Round,
    IvgStrokeCap_Square,
    IvgStrokeCap_Triangle,
};

enum IvgStrokeJoin
{
    IvgStrokeJoin_None,
    IvgStrokeJoin_Miter,
    IvgStrokeJoin_Round,
    IvgStrokeJoin_Bevel,
};

enum IvgStrokeOffset
{
    IvgStrokeOffset_Center,
    IvgStrokeOffset_Inside,
    IvgStrokeOffset_Outside,
};

enum IvgFillMode
{
    IvgFillMode_NonZero,
    IvgFillMode_EvenOdd,
};

enum IvgPaintType
{
    IvgPaintType_Solid,
    IvgPaintType_Linear,
};

enum IvgPathCmd
{
    IvgPathCmd_Close,
    IvgPathCmd_MoveTo,
    IvgPathCmd_LineTo,
    IvgPathCmd_QuadTo,
    IvgPathCmd_CubicTo,
    IvgPathCmd_ArcTo,
};

struct IvgV2
{
    IvgCoord x, y;
    IvgV2() : x(0), y(0) {}
    IvgV2(IvgCoord coord_x, IvgCoord coord_y) : x(coord_x), y(coord_y) {}
};

static inline IvgV2 operator+(const IvgV2& lhs, const IvgCoord rhs) { return IvgV2(lhs.x + rhs, lhs.y + rhs); }
static inline IvgV2 operator-(const IvgV2& lhs, const IvgCoord rhs) { return IvgV2(lhs.x - rhs, lhs.y - rhs); }
static inline IvgV2 operator*(const IvgV2& lhs, const IvgCoord rhs) { return IvgV2(lhs.x * rhs, lhs.y * rhs); }
static inline IvgV2 operator/(const IvgV2& lhs, const IvgCoord rhs) { return IvgV2(lhs.x / rhs, lhs.y / rhs); }
static inline IvgV2 operator+(const IvgV2& lhs, const IvgV2& rhs) { return IvgV2(lhs.x + rhs.x, lhs.y + rhs.y); }
static inline IvgV2 operator-(const IvgV2& lhs, const IvgV2& rhs) { return IvgV2(lhs.x - rhs.x, lhs.y - rhs.y); }
static inline IvgV2 operator*(const IvgV2& lhs, const IvgV2& rhs) { return IvgV2(lhs.x * rhs.x, lhs.y * rhs.y); }
static inline IvgV2 operator/(const IvgV2& lhs, const IvgV2& rhs) { return IvgV2(lhs.x / rhs.x, lhs.y / rhs.y); }
static inline IvgV2 operator-(const IvgV2& lhs) { return IvgV2(-lhs.x, -lhs.y); }
static inline IvgV2& operator+=(IvgV2& lhs, const IvgCoord rhs) { lhs.x += rhs; lhs.y += rhs; return lhs; }
static inline IvgV2& operator-=(IvgV2& lhs, const IvgCoord rhs) { lhs.x -= rhs; lhs.y -= rhs; return lhs; }
static inline IvgV2& operator*=(IvgV2& lhs, const IvgCoord rhs) { lhs.x *= rhs; lhs.y *= rhs; return lhs; }
static inline IvgV2& operator/=(IvgV2& lhs, const IvgCoord rhs) { lhs.x /= rhs; lhs.y /= rhs; return lhs; }
static inline IvgV2& operator+=(IvgV2& lhs, const IvgV2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
static inline IvgV2& operator-=(IvgV2& lhs, const IvgV2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
static inline IvgV2& operator*=(IvgV2& lhs, const IvgV2& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; }
static inline IvgV2& operator/=(IvgV2& lhs, const IvgV2& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; return lhs; }
static inline bool operator==(const IvgV2& lhs, const IvgV2& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
static inline bool operator!=(const IvgV2& lhs, const IvgV2& rhs) { return lhs.x != rhs.x || lhs.y != rhs.y; }

struct IvgRect
{
    IvgV2 min;
    IvgV2 max;

    IvgRect() : min(), max() {}
    IvgRect(IvgCoord x, IvgCoord y, IvgCoord width, IvgCoord height) : min(x, y), max(x + width, y + height) {}
    IvgRect(const IvgV2& min_coord, const IvgV2& max_coord) : min(min_coord), max(max_coord) {}
};

struct IvgMat
{
    float m[6];
};

struct IvgColor
{
    float r, g, b, a;
};

struct IvgPaint
{
    IvgPaintType type_;
    uint32_t num_stops_;
    uint32_t colors_[8];
    float offsets_[8];
    uint32_t* color_ptr_{ colors_ };
    float* offsets_ptr_{ offsets_ };

    constexpr IvgPaint() : type_(IvgPaintType_Solid), num_stops_(1), colors_{}, offsets_{} {}

    constexpr IvgPaint(int r, int g, int b, int a) :
        type_(IvgPaintType_Solid),
        num_stops_(1),
        colors_{ IVG_MAKE_COLOR(r, g, b, a) },
        offsets_{}
    {
    }

    constexpr IvgPaint(uint32_t rgba) :
        type_(IvgPaintType_Solid),
        num_stops_(1),
        colors_{ rgba },
        offsets_{}
    {
    }

    inline void SetType(IvgPaintType type) { type_ = type; }

    inline void SetColor(uint32_t rgba) { color_ptr_[0] = rgba; }

    inline void SetColor(int r, int g, int b, int a) { color_ptr_[0] = IVG_MAKE_COLOR(r, g, b, a); }

    inline void SetColor(const IvgColor& color) {}

    inline void ResizeGradientStop(uint32_t n) {}

    inline void SetGradientStop() {}

    inline void PushGradientStop() {}
};

struct IvgPath
{
    IvgPathCmd* cmd_;
    IvgCoord* coords_;
    uint32_t cmd_size_;
    uint32_t cmd_capacity_;
    uint32_t coord_size_;

    IvgPath();
    IvgPath(IvgPath&& path);
    IvgPath(const IvgPath&) = delete;
    ~IvgPath();

    void MoveTo(IvgCoord x, IvgCoord y);
    void MoveTo(const IvgV2& p);
    void LineTo(IvgCoord x, IvgCoord y);
    void LineTo(const IvgV2& p);
    void QuadTo(IvgCoord x0, IvgCoord y0, IvgCoord x1, IvgCoord y1);
    void QuadTo(const IvgV2& p0, const IvgV2& p1);
    void CubicTo(IvgCoord x0, IvgCoord y0, IvgCoord x1, IvgCoord y1, IvgCoord x2, IvgCoord y2);
    void CubicTo(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2);
    void ArcTo(IvgCoord rh, IvgCoord rv, IvgCoord angle, IvgCoord end_x, IvgCoord end_y);
    void ArcTo(const IvgV2& r, IvgCoord angle, const IvgV2& end);
    void ArcTo(IvgCoord cx, IvgCoord cy, IvgCoord rh, IvgCoord rv, IvgCoord start_angle, IvgCoord end_angle);
    void ArcTo(const IvgV2& c, const IvgV2& r, IvgCoord start_angle, IvgCoord end_angle);
    void Close();

    void Append(const IvgPath& path);
    void Append(const IvgPathCmd* cmd, uint32_t num_cmds, const IvgCoord* coords, uint32_t num_coords);
    void Reserve(uint32_t cmd_capacity, uint32_t coord_capacity);
    void ShrinkToFit();
    void Clone(IvgPath* path);
};

struct IvgBackend
{
    void* backend_data;
    void (*begin)(void* backend_data);
    void (*end)(void* backend_data);
};

enum IvgCommandHeader
{
    IvgCommandHeader_SetDrawState,
    IvgCommandHeader_SetClipRect,
    IvgCommandHeader_Draw,
};

struct IvgSetDrawStateCmd
{
    uint32_t header;
    IvgFillMode fill_mode;
    IvgPaintType paint_type;
    uint32_t num_color_stops;
    uint32_t color[2];
    float color_offset[2];
};

struct IvgSetClipRectCmd
{
    uint32_t header;
    int32_t x, y;
    uint32_t w, h;
};

struct IvgDrawCmd
{
    uint32_t header;
    uint32_t w, h;
    uint32_t vtx_count;
    uint32_t vtx_offset;
    IvgRect rect;
};

union IvgBackendCommand
{
    uint32_t header;
    IvgSetDrawStateCmd set_draw_state;
    IvgSetClipRectCmd set_clip_rect;
    IvgDrawCmd draw;
};

union IvgCmdBufPtr
{
    IvgByte* cmd_bytes;
    IvgBackendCommand* cmd_data;
};

struct IvgContext
{
    enum StateUpdate
    {
        DrawStateUpdate = 1 << 0,
        ClipRectUpdate = 1 << 1,
        ColorStateUpdate = 1 << 2,
    };

    IvgBackend backend_;
    IvgStrokeJoin stroke_join_;
    IvgFillMode fill_mode_;
    float stroke_width_;
    float miter_limit_;
    IvgPath imm_path_;
    IvgRect clip_rect_;
    IvgRect fill_rect_;
    uint32_t fb_width_ = 0;
    uint32_t fb_height_ = 0;
    const IvgPaint* paint_;

    IvgV2* vtx_buf_{};
    uint32_t vtx_count_{};
    uint32_t vtx_offset_{};
    uint32_t vtx_close_{};

    uint32_t state_update_flags{};
    IvgByte* cmd_buf_{};
    uint32_t cmd_size_{};
    uint32_t cmd_offset_{};

    IvgContext();
    ~IvgContext();

    void Begin();
    void End();

    inline void SetFramebufferSize(uint32_t width, uint32_t height)
    {
        fb_width_ = width;
        fb_height_ = height;
    }

    inline void SetPaint(const IvgPaint* paint)
    {
        paint_ = paint;
        state_update_flags |= DrawStateUpdate;
        if (paint->num_stops_ > 2 && paint->type_ != IvgPaintType_Solid)
            state_update_flags |= ColorStateUpdate;
    }

    inline void SetStrokeWidth(float width) { stroke_width_ = width; }

    inline void SetMiterLimit(float miter_limit) { miter_limit_ = miter_limit; }

    inline void SetStrokeJoin(IvgStrokeJoin join) { stroke_join_ = join; }

    inline void SetFillMode(IvgFillMode fill_mode)
    {
        fill_mode_ = fill_mode;
        state_update_flags |= DrawStateUpdate;
    }

    inline void SetClipRect(const IvgRect& rect)
    {
        clip_rect_ = rect;
        state_update_flags |= ClipRectUpdate;
    }

    inline void SetTransformMatrix(const IvgMat& mat);

    void StrokeRect(const IvgV2& min_bb, const IvgV2& max_bb);
    void StrokeLine(const IvgV2& p0, const IvgV2& p1);
    void StrokeTriangle(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2);
    void StrokePolyline(const IvgV2* points, const uint32_t count);

    void FillRect(const IvgV2& min_bb, const IvgV2& max_bb);
    void FillTriangle(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2);
    void FillPolygon(const IvgV2* points, const uint32_t count);
    void FillPath(const IvgPath& path);
    void FillPathBuffer(const IvgPathCmd* cmd, const IvgCoord* coord, uint32_t num_commands);

    // Immediate path command
    void BeginPath();
    void MoveTo(IvgCoord x, IvgCoord y);
    void MoveTo(const IvgV2& p);
    void LineTo(IvgCoord x, IvgCoord y);
    void LineTo(const IvgV2& p);
    void QuadTo(IvgCoord x0, IvgCoord y0, IvgCoord x1, IvgCoord y1);
    void QuadTo(const IvgV2& p0, const IvgV2& p1);
    void CubicTo(IvgCoord x0, IvgCoord y0, IvgCoord x1, IvgCoord y1, IvgCoord x2, IvgCoord y2);
    void CubicTo(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2);
    void ArcTo(IvgCoord rh, IvgCoord rv, IvgCoord angle, IvgCoord end_x, IvgCoord end_y);
    void ArcTo(const IvgV2& r, IvgCoord angle, const IvgV2& end);
    void ArcTo(IvgCoord cx, IvgCoord cy, IvgCoord rh, IvgCoord rv, IvgCoord start_angle, IvgCoord end_angle);
    void ArcTo(const IvgV2& c, const IvgV2& r, IvgCoord start_angle, IvgCoord end_angle);
    void ClosePath();
    void FillPath(bool preserve_path = false);
    void StrokePath(bool preserve_path = false);


    inline void _PushPoint(float x, float y)
    {
        if (vtx_offset_ == vtx_count_)
            _ReserveVertices(1);
        if (x < fill_rect_.min.x)
            fill_rect_.min.x = x;
        if (y < fill_rect_.min.y)
            fill_rect_.min.y = y;
        if (x > fill_rect_.max.x)
            fill_rect_.max.x = x;
        if (y > fill_rect_.max.y)
            fill_rect_.max.y = y;
        IvgV2* vtx = vtx_buf_ + vtx_offset_;
        vtx->x = x;
        vtx->y = y;
        vtx_offset_++;
    }

    inline void _PushPointUnchecked(float x, float y)
    {
        if (x < fill_rect_.min.x)
            fill_rect_.min.x = x;
        if (y < fill_rect_.min.y)
            fill_rect_.min.y = y;
        if (x > fill_rect_.max.x)
            fill_rect_.max.x = x;
        if (y > fill_rect_.max.y)
            fill_rect_.max.y = y;
        IvgV2* vtx = vtx_buf_ + vtx_offset_;
        vtx->x = x;
        vtx->y = y;
        vtx_offset_++;
    }

    inline void _ResetFillRect()
    {
        fill_rect_.min.x = (float)fb_width_;
        fill_rect_.min.y = (float)fb_height_;
        fill_rect_.max.x = 0.0f;
        fill_rect_.max.y = 0.0f;
    }

    template <typename T>
    inline T* _AllocateCommand()
    {
        uint32_t new_offset = cmd_offset_ + sizeof(T);
        if (new_offset >= cmd_size_)
            _ReserveCommandBytes(sizeof(T));
        uint32_t current_offset = cmd_offset_;
        cmd_offset_ = new_offset;
        return (T*)(cmd_buf_ + current_offset);
    }

    void _ReserveVertices(uint32_t count);
    void _ReserveCommandBytes(uint32_t size);
    void _EmitDrawCommand(uint32_t vtx_offset, uint32_t vtx_count);
};

namespace ImVG
{
    bool CreateContext(IvgContext** out_context, IvgBackend& backend);
    inline static void Begin() {}
    inline static void End() {}
}

namespace ImVG::Utils
{
    void* DllLoad(const char* name);
    void DllUnload(void* dll);
    void* DllFindFunction(void* dll, const char* name);
}

#endif // __IMVG_VULKAN_H__
