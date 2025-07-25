/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "DirtProjectile.h"

#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "Map/Ground.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Projectiles/ExpGenSpawnableMemberInfo.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CDirtProjectile, CProjectile, )

CR_REG_METADATA(CDirtProjectile,
(
	CR_MEMBER_BEGINFLAG(CM_Config),
		CR_MEMBER(alpha),
		CR_MEMBER(alphaFalloff),
		CR_MEMBER(size),
		CR_MEMBER(sizeExpansion),
		CR_MEMBER(slowdown),
		CR_MEMBER(color),
		CR_IGNORED(texture),
	CR_MEMBER_ENDFLAG(CM_Config),
	CR_SERIALIZER(Serialize)
))


CDirtProjectile::CDirtProjectile(
	CUnit* owner,
	const float3& pos,
	const float3& speed,
	float ttl,
	float size,
	float expansion,
	float slowdown,
	const float3& color
):
	CProjectile(pos, speed, owner, false, false, false),

	alpha(255),
	size(size),
	sizeExpansion(expansion),
	slowdown(slowdown),
	color(color)
{
	checkCol = false;
	alphaFalloff = 255 / ttl;
	texture = projectileDrawer->randdotstex;
}

CDirtProjectile::CDirtProjectile() :
	alpha(255.0f),
	alphaFalloff(10.0f),
	size(10.0f),
	sizeExpansion(0.0f),
	slowdown(1.0f)
{
	checkCol = false;
	texture = projectileDrawer->randdotstex;
}

void CDirtProjectile::Serialize(creg::ISerializer* s)
{
	RECOIL_DETAILED_TRACY_ZONE;
	std::string name;
	if (s->IsWriting())
		name = projectileDrawer->textureAtlas->GetTextureName(texture);
	creg::GetType(name)->Serialize(s, &name);
	if (!s->IsWriting())
		texture = name.empty() ? projectileDrawer->randdotstex
				: projectileDrawer->textureAtlas->GetTexturePtr(name);
}

void CDirtProjectile::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	SetVelocityAndSpeed((speed * slowdown) + (UpVector * mygravity));
	SetPosition(pos + speed);

	alpha = std::max(alpha - alphaFalloff, 0.0f);
	size += sizeExpansion;

	deleteMe |= (CGround::GetApproximateHeight(pos.x, pos.z, false) - 40.0f > pos.y);
	deleteMe |= (alpha <= 0.0f);
}

void CDirtProjectile::Draw()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!IsValidTexture(texture))
		return;

	float partAbove = (pos.y / (size * camera->GetUp().y));

	if (partAbove < -1.0f)
		return;

	partAbove = std::min(partAbove, 1.0f);

	unsigned char col[4];
	col[0] = (unsigned char) (color.x * alpha);
	col[1] = (unsigned char) (color.y * alpha);
	col[2] = (unsigned char) (color.z * alpha);
	col[3] = (unsigned char) (alpha)/*- (globalRendering->timeOffset * alphaFalloff)*/;

	const float interSize = size + globalRendering->timeOffset * sizeExpansion;
	const float texx = texture->xstart + (texture->xend - texture->xstart) * ((1.0f - partAbove) * 0.5f);

	AddEffectsQuad<0>(
		texture->pageNum,
		{ drawPos - camera->GetRight() * interSize - camera->GetUp() * interSize * partAbove, texx,          texture->ystart, col },
		{ drawPos - camera->GetRight() * interSize + camera->GetUp() * interSize,             texture->xend, texture->ystart, col },
		{ drawPos + camera->GetRight() * interSize + camera->GetUp() * interSize,             texture->xend, texture->yend,   col },
		{ drawPos + camera->GetRight() * interSize - camera->GetUp() * interSize * partAbove, texx,          texture->yend,   col }
	);
}

int CDirtProjectile::GetProjectilesCount() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return 1 * IsValidTexture(texture);
}


bool CDirtProjectile::GetMemberInfo(SExpGenSpawnableMemberInfo& memberInfo)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (CProjectile::GetMemberInfo(memberInfo))
		return true;

	CHECK_MEMBER_INFO_FLOAT (CDirtProjectile, alpha        );
	CHECK_MEMBER_INFO_FLOAT (CDirtProjectile, alphaFalloff );
	CHECK_MEMBER_INFO_FLOAT (CDirtProjectile, size         );
	CHECK_MEMBER_INFO_FLOAT (CDirtProjectile, sizeExpansion);
	CHECK_MEMBER_INFO_FLOAT (CDirtProjectile, slowdown     );
	CHECK_MEMBER_INFO_FLOAT3(CDirtProjectile, color        );
	CHECK_MEMBER_INFO_PTR   (CDirtProjectile, texture, projectileDrawer->textureAtlas->GetTexturePtr);

	return false;
}
