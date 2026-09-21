#include "Room.hpp"

namespace dethnor {

RoomAssets LoadRoom(engine::Engine& app) {
    return RoomAssets{
        .background = app.LoadTexture(RoomConfig::backgroundAsset),
        .sideWall = app.LoadTexture(RoomConfig::sideWallAsset),
    };
}

void DrawRoom(engine::Engine& app, const RoomAssets& assets) {
    // ZoneRuntime._draw(): one background tile per screen of zone size,
    // tiled left to right using the texture's own width.
    const float bgWidth = static_cast<float>(app.TextureWidth(assets.background));
    for (int i = 0; i < RoomConfig::sizeInScreens; ++i) {
        app.DrawSprite(assets.background, bgWidth * static_cast<float>(i), 0.0f);
    }

    // Left side wall, mirrored -- ZoneRuntime._draw()'s has_left_wall branch
    // (draw_set_transform with a (-1,1) scale). Negative source width flips
    // the draw the same way character facing does; the destination position
    // (0,0) is unaffected by the flip, same as DrawSpriteRegion's facing use
    // elsewhere.
    const float sideWallWidth = static_cast<float>(app.TextureWidth(assets.sideWall));
    const float sideWallHeight = static_cast<float>(app.TextureHeight(assets.sideWall));
    app.DrawSpriteRegion(assets.sideWall, engine::Rect{0.0f, 0.0f, -sideWallWidth, sideWallHeight}, 0.0f, 0.0f);

    // No right side wall: has_right_wall is false for this zone in the
    // source data (see RoomConfig's comment on the room-selection tradeoff).
}

} // namespace dethnor
