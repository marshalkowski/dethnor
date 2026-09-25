// TEMPORARY (removed in M4 step 4, together with the hardcoded content it
// compares against): the JSON must reproduce the C++ definitions exactly.
// Included from inside content_check.cpp's anonymous namespace.

void CheckAction(const ActionDefinition& a, const ActionDefinition& b, const std::string& what) {
    Check(a.kind == b.kind, what + ": kind");
    Check(a.textureAsset == b.textureAsset, what + ": texture");
    Check(Near(a.frameWidth, b.frameWidth) && Near(a.frameHeight, b.frameHeight), what + ": frame size");
    Check(a.frameColumns == b.frameColumns, what + ": frame columns");
    Check(Near(a.frameDuration, b.frameDuration), what + ": frame duration");
    Check(a.activeFrameStart == b.activeFrameStart && a.activeFrameEnd == b.activeFrameEnd, what + ": active frames");
    Check(a.damage == b.damage, what + ": damage");
    Check(Near(a.knockbackForce, b.knockbackForce), what + ": knockback");
    Check(Near(a.stunTime, b.stunTime), what + ": stun");
    Check(Near(a.hitStopDuration, b.hitStopDuration), what + ": hit stop");
    Check(a.hitFreezeAttacker == b.hitFreezeAttacker && a.hitFreezeTarget == b.hitFreezeTarget, what + ": freeze");
    if (a.activeFrameStart >= 0) { // hitbox only meaningful with an active window
        Check(Near(a.hitboxOffset.x, b.hitboxOffset.x) && Near(a.hitboxOffset.y, b.hitboxOffset.y),
              what + ": hitbox offset");
        Check(Near(a.hitboxSize.x, b.hitboxSize.x) && Near(a.hitboxSize.y, b.hitboxSize.y), what + ": hitbox size");
    }
    Check(Near(a.initialStaminaCost, b.initialStaminaCost), what + ": initial stamina");
    Check(a.sustainable == b.sustainable, what + ": sustainable");
    Check(Near(a.sustainStaminaCostPerSec, b.sustainStaminaCostPerSec), what + ": sustain cost");
}

void CheckClipEq(const engine::AnimationClip& a, const engine::AnimationClip& b, const std::string& what) {
    Check(a.firstFrame == b.firstFrame && a.frameCount == b.frameCount, what + ": frames");
    Check(Near(a.frameWidth, b.frameWidth) && Near(a.frameHeight, b.frameHeight), what + ": size");
    Check(Near(a.frameDuration, b.frameDuration), what + ": duration");
    Check(a.loop == b.loop, what + ": loop");
}

void CheckCharacter(const CharacterDefinition& a, const CharacterDefinition& b, const std::string& what) {
    Check(Near(a.speed, b.speed), what + ": speed");
    Check(Near(a.collisionHalfWidth, b.collisionHalfWidth), what + ": collisionHalfWidth");
    Check(Near(a.collisionTopOffset, b.collisionTopOffset), what + ": collisionTopOffset");
    Check(Near(a.collisionHeight, b.collisionHeight), what + ": collisionHeight");
    Check(Near(a.hurtboxSize.x, b.hurtboxSize.x) && Near(a.hurtboxSize.y, b.hurtboxSize.y), what + ": hurtbox size");
    Check(Near(a.hurtboxOffset.x, b.hurtboxOffset.x) && Near(a.hurtboxOffset.y, b.hurtboxOffset.y),
          what + ": hurtbox offset");
    Check(Near(a.maxHitPoints, b.maxHitPoints), what + ": hp");
    Check(Near(a.maxStamina, b.maxStamina), what + ": stamina");
    Check(Near(a.staminaRegenPerSecond, b.staminaRegenPerSecond), what + ": stamina regen");
    Check(Near(a.staminaRegenDelay, b.staminaRegenDelay), what + ": stamina delay");
    Check(Near(a.maxMagicPoints, b.maxMagicPoints), what + ": mp");
    Check(Near(a.iframesOnHitSec, b.iframesOnHitSec), what + ": iframes");
    Check(Near(a.iframeBlinkPeriod, b.iframeBlinkPeriod), what + ": blink");
    Check(Near(a.knockbackDecay, b.knockbackDecay), what + ": knockback decay");
    Check(a.immobile == b.immobile, what + ": immobile");
    Check(Near(a.spriteFrameWidth, b.spriteFrameWidth) && Near(a.spriteFrameHeight, b.spriteFrameHeight),
          what + ": sprite size");
    CheckClipEq(a.idleClip, b.idleClip, what + " idle");
    CheckClipEq(a.walkClip, b.walkClip, what + " walk");
    CheckClipEq(a.hurtClip, b.hurtClip, what + " hurt");
    CheckClipEq(a.deathClip, b.deathClip, what + " death");
    Check(a.idleAsset == b.idleAsset && a.walkAsset == b.walkAsset && a.hurtAsset == b.hurtAsset &&
              a.deathAsset == b.deathAsset,
          what + ": assets");
    Check(a.actions.size() == b.actions.size(), what + ": action count");
    for (std::size_t i = 0; i < std::min(a.actions.size(), b.actions.size()); ++i) {
        Check(a.actions[i].command == b.actions[i].command, what + ": action command " + std::to_string(i));
        CheckAction(*a.actions[i].action, *b.actions[i].action, what + " action " + std::to_string(i));
    }
}

