#include "imvg.h"
#include <cmath>

IvgPath::IvgPath()
{
}

IvgPath::IvgPath(IvgPath&& path)
{
}

IvgPath::~IvgPath()
{
}

//

IvgContext::IvgContext()
{
}

IvgContext::~IvgContext()
{
}

void IvgContext::Begin()
{
    vtx_offset_ = 0;
    cmd_offset_ = 0;
}

void IvgContext::End()
{
}

void IvgContext::StrokeRect(const IvgV2& min_bb, const IvgV2& max_bb)
{
}

void IvgContext::StrokeLine(const IvgV2& p0, const IvgV2& p1)
{
}

void IvgContext::StrokeTriangle(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2)
{
}

void IvgContext::StrokePolyline(const IvgV2* points, const uint32_t count)
{
}

void IvgContext::FillRect(const IvgV2& min_bb, const IvgV2& max_bb)
{
    _ResetFillRect();
    _ReserveVertices(5);
    uint32_t current_offset = vtx_offset_;
    IvgV2* vtx = vtx_buf_ + current_offset;
    vtx[0] = { min_bb.x, min_bb.y };
    vtx[1] = { max_bb.x, min_bb.y };
    vtx[2] = { max_bb.x, max_bb.y };
    vtx[3] = { min_bb.x, max_bb.y };
    vtx[4] = { min_bb.x, min_bb.y };
    vtx_offset_ += 5;
    fill_rect_.min = min_bb;
    fill_rect_.max = max_bb;
    _EmitDrawCommand(current_offset, 4);
}

void IvgContext::FillTriangle(const IvgV2& p0, const IvgV2& p1, const IvgV2& p2)
{
    uint32_t current_offset = vtx_offset_;
    _ResetFillRect();
    _ReserveVertices(4);
    _PushPointUnchecked(p0.x, p0.y);
    _PushPointUnchecked(p1.x, p1.y);
    _PushPointUnchecked(p2.x, p2.y);
    _PushPointUnchecked(p0.x, p0.y);
    _EmitDrawCommand(current_offset, 3);
}

void IvgContext::FillPolygon(const IvgV2* points, const uint32_t count)
{
    IVG_ASSERT(points && "points must be a valid pointer");
    IVG_ASSERT(count > 2 && "There must be at least 3 points");
    uint32_t current_offset = vtx_offset_;
    _ResetFillRect();
    _ReserveVertices(count + 1);
    for (uint32_t i = 0; i < count; i++) {
        const IvgV2& point = points[i];
        _PushPointUnchecked(point.x, point.y);
    }
    _PushPointUnchecked(points[0].x, points[0].y);
    _EmitDrawCommand(current_offset, count);
}

void IvgContext::FillPath(const IvgPath& path)
{
}

void IvgContext::FillPathBuffer(const IvgPathCmd* cmd, const IvgCoord* coord, uint32_t num_commands)
{
}

void IvgContext::_ReserveVertices(uint32_t count)
{
    uint32_t new_size = vtx_offset_ + count;
    if (new_size <= vtx_count_)
        return;
    uint32_t capacity = vtx_count_ ? vtx_count_ + vtx_count_ / 2 : 256;
    IvgV2* new_vtx_buf = (IvgV2*)IVG_ALLOC((size_t)capacity * sizeof(IvgV2));
    IVG_ASSERT(new_vtx_buf != nullptr && "Cannot allocate vertex buffer");
    if (vtx_buf_)
        std::memcpy(new_vtx_buf, vtx_buf_, vtx_offset_ * sizeof(IvgV2));
    IVG_FREE(vtx_buf_);
    vtx_count_ = capacity;
    vtx_buf_ = new_vtx_buf;
}

void IvgContext::_ReserveCommandBytes(uint32_t size)
{
    uint32_t new_size = cmd_offset_ + size;
    if (new_size <= cmd_size_)
        return;
    uint32_t capacity = cmd_size_ ? cmd_size_ + cmd_size_ / 2 : 256;
    IvgByte* new_cmd_buf = (IvgByte*)IVG_ALLOC((size_t)capacity);
    IVG_ASSERT(new_cmd_buf != nullptr && "Cannot allocate command buffer");
    if (cmd_buf_)
        std::memcpy(new_cmd_buf, cmd_buf_, cmd_offset_);
    IVG_FREE(cmd_buf_);
    cmd_size_ = capacity;
    cmd_buf_ = new_cmd_buf;
}

void IvgContext::_EmitDrawCommand(uint32_t vtx_offset, uint32_t vtx_count)
{
    IVG_ASSERT(paint_ != nullptr && "Paint object must be set");

    if (state_update_flags & DrawStateUpdate) {
        IvgSetDrawStateCmd* cmd = _AllocateCommand<IvgSetDrawStateCmd>();
        cmd->header = IvgCommandHeader_SetDrawState;
        cmd->fill_mode = fill_mode_;
        cmd->paint_type = paint_->type_;
        cmd->num_color_stops = paint_->num_stops_;
        if (cmd->num_color_stops < 3) {
            cmd->color[0] = paint_->color_ptr_[0];
            cmd->color[1] = paint_->color_ptr_[1];
            cmd->color_offset[0] = paint_->offsets_ptr_[0];
            cmd->color_offset[1] = paint_->offsets_ptr_[1];
        }
    }

    if (state_update_flags & ClipRectUpdate) {
        IvgSetClipRectCmd* cmd = _AllocateCommand<IvgSetClipRectCmd>();
        cmd->header = IvgCommandHeader_SetClipRect;
        cmd->x = (int32_t)clip_rect_.min.x;
        cmd->y = (int32_t)clip_rect_.min.y;
        cmd->w = (int32_t)std::ceil(clip_rect_.max.x - clip_rect_.min.x);
        cmd->h = (int32_t)std::ceil(clip_rect_.max.y - clip_rect_.min.y);
    }

    if (state_update_flags & ColorStateUpdate) {
        // TODO
    }

    // Clip fill rect to clip rect
    fill_rect_.min.x = IvgMax(fill_rect_.min.x, clip_rect_.min.x);
    fill_rect_.min.y = IvgMax(fill_rect_.min.y, clip_rect_.min.y);
    fill_rect_.max.x = IvgMin(fill_rect_.max.x, clip_rect_.max.x);
    fill_rect_.max.y = IvgMin(fill_rect_.max.y, clip_rect_.max.y);

    IvgDrawCmd* cmd = _AllocateCommand<IvgDrawCmd>();
    int32_t min_x = (int32_t)fill_rect_.min.x;
    int32_t min_y = (int32_t)fill_rect_.min.y;
    int32_t max_x = (int32_t)std::ceil(fill_rect_.max.x);
    int32_t max_y = (int32_t)std::ceil(fill_rect_.max.y);
    cmd->header = IvgCommandHeader_Draw;
    cmd->w = (uint32_t)(max_x - min_x);
    cmd->h = (uint32_t)(max_y - min_y);
    cmd->vtx_count = vtx_count;
    cmd->vtx_offset = vtx_offset;
    cmd->rect = fill_rect_;
    state_update_flags = 0;
}
