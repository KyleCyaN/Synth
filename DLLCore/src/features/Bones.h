#pragma once
#include <vector>
#include "ESP.h"

using BoneId = int;

struct BoneConnection {
	const char* from;
	const char* to;
};

#define BONE_NAME(id) KNOWN_BONE_NAMES[id]

void CollectAllBones(std::vector<BoneInfo>& outAllBones);
void DistributeBonesToPlayers(std::vector<PlayerInfo>& players, const std::vector<BoneInfo>& allBones);

inline extern const char* KNOWN_BONE_NAMES[] = {
	"Bip01_Head", "Bip01_L_UpperArm", "Bip01_R_UpperArm",
	"Bip01_L_Forearm", "Bip01_R_Forearm", "Bip01_Spine",
	"Bip01_Pelvis", "Bip01_L_Thigh", "Bip01_R_Thigh",
	"Bip01_L_Calf", "Bip01_R_Calf",
};

inline extern constexpr int KNOWN_BONE_COUNT = std::size(KNOWN_BONE_NAMES);

inline extern constexpr BoneId BONE_HEAD = 0;
inline extern constexpr BoneId BONE_L_UPPERARM = 1;
inline extern constexpr BoneId BONE_R_UPPERARM = 2;
inline extern constexpr BoneId BONE_L_FOREARM = 3;
inline extern constexpr BoneId BONE_R_FOREARM = 4;
inline extern constexpr BoneId BONE_SPINE = 5;
inline extern constexpr BoneId BONE_PELVIS = 6;
inline extern constexpr BoneId BONE_L_THIGH = 7;
inline extern constexpr BoneId BONE_R_THIGH = 8;
inline extern constexpr BoneId BONE_L_CALF = 9;
inline extern constexpr BoneId BONE_R_CALF = 10;


inline extern constexpr BoneConnection boneConnections[] = {
	{"Bip01_Head",       "Bip01_Spine"},
	{"Bip01_Spine",      "Bip01_Pelvis"},
	{"Bip01_Spine",      "Bip01_L_UpperArm"},
	{"Bip01_L_UpperArm", "Bip01_L_Forearm"},
	{"Bip01_Spine",      "Bip01_R_UpperArm"},
	{"Bip01_R_UpperArm", "Bip01_R_Forearm"},
	{"Bip01_Pelvis",     "Bip01_L_Thigh"},
	{"Bip01_L_Thigh",    "Bip01_L_Calf"},
	{"Bip01_Pelvis",     "Bip01_R_Thigh"},
	{"Bip01_R_Thigh",    "Bip01_R_Calf"}
};