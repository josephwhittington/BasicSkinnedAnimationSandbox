#pragma once

struct WFLOAT2 { float x, y; };
struct WFLOAT3 { float x, y, z; };
struct WFLOAT4 { float x, y, z, w; };
struct MAT4X4 { float data[16]; };

struct joint
{
	MAT4X4 transform;
	int parent_index;
};

struct keyframe
{
	float time = 0.0f;
	std::vector<joint> joints;
	std::vector<XMMATRIX> xmjoints;
	std::vector<XMFLOAT4X4> fmjoints;
};

struct animation_clip
{
	float duration;
	std::vector<keyframe> keyframes;
};

struct Header
{
	int indexcount, vertexcount;
	int indexstart, vertexstart;
	int animation_start, animation_count;
	int joint_count, keyframe_count;
	char t_diffuse[256];
	char t_specular[256];
	char t_emissive[256];
	char t_normal[256];
};

struct AnimVertex
{
	WFLOAT3 position;
	WFLOAT3 normal;
	int joints[4];
	float weights[4];
	WFLOAT2 uv_diffuse;
};

struct DebugLineVertex
{
	WFLOAT3 position;
	WFLOAT3 color;
};