void CheckRectEq(const engine::Rect& a, const engine::Rect& b, const std::string& what) {
    Check(Near(a.x, b.x) && Near(a.y, b.y) && Near(a.width, b.width) && Near(a.height, b.height), what + ": rect");
}

void CheckDestinationEq(const std::optional<Destination>& a, const std::optional<Destination>& b,
                        const std::string& what) {
    Check(a.has_value() == b.has_value(), what + ": presence");
    if (a && b) {
        Check(a->world == b->world && a->level == b->level && a->spawnId == b->spawnId, what + ": value");
    }
}

void CheckLevelEq(const LevelDefinition& a, const LevelDefinition& b, const std::string& what) {
    Check(a.bgSet == b.bgSet, what + ": bg set");
    CheckDestinationEq(a.leftDestination, b.leftDestination, what + " left");
    CheckDestinationEq(a.rightDestination, b.rightDestination, what + " right");
    if (a.zones.size() != b.zones.size()) {
        Fail(what + ": zone count");
        return;
    }
    for (std::size_t i = 0; i < a.zones.size(); ++i) {
        const ZoneDefinition& x = a.zones[i];
        const ZoneDefinition& y = b.zones[i];
        const std::string zone = what + " zone " + std::to_string(i);
        Check(x.sizeInScreens == y.sizeInScreens && x.hasLeftWall == y.hasLeftWall && x.hasRightWall == y.hasRightWall,
              zone + ": size/walls");
        CheckDestinationEq(x.doorDestination, y.doorDestination, zone + " door");
        Check(x.triggerRects.size() == y.triggerRects.size(), zone + ": trigger count");
        for (std::size_t r = 0; r < std::min(x.triggerRects.size(), y.triggerRects.size()); ++r) {
            CheckRectEq(x.triggerRects[r], y.triggerRects[r], zone + " trigger");
        }
        if (!x.waves.empty()) {
            CheckRectEq(x.spawnRect, y.spawnRect, zone + " spawn rect");
        }
        Check(x.spawnPoints.size() == y.spawnPoints.size(), zone + ": spawn point count");
        for (std::size_t s = 0; s < std::min(x.spawnPoints.size(), y.spawnPoints.size()); ++s) {
            const SpawnPointDefinition& p = x.spawnPoints[s];
            const SpawnPointDefinition& q = y.spawnPoints[s];
            Check(p.id == q.id && Near(p.position.x, q.position.x) && Near(p.position.y, q.position.y) &&
                      p.faceLeft == q.faceLeft,
                  zone + ": spawn point " + std::to_string(s));
        }
        Check(x.waves.size() == y.waves.size(), zone + ": wave count");
        for (std::size_t w = 0; w < std::min(x.waves.size(), y.waves.size()); ++w) {
            Check(x.waves[w].TotalEnemyCount() == y.waves[w].TotalEnemyCount(), zone + ": wave enemy count");
        }
    }
}

void CheckAgainstHardcoded(const ContentLibrary& library) {
    CheckCharacter(library.Character("player_knight"), MakeKnightDefinition(), "knight");
    CheckCharacter(library.Character("enemy_skeleton"), MakeSkeletonDefinition(), "skeleton");
    const AIDefinition hardcodedAi{};
    const AIDefinition& ai = *library.Character("enemy_skeleton").ai;
    Check(Near(ai.detectRange, hardcodedAi.detectRange) && Near(ai.attackEnterRange, hardcodedAi.attackEnterRange) &&
              Near(ai.attackExitRange, hardcodedAi.attackExitRange) &&
              Near(ai.attackCooldown, hardcodedAi.attackCooldown),
          "skeleton ai");
    CheckLevelEq(library.Level(1, 1), GetLevelDefinition(1, 1), "world1_level1");
    CheckLevelEq(library.Level(1, 2), GetLevelDefinition(1, 2), "world1_level2");
}
