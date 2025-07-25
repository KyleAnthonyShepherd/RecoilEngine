/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "RepulseGfx.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Units/Unit.h"

#include "System/Misc/TracyDefs.h"

CR_BIND_DERIVED(CRepulseGfx, CProjectile, )

CR_REG_METADATA(CRepulseGfx,(
	CR_MEMBER(repulsed),
	CR_MEMBER(sqMaxOwnerDist),
	CR_MEMBER(age),
	CR_MEMBER(color),
	CR_MEMBER(vertexDists)
))


CRepulseGfx::CRepulseGfx(CUnit* owner, CProjectile* repulsee, float maxOwnerDist, const float4& gfxColor):
	CProjectile((repulsee != nullptr)? repulsee->pos: ZeroVector, (repulsee != nullptr)? repulsee->speed: ZeroVector, owner, false, false, false),
	repulsed(repulsee),
	age(0),
	sqMaxOwnerDist((maxOwnerDist * maxOwnerDist) + 100.0f),
	color(gfxColor)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (repulsed != nullptr)
		AddDeathDependence(repulsed, DEPENDENCE_REPULSE);

	checkCol = false;
	useAirLos = true;

	SetRadiusAndHeight(maxOwnerDist, 0.0f);

	for (int y = 0; y < 5; ++y) {
		float yp = (y / 4.0f - 0.5f);

		for (int x = 0; x < 5; ++x) {
			float xp = (x / 4.0f - 0.5f);
			float d = 0;

			if (xp != 0 || yp != 0)
				d = fastmath::apxsqrt2(xp * xp + yp * yp);

			vertexDists[y * 5 + x] = (1.0f - fastmath::cos(d * 2.0f)) * 20.0f;
		}
	}
}


void CRepulseGfx::DependentDied(CObject* o)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (o != repulsed)
		return;

	repulsed = nullptr;
	deleteMe = true;
}

void CRepulseGfx::Draw()
{
	RECOIL_DETAILED_TRACY_ZONE;
	const CUnit* owner = CProjectile::owner();

	if (owner == nullptr || repulsed == nullptr)
		return;

	const float3 zdir = (repulsed->pos - owner->pos).SafeANormalize();
	const float3 xdir = (zdir.cross(UpVector)).SafeANormalize();
	const float3 ydir = xdir.cross(zdir);

	float3 xdirDS;
	float3 ydirDS;

	pos = (repulsed->pos - zdir * 10.0f) + (repulsed->speed * globalRendering->timeOffset);

	float drawsize = 10.0f;
	float alpha = std::min(255.0f, age * 10.0f);

	unsigned char col[4] = {
		(unsigned char) (color.x * alpha       ),
		(unsigned char) (color.y * alpha       ),
		(unsigned char) (color.z * alpha       ),
		(unsigned char) (color.w * alpha * 0.2f),
	};
	constexpr unsigned char col2[4] = {0, 0, 0, 0};

	xdirDS = xdir * drawsize;
	ydirDS = ydir * drawsize;

	const auto* et = projectileDrawer->repulsetex;
	const float txo = et->xstart;
	const float tyo = et->ystart;
	const float txs = et->xend - et->xstart;
	const float tys = et->yend - et->ystart;

	static constexpr int loopCountY = 4;
	static constexpr int loopCountX = 4;

	for (int y = 0; y < loopCountY; ++y) { //! CAUTION: loop count must match EnlargeArrays above
		const float dy = y - 2.0f;
		const float ry = y * 0.25f;

		for (int x = 0; x < loopCountX; ++x) {
			const float dx = x - 2.00f;
			const float rx = x * 0.25f;
			AddEffectsQuad<0>(
				et->pageNum,
				{ pos + xdirDS * (dx + 0) + ydirDS * (dy + 0) + zdir * vertexDists[(y    ) * 5 + x    ],  txo + (ry        ) * txs, tyo + (rx        ) * tys,  col },
				{ pos + xdirDS * (dx + 0) + ydirDS * (dy + 1) + zdir * vertexDists[(y + 1) * 5 + x    ],  txo + (ry + 0.25f) * txs, tyo + (rx        ) * tys,  col },
				{ pos + xdirDS * (dx + 1) + ydirDS * (dy + 1) + zdir * vertexDists[(y + 1) * 5 + x + 1],  txo + (ry + 0.25f) * txs, tyo + (rx + 0.25f) * tys,  col },
				{ pos + xdirDS * (dx + 1) + ydirDS * (dy + 0) + zdir * vertexDists[(y    ) * 5 + x + 1],  txo + (ry        ) * txs, tyo + (rx + 0.25f) * tys,  col }
			);
		}
	}

	drawsize = 7.0f;
	alpha = std::min(10.0f, age / 2.0f);

	col[0] = (unsigned char) (color.x * alpha       );
	col[1] = (unsigned char) (color.y * alpha       );
	col[2] = (unsigned char) (color.z * alpha       );
	col[3] = (unsigned char) (color.w * alpha * 0.4f);

	const auto* ct = projectileDrawer->repulsegfxtex;
	const float tx = (ct->xend + ct->xstart) * 0.5f;
	const float ty = (ct->yend + ct->ystart) * 0.5f;

	xdirDS = xdir * drawsize;
	ydirDS = ydir * drawsize;

	AddEffectsQuad<0>(
		ct->pageNum,
		{ owner->pos + (-xdir + ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ owner->pos + ( xdir + ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ pos + xdirDS + ydirDS + zdir * vertexDists[6],  tx, ty, col },
		{ pos - xdirDS + ydirDS + zdir * vertexDists[6],  tx, ty, col }
	);

	AddEffectsQuad<0>(
		ct->pageNum,
		{ owner->pos + (-xdir - ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ owner->pos + ( xdir - ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ pos + xdirDS - ydirDS + zdir * vertexDists[6],  tx, ty, col },
		{ pos - xdirDS - ydirDS + zdir * vertexDists[6],  tx, ty, col }
	);

	AddEffectsQuad<0>(
		ct->pageNum,
		{ owner->pos + ( xdir - ydir) * drawsize * 0.2f,   tx, ty, col2 },
		{ owner->pos + ( xdir + ydir) * drawsize * 0.2f,   tx, ty, col2 },
		{ pos + xdirDS + ydirDS + zdir * vertexDists[6],  tx, ty, col },
		{ pos + xdirDS - ydirDS + zdir * vertexDists[6],  tx, ty, col }
	);

	AddEffectsQuad<0>(
		ct->pageNum,
		{ owner->pos + (-xdir - ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ owner->pos + (-xdir + ydir) * drawsize * 0.2f,  tx, ty, col2 },
		{ pos - xdirDS + ydirDS + zdir * vertexDists[6],  tx, ty, col },
		{ pos - xdirDS - ydirDS + zdir * vertexDists[6],  tx, ty, col }
	);
}

void CRepulseGfx::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	age += 1;
	deleteMe |= (repulsed != nullptr && owner() != nullptr && (repulsed->pos - owner()->pos).SqLength() > sqMaxOwnerDist);
}