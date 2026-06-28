#include "Bones.h"
#include "features.h"
#include "memory.h"
#include <windows.h>
#include <cmath>
#include <features/ESP.h>
#include "imgui.h"
#include "utils/SynthUtils.h"

extern float ViewMatrix[16];

void CollectAllBones(std::vector<BoneInfo> &outAllBones)
{
	outAllBones.clear();
	uintptr_t baseAddr = Memory::ResolveAddress(ENEMY_BONE_ENTITY_EXPR);
	uintptr_t basePtr = 0;
	if (!Memory::SafeReadPtr(baseAddr, basePtr) || baseAddr == 0) return;
	constexpr int MAX_WORLD_OBJECTS = 1024;
	for (int objIdx = 0; objIdx < MAX_WORLD_OBJECTS; objIdx++)
	{
		constexpr int WORLD_OBJ_STEP = 0x8;
		uintptr_t worldObjPtr = 0;
		uintptr_t worldObjAddr = basePtr + (objIdx * WORLD_OBJ_STEP);
		if (!Memory::SafeReadPtr(worldObjAddr, worldObjPtr) || worldObjPtr == 0) continue;
		uintptr_t boneBasePtr = 0;
		if (!Memory::SafeReadPtr(worldObjPtr + 0x108, boneBasePtr) || boneBasePtr == 0) continue;
		uintptr_t boneCollectionPtr = 0;
		if (!Memory::SafeReadPtr(boneBasePtr + 0x10, boneCollectionPtr) || boneCollectionPtr == 0) continue;
		uintptr_t boneListPtr = 0;
		if (!Memory::SafeReadPtr(boneCollectionPtr + 0x10, boneListPtr) || boneListPtr == 0) continue;
		for (int boneOffset = 0x80; boneOffset <= 0xE0; boneOffset += 0x8)
		{
			uintptr_t bonePtr = 0;
			uintptr_t boneAddr = boneListPtr + boneOffset;
			if (!Memory::SafeReadPtr(boneAddr, bonePtr) || bonePtr == 0) continue;
			uintptr_t nameBasePtr = 0;
			if (!Memory::SafeReadPtr(bonePtr + 0x18, nameBasePtr) || nameBasePtr == 0) continue;
			uintptr_t corBasePtr = 0;
			if (!Memory::SafeReadPtr(bonePtr + 0x38, corBasePtr) || corBasePtr == 0) continue;
			char boneType[64] = {0};
			bool gotName = false;
			if (Memory::SafeReadString(nameBasePtr + 0x38, boneType, sizeof(boneType)) && strlen(boneType) > 0)
				gotName = true;
			if (!gotName || !isalpha(static_cast<unsigned char>(boneType[0])))
			{
				uintptr_t strPtr = 0;
				if (Memory::SafeReadPtr(nameBasePtr + 0x38, strPtr) && strPtr > 0x10000)
					if (Memory::SafeReadString(strPtr, boneType, sizeof(boneType)) && strlen(boneType) > 0)
						gotName = true;
			}
			if (!gotName || strlen(boneType) == 0) continue;
			bool isKnownBone = false;
			for (auto & j : KNOWN_BONE_NAMES)
			{
				if (strcmp(boneType, j) == 0)
				{
					isKnownBone = true;
					break;
				}
			}
			if (!isKnownBone) continue;
			BoneInfo bone;
			bone.nodeAddress = bonePtr;
			bone.boneName = boneType;
			if (!Memory::SafeReadFloat(corBasePtr + 0x40, bone.x) ||
				!Memory::SafeReadFloat(corBasePtr + 0x44, bone.y) || !
			    Memory::SafeReadFloat(corBasePtr + 0x48, bone.z)) continue;
			if (std::isnan(bone.x) || std::isinf(bone.x) || std::isnan(bone.y) || std::isinf(bone.y) ||
			    std::isnan(bone.z) || std::isinf(bone.z)) continue;
			if (fabsf(bone.x) > 50000.0f || fabsf(bone.y) > 50000.0f || fabsf(bone.z) > 50000.0f) continue;
			bone.isOnScreen = WorldToScreen(bone.x, bone.y, bone.z, bone.screenX, bone.screenY);
			outAllBones.push_back(bone);
		}
	}
}

void DistributeBonesToPlayers(std::vector<PlayerInfo> &players, const std::vector<BoneInfo> &allBones)
{
	if (allBones.empty() || players.empty()) return;
	struct BoneGroup
	{
		BoneInfo head;
		std::vector<BoneInfo> bones;
		int playerIndex = -1;
	};
	std::vector<BoneGroup> groups;
	for (const auto &bone: allBones)
		if (bone.boneName == "Bip01_Head")
		{
			BoneGroup g;
			g.head = bone;
			groups.push_back(g);
		}
	if (groups.empty()) return;
	for (const auto &bone: allBones)
	{
		if (bone.boneName == "Bip01_Head") continue;
		float minDist = 8.0f;
		int bestGroup = -1;
		for (size_t g = 0; g < groups.size(); g++)
		{
			float dist = CalculateDistance(groups[g].head.x, groups[g].head.y, groups[g].head.z, bone.x, bone.y,
			                                    bone.z);
			if (dist < minDist)
			{
				minDist = dist;
				bestGroup = (int) g;
			}
		}
		if (bestGroup >= 0) groups[bestGroup].bones.push_back(bone);
	}
	for (auto &group: groups) group.bones.push_back(group.head);
	for (auto &player: players)
	{
		player.bones.clear();
		float minDist = 8.0f;
		int bestGroup = -1;
		for (size_t g = 0; g < groups.size(); g++)
		{
			if (groups[g].playerIndex >= 0) continue;
			float dist = CalculateDistance(player.X, player.Y, player.Z, groups[g].head.x, groups[g].head.y,
			                                    groups[g].head.z);
			if (dist < minDist)
			{
				minDist = dist;
				bestGroup = static_cast<int>(g);
			}
		}
		if (bestGroup >= 0)
		{
			groups[bestGroup].playerIndex = (int) (&player - &players[0]);
			player.bones = groups[bestGroup].bones;
		}
	}
}
