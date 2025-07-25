/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "BubbleProjectile.h"

#include "Game/Camera.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Projectiles/ExpGenSpawnableMemberInfo.h"
#include "Sim/Projectiles/ProjectileHandler.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CBubbleProjectile, CProjectile, )

CR_REG_METADATA(CBubbleProjectile, (
	CR_MEMBER_BEGINFLAG(CM_Config),
		CR_MEMBER(ttl),
		CR_MEMBER(alpha),
		CR_MEMBER(startSize),
		CR_MEMBER(sizeExpansion),
	CR_MEMBER_ENDFLAG(CM_Config),
	CR_MEMBER(size)
))


CBubbleProjectile::CBubbleProjectile()
	: ttl(0)
	, alpha(0.0f)
	, size(0.0f)
	, startSize(0.0f)
	, sizeExpansion(0.0f)
{ }

CBubbleProjectile::CBubbleProjectile(
	CUnit* owner,
	float3 pos,
	float3 speed,
	int ttl,
	float startSize,
	float sizeExpansion,
	float alpha
):
	CProjectile(pos, speed, owner, false, false, false),
	ttl(ttl),
	alpha(alpha),
	size(startSize * 0.4f),
	startSize(startSize),
	sizeExpansion(sizeExpansion)
{
	checkCol = false;
}


void CBubbleProjectile::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	pos += speed;
	--ttl;
	size += sizeExpansion;
	if (size < startSize) {
		size += (startSize - size) * 0.2f;
	}
	drawRadius=size;

	if (pos.y > (-size * 0.7f)) {
		pos.y = -size * 0.7f;
		alpha -= 0.03f;
	}
	if (ttl < 0) {
		alpha -= 0.03f;
	}
	if (alpha < 0) {
		alpha = 0;
		deleteMe = true;
	}
}

void CBubbleProjectile::Draw()
{
	RECOIL_DETAILED_TRACY_ZONE;
	unsigned char col[4];
	col[0] = (unsigned char)(255 * alpha);
	col[1] = (unsigned char)(255 * alpha);
	col[2] = (unsigned char)(255 * alpha);
	col[3] = (unsigned char)(255 * alpha);

	const float interSize = size + sizeExpansion * globalRendering->timeOffset;

	const auto* bt = projectileDrawer->bubbletex;
	AddEffectsQuad<0>(
		bt->pageNum,
		{ drawPos - camera->GetRight() * interSize - camera->GetUp() * interSize, bt->xstart, bt->ystart, col },
		{ drawPos + camera->GetRight() * interSize - camera->GetUp() * interSize, bt->xend,   bt->ystart, col },
		{ drawPos + camera->GetRight() * interSize + camera->GetUp() * interSize, bt->xend,   bt->yend,   col },
		{ drawPos - camera->GetRight() * interSize + camera->GetUp() * interSize, bt->xstart, bt->yend,   col }
	);
}

int CBubbleProjectile::GetProjectilesCount() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return 1;
}


bool CBubbleProjectile::GetMemberInfo(SExpGenSpawnableMemberInfo& memberInfo)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (CProjectile::GetMemberInfo(memberInfo))
		return true;

	CHECK_MEMBER_INFO_FLOAT(CBubbleProjectile, alpha        );
	CHECK_MEMBER_INFO_FLOAT(CBubbleProjectile, startSize    );
	CHECK_MEMBER_INFO_FLOAT(CBubbleProjectile, sizeExpansion);
	CHECK_MEMBER_INFO_INT  (CBubbleProjectile, ttl          );

	return false;
}